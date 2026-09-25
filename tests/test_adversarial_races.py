#!/usr/bin/env python3
"""Regression gates for pending exports, callback failures, and reentrancy."""
import ctypes
import gc
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import unittest

import _elite_buffer
from elite_ringbuffer import EliteShm, cleanup_failures

ROOT = Path(__file__).resolve().parents[1]


class Owner:
    def __init__(self):
        self.data = (ctypes.c_ubyte * 64)(*range(64))
        self.drop_count = 0
        self.assertion = lambda: None
    def _assert_live(self):
        return self.assertion()
    def _drop_view_pin(self):
        self.drop_count += 1
    def _exporter_cleanup_failed(self):
        raise AssertionError('unexpected cleanup failure')
    def exporter(self, readonly=False):
        return _elite_buffer.create(ctypes.addressof(self.data), 64, readonly, self)


class ExportTransactionTests(unittest.TestCase):
    def test_original_four_race_schedules(self):
        with tempfile.TemporaryDirectory() as tmp:
            build = Path(os.environ['ELITE_LIBRARY']).resolve().parent
            cmd = [sys.executable, str(ROOT/'repros/run_export_races.py'),
                   '--root', str(ROOT), '--build', str(build), '--out', tmp+'/results',
                   '--repeats', '3']
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            records = json.loads(Path(tmp, 'results/results.json').read_text())
            self.assertEqual(len(records), 12)
            self.assertTrue(all(x['safe_block_observed'] and x['returncode'] == 0 for x in records))

    def test_callback_exception_rolls_back_pending_count(self):
        owner = Owner()
        ex = owner.exporter()
        for typ in (BufferError, RuntimeError, MemoryError, KeyboardInterrupt):
            def fail(typ=typ):
                self.assertEqual(ex.exports, 1)
                raise typ('injected callback failure')
            owner.assertion = fail
            with self.assertRaises(typ):
                memoryview(ex)
            self.assertEqual(ex.exports, 0)
            self.assertFalse(ex.closed)
        owner.assertion = lambda: None
        with memoryview(ex) as view:
            self.assertEqual(view[3], 3)
        ex.close()
        ex.close()
        self.assertEqual(owner.drop_count, 1)

    def test_fillinfo_readonly_failure_rolls_back(self):
        owner = Owner()
        ex = owner.exporter(readonly=True)
        for _ in range(100):
            with self.assertRaises((TypeError, BufferError)):
                ctypes.c_ubyte.from_buffer(ex)
            self.assertEqual(ex.exports, 0)
        with memoryview(ex) as view:
            self.assertTrue(view.readonly)
        ex.close()
        self.assertEqual(owner.drop_count, 1)

    def test_callback_failure_completes_requested_finalization(self):
        owner = Owner()
        ex = owner.exporter()
        finalize = ctypes.pythonapi.PyObject_CallFinalizer
        finalize.argtypes = [ctypes.py_object]
        finalize.restype = None
        def request_then_fail():
            finalize(ex)
            self.assertEqual(ex.exports, 1)
            self.assertFalse(ex.closed)
            raise RuntimeError('original export error')
        owner.assertion = request_then_fail
        with self.assertRaisesRegex(RuntimeError, 'original export error'):
            memoryview(ex)
        self.assertTrue(ex.closed)
        self.assertEqual(ex.exports, 0)
        self.assertEqual(owner.drop_count, 1)

    def test_fillinfo_failure_completes_requested_finalization(self):
        owner = Owner()
        ex = owner.exporter(readonly=True)
        finalize = ctypes.pythonapi.PyObject_CallFinalizer
        finalize.argtypes = [ctypes.py_object]
        finalize.restype = None
        owner.assertion = lambda: finalize(ex)
        with self.assertRaises((TypeError, BufferError)):
            ctypes.c_ubyte.from_buffer(ex)
        self.assertTrue(ex.closed)
        self.assertEqual(ex.exports, 0)
        self.assertEqual(owner.drop_count, 1)

    def test_successful_export_defers_requested_finalization(self):
        owner = Owner()
        ex = owner.exporter()
        finalize = ctypes.pythonapi.PyObject_CallFinalizer
        finalize.argtypes = [ctypes.py_object]
        finalize.restype = None
        owner.assertion = lambda: finalize(ex)
        view = memoryview(ex)
        self.assertFalse(ex.closed)
        self.assertEqual(ex.exports, 1)
        self.assertEqual(view[1], 1)
        view.release()
        self.assertTrue(ex.closed)
        self.assertEqual(owner.drop_count, 1)

    def test_reentrant_close_rejected(self):
        owner = Owner()
        ex = owner.exporter()
        def reenter():
            with self.assertRaises(BufferError): ex.close()
            gc.collect()
        owner.assertion = reenter
        with memoryview(ex) as view:
            self.assertEqual(view[7], 7)
        ex.close()
        self.assertEqual(owner.drop_count, 1)

    def test_callback_result_decref_is_inside_transaction(self):
        owner = Owner()
        ex = owner.exporter()
        attempts = []
        class Result:
            def __del__(self):
                try: ex.close()
                except BufferError: attempts.append('blocked')
        owner.assertion = Result
        with memoryview(ex): pass
        self.assertEqual(attempts, ['blocked'])
        ex.close()
        self.assertEqual(owner.drop_count, 1)

    def test_close_failure_can_retry_without_double_drop(self):
        owner = Owner()
        ex = owner.exporter()
        attempts = []
        original = owner._drop_view_pin
        def fail_once():
            if not attempts:
                attempts.append(1)
                raise RuntimeError('before drop')
            original()
        owner._drop_view_pin = fail_once
        with self.assertRaises(RuntimeError): ex.close()
        self.assertEqual(ex.exports, 0)
        self.assertFalse(ex.closed)
        ex.close()
        self.assertTrue(ex.closed)
        self.assertEqual(owner.drop_count, 1)

    def test_close_is_serialized_during_cleanup_callback(self):
        owner = Owner()
        ex = owner.exporter()
        original = owner._drop_view_pin
        def reenter():
            with self.assertRaises(BufferError): memoryview(ex)
            with self.assertRaises(BufferError): ex.close()
            original()
        owner._drop_view_pin = reenter
        ex.close()
        self.assertEqual(owner.drop_count, 1)

    def test_pending_export_blocks_abort_and_close(self):
        for mode in ('spsc', 'ncq'):
            for action in ('abort', 'exporter.close', 'endpoint.close'):
                with self.subTest(mode=mode, action=action):
                    with EliteShm(mode, capacity=4, max_payload=64) as shm:
                        p, c = shm.producer(), shm.consumer()
                        lease = p.reserve()
                        v = lease.buffer
                        ex = v.obj
                        v.release()
                        ready, done = threading.Event(), threading.Event()
                        outputs, errors = [], []
                        def worker():
                            fired = False
                            def trace(frame, event, arg):
                                nonlocal fired
                                if not fired and event == 'return' and frame.f_code.co_name == '_assert_live':
                                    fired = True
                                    sys.settrace(None)
                                    ready.set()
                                    if not done.wait(10): raise TimeoutError('schedule')
                                return trace
                            try:
                                sys.settrace(trace)
                                outputs.append(memoryview(ex))
                            except BaseException as exc: errors.append(repr(exc))
                            finally: sys.settrace(None)
                        t = threading.Thread(target=worker)
                        t.start()
                        try:
                            self.assertTrue(ready.wait(10))
                            with self.assertRaises((BufferError, RuntimeError)):
                                {'abort': lease.abort, 'exporter.close': ex.close,
                                 'endpoint.close': p.close}[action]()
                        finally:
                            done.set()
                            t.join(10)
                        self.assertFalse(t.is_alive())
                        self.assertFalse(errors, errors)
                        self.assertEqual(len(outputs), 1)
                        outputs.pop().release()
                        lease.abort()
                        p.close(); c.close()
        self.assertEqual(cleanup_failures(), ())

if __name__ == '__main__':
    unittest.main(verbosity=2)
