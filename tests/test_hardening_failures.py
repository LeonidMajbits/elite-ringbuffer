#!/usr/bin/env python3
"""Isolated failure-injection tests. Expected retained failures are not leaks
silently called clean: each failed owner is an exact child which is reaped before
its one reported POSIX name is removed.
"""
import ctypes
import json
import os
from pathlib import Path
import resource
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]


def no_core():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def run_probe(filename):
    child = subprocess.Popen([sys.executable, str(ROOT/'repros'/filename)],
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                             text=True, preexec_fn=no_core)
    try:
        out, err = child.communicate(timeout=20)
    except subprocess.TimeoutExpired:
        child.kill()
        out, err = child.communicate()
        raise AssertionError('probe timeout: ' + out + err)
    records = []
    for line in out.splitlines():
        try: records.append(json.loads(line))
        except json.JSONDecodeError: pass
    libc = ctypes.CDLL(None, use_errno=True)
    libc.shm_unlink.argtypes = [ctypes.c_char_p]
    libc.shm_unlink.restype = ctypes.c_int
    for row in records:
        if isinstance(row, dict) and 'name' in row:
            name = row['name']
            if not isinstance(name, str) or not name.startswith('/el-') or len(name) != 30:
                raise AssertionError('bad child resource name')
            rc = libc.shm_unlink(name.encode('ascii'))
            if rc and ctypes.get_errno() != 2: raise AssertionError('owned child cleanup failed')
    return child.returncode, records, out, err


class FailureRecoveryTests(unittest.TestCase):
    def test_unraisable_hook_never_receives_freed_self(self):
        rc, rows, out, err = run_probe('a09_unraisable_resurrection.py')
        self.assertEqual(rc, 0, out + err)
        state = next(r for r in rows if 'saved_objects' in r)
        self.assertTrue(state['trace_fired'])
        self.assertEqual(state['saved_objects'], 0)
        self.assertNotIn('AddressSanitizer', err)
        self.assertNotIn('runtime error:', err)

    def test_lost_acquisition_result_is_visible_and_retained(self):
        rc, rows, out, err = run_probe('a09_acquire_exception.py')
        self.assertEqual(rc, 0, out + err)
        self.assertTrue(next(r['public_busy_property'] for r in rows if 'public_busy_property' in r))
        attempts = {r['operation']: r for r in rows if 'operation' in r}
        self.assertEqual(attempts['reserve_again']['status'], 4)
        self.assertEqual(attempts['close']['status'], 12)
        self.assertNotIn('AddressSanitizer', err)
        self.assertNotIn('runtime error:', err)

if __name__ == '__main__':
    unittest.main(verbosity=2)
