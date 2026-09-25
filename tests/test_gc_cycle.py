#!/usr/bin/env python3
"""Cyclic GC must retain exported bytes, then close native lifetimes exactly once.

These are real shared-memory tests using public wrapper objects. No cycle is
broken by a test after it becomes unreachable. Private state is used only in
assertions/observer counters, never to manufacture safe cleanup.
"""
import ctypes
import gc
import os
import unittest
import weakref

from elite_ringbuffer import EliteShm, cleanup_failures


def collect():
    for _ in range(4):
        gc.collect()


def gone(name):
    libc = ctypes.CDLL(None, use_errno=True)
    libc.shm_open.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_uint]
    libc.shm_open.restype = ctypes.c_int
    fd = libc.shm_open(name.encode('ascii'), os.O_RDWR, 0)
    if fd >= 0:
        os.close(fd)
        return False
    return ctypes.get_errno() == 2


def group(mode, role):
    shm = EliteShm(mode, capacity=4, max_payload=64)
    p, c = shm.producer(), shm.consumer()
    lease = p.reserve()
    with lease.buffer as v:
        v[:] = bytes(range(64))
    if role == 'read':
        lease.commit(64)
        lease = c.borrow()
    return shm, p, c, lease


class GCLifetimeTests(unittest.TestCase):
    def setUp(self):
        collect()
        self.baseline = cleanup_failures()

    def tearDown(self):
        collect()
        self.assertEqual(cleanup_failures(), self.baseline)

    def cycle(self, mode, role, where):
        shm, p, c, lease = group(mode, role)
        name = shm.name
        v = lease.buffer
        self.assertTrue(gc.is_tracked(v.obj))
        self.assertTrue(gc.is_tracked(lease._pin))
        if where == 'endpoint':
            (p if role == 'write' else c).cached_view = v
        elif where == 'lease':
            lease.loop = lease
            lease.cached_view = v
        else:
            shm.cached_view = v
        refs = [weakref.ref(x) for x in (shm, p, c, lease)]
        del shm, p, c, lease, v
        collect()
        self.assertTrue(all(r() is None for r in refs))
        self.assertTrue(gone(name))

    def test_external_slice_prevents_reclamation(self):
        for mode in ('spsc', 'ncq'):
            for role in ('write', 'read'):
                with self.subTest(mode=mode, role=role):
                    shm, p, c, lease = group(mode, role)
                    name = shm.name
                    v = lease.buffer
                    (p if role == 'write' else c).cached_view = v
                    part = v[::-2]
                    refs = [weakref.ref(x) for x in (shm, p, c)]
                    del shm, p, c, lease, v
                    collect()
                    self.assertFalse(gone(name))
                    self.assertTrue(any(r() is not None for r in refs))
                    self.assertEqual(bytes(part), bytes(range(64))[::-2])
                    part.release()
                    del part
                    collect()
                    self.assertTrue(all(r() is None for r in refs))
                    self.assertTrue(gone(name))

    def test_two_independent_exports(self):
        shm, p, c, lease = group('ncq', 'write')
        name = shm.name
        first = lease.buffer
        second = memoryview(first.obj)
        p.cached_view = first
        del lease, p, c, shm
        collect()
        self.assertFalse(gone(name))
        first.release()
        del first
        collect()
        self.assertFalse(gone(name))
        self.assertEqual(second[17], 17)
        second.release()
        del second
        collect()
        self.assertTrue(gone(name))

    def test_downstream_ctypes_alias_keeps_pin(self):
        shm, p, c, lease = group('spsc', 'write')
        name = shm.name
        view = lease.buffer
        array = (ctypes.c_ubyte * 64).from_buffer(view)
        p.cached_view = view
        del view, lease, p, c, shm
        collect()
        self.assertFalse(gone(name))
        self.assertEqual(array[31], 31)
        del array
        collect()
        self.assertTrue(gone(name))

    def test_finalizer_resurrects_live_view_without_revocation(self):
        saved = []
        class Holder:
            def __del__(self):
                saved.append(self.view)
        shm, p, c, lease = group('ncq', 'read')
        name = shm.name
        holder = Holder()
        holder.view = lease.buffer
        holder.loop = holder
        p.extra = holder
        c.extra = holder
        del lease, holder, p, c, shm
        collect()
        self.assertEqual(len(saved), 1)
        self.assertEqual(bytes(saved[0]), bytes(range(64)))
        self.assertFalse(gone(name))
        saved[0].release()
        saved.clear()
        collect()
        self.assertTrue(gone(name))

    def test_cross_object_cycles(self):
        a, p, c, wa = group('spsc', 'write')
        b, q, d, wb = group('ncq', 'write')
        names = (a.name, b.name)
        p.other = q
        q.other = p
        a.other = b
        b.other = a
        p.view = wa.buffer
        q.view = wb.buffer
        del a, b, p, q, c, d, wa, wb
        collect()
        self.assertTrue(all(gone(n) for n in names))

    def test_zero_length_export_still_pins(self):
        shm = EliteShm(capacity=2, max_payload=64)
        name = shm.name
        p, c = shm.producer(), shm.consumer()
        w = p.reserve()
        w.commit(0)
        r = c.borrow()
        root = r.buffer
        part = root[:]
        c.cached_view = root
        del w, r, root, p, c, shm
        collect()
        self.assertEqual(len(part), 0)
        self.assertFalse(gone(name))
        part.release()
        del part
        collect()
        self.assertTrue(gone(name))

    def test_repeated_cycles_no_failures(self):
        for i in range(64):
            self.cycle('spsc' if i & 1 else 'ncq',
                       'write' if i & 2 else 'read',
                       ('endpoint', 'lease', 'manager')[i % 3])


for _mode in ('spsc', 'ncq'):
    for _role in ('write', 'read'):
        for _where in ('endpoint', 'lease', 'manager'):
            def _test(self, mode=_mode, role=_role, where=_where):
                self.cycle(mode, role, where)
            setattr(GCLifetimeTests, f'test_cycle_{_mode}_{_role}_{_where}', _test)

if __name__ == '__main__':
    unittest.main(verbosity=2)
