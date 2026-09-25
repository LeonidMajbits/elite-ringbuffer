#!/usr/bin/env python3
"""Run only owned exporter-race child processes and preserve exact outcomes.

Usage (from any directory):
  python run_export_races.py --root /path/to/elite-ringbuffer-1.0.0 \
      --build build/audit --out /path/to/new-results

WARNING: a vulnerable build is expected to crash each child. This runner never
signals unrelated processes or enumerates a shared-memory namespace. It unlinks
only the exact test name printed by a child after that child has been reaped.
Optional --exporter-dir selects an isolated candidate exporter; the native
library still comes from --build. Set sanitizer preload/options externally.
"""
from __future__ import annotations
import argparse
import ctypes
import json
import os
from pathlib import Path
import re
import resource
import subprocess
import sys


def no_core() -> None:
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--build', type=Path, default=Path('build/audit'))
    parser.add_argument('--exporter-dir', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--repeats', type=int, default=1)
    args = parser.parse_args()
    if not 1 <= args.repeats <= 100:
        parser.error('--repeats must be between 1 and 100')
    if sys.platform not in ('linux', 'darwin'):
        parser.error('only the project Linux/Darwin adapters are supported')
    root = args.root.resolve(strict=True)
    build = (root / args.build).resolve(strict=True)
    library = build / ('libelite_ringbuffer.dylib' if sys.platform == 'darwin' else 'libelite_ringbuffer.so')
    library.resolve(strict=True)
    exporter_dir = (args.exporter_dir or (build / 'python')).resolve(strict=True)
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    env = os.environ.copy()
    env['ELITE_LIBRARY'] = str(library)
    env['PYTHONPATH'] = os.pathsep.join((str(exporter_dir), str(root / 'bindings/python')))
    libc = ctypes.CDLL(None, use_errno=True)
    unlink = libc.shm_unlink
    unlink.argtypes = [ctypes.c_char_p]
    unlink.restype = ctypes.c_int
    repro = Path(__file__).resolve().with_name('export_race.py')
    records = []
    for mode in ('spsc', 'ncq'):
        for kind in ('write', 'read'):
            for repeat in range(args.repeats):
                command = [sys.executable, str(repro), mode, kind]
                timed_out = False
                process = subprocess.Popen(command, env=env, stdout=subprocess.PIPE,
                                           stderr=subprocess.STDOUT, text=True, preexec_fn=no_core)
                try:
                    output, _ = process.communicate(timeout=15)
                except subprocess.TimeoutExpired:
                    timed_out = True
                    process.kill()  # exact owned child only
                    output, _ = process.communicate()
                label = f'{mode}-{kind}-{repeat}'
                (out / (label + '.log')).write_text(output)
                names = re.findall(r'^SHM_NAME=(/el-[a-z2-7]{26})$', output, re.MULTILINE)
                cleanup = []
                for name in names:
                    ctypes.set_errno(0)
                    result = unlink(name.encode('ascii'))
                    error = ctypes.get_errno()
                    # ENOENT=2: a safely blocked/corrected child cleaned itself.
                    cleanup.append({'name': name, 'returncode': result, 'errno': error})
                records.append({'case': label, 'command': command, 'returncode': process.returncode,
                                'timed_out': timed_out, 'child_reaped': True, 'named_cleanup': cleanup,
                                'safe_block_observed': '"transfer_blocked": true' in output,
                                'invalid_export_observed': '"closed": true, "active": false, "exports": 1' in output})
    (out / 'results.json').write_text(json.dumps(records, indent=2) + '\n')
    print(json.dumps(records, indent=2))
    # A nonzero result means this diagnostic observed vulnerable or inconclusive
    # behavior. It never calls an arbitrary exit-1 result a proven vulnerability.
    return 0 if all(r['returncode'] == 0 and r['safe_block_observed'] and not r['timed_out'] for r in records) else 1


if __name__ == '__main__':
    raise SystemExit(main())
