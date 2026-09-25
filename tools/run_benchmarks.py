#!/usr/bin/env python3
"""Run reproducible native benchmark conditions; no third-party Python modules.

This is a development orchestrator, not a library runtime or sampling profiler.
Each C executable owns and watches its spawned workers. Raw evidence is retained.
"""
from __future__ import annotations
import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b''):
            digest.update(chunk)
    return digest.hexdigest()


def manifest(root: Path) -> None:
    paths = sorted(p for p in root.rglob('*') if p.is_file() and p.name != 'RUN_MANIFEST.sha256')
    with (root / 'RUN_MANIFEST.sha256').open('w', encoding='utf-8', newline='\n') as out:
        for path in paths:
            out.write(f'{sha256(path)}  {path.relative_to(root).as_posix()}\n')


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=Path('build/native'))
    parser.add_argument('--out', type=Path, required=True, help='New run directory; never overwritten')
    parser.add_argument('--count', type=int, default=100_000_000)
    parser.add_argument('--warmup', type=int, default=1_000_000)
    parser.add_argument('--trials', type=int, default=1)
    parser.add_argument('--timeout', type=int, default=600, help='C controller watchdog seconds per trial')
    parser.add_argument('--cpus', help='Linux CPU IDs, read back by each worker; round-robin on this list')
    parser.add_argument('--qos', choices=('default', 'user-initiated', 'background'), default='default')
    parser.add_argument('--affinity-tag', type=int, help='Darwin task-local hint, not a core selector')
    parser.add_argument('--only', choices=('all', 'latency', 'throughput'), default='all')
    parser.add_argument('--cache-control', action='store_true')
    args = parser.parse_args()
    if not 1 <= args.count <= 100_000_000 or not 0 <= args.warmup <= 100_000_000:
        parser.error('count/warmup outside native capacity')
    if args.only != 'latency' and (args.count % 8 or args.warmup % 8):
        parser.error('throughput sweep requires count and warmup divisible by 8')
    build = args.build.resolve()
    root = args.out.resolve()
    root.mkdir(mode=0o700, parents=False, exist_ok=False)
    source_root = Path(__file__).resolve().parent.parent
    source_files = list((source_root / 'src').glob('*.[ch]')) + list((source_root / 'include').glob('*.h')) + list((source_root / 'benchmarks').glob('*.[ch]')) + [source_root / 'Makefile']
    provenance = {
        'utc_started': dt.datetime.now(dt.timezone.utc).isoformat(),
        'platform': platform.platform(), 'python': sys.version,
        'requested': vars(args) | {'build': str(build), 'out': str(root)},
        'sources': {str(p.relative_to(source_root)): sha256(p) for p in sorted(source_files)},
        'binary_hashes': {}, 'runs': [],
        'qualification': 'finite native RTT/goodput observations; NOT complete Turn5 one-way qualification',
    }
    for name in ('bench_latency', 'bench_throughput', 'bench_cache', 'bench_analyze'):
        binary = build / name
        if binary.is_file():
            provenance['binary_hashes'][name] = sha256(binary)
    conditions: list[tuple[str, list[str]]] = []
    if args.only in ('all', 'latency'):
        for mode in ('spsc', 'ncq'):
            conditions.append((f'rtt-{mode}', ['bench_latency', '--mode', mode]))
    if args.only in ('all', 'throughput'):
        conditions.append(('goodput-spsc-1', ['bench_throughput', '--mode', 'spsc']))
        for n in (1, 2, 4, 8):
            conditions.append((f'goodput-ncq-{n}', ['bench_throughput', '--mode', 'ncq', '--producers', str(n), '--consumers', str(n)]))
    if args.cache_control:
        conditions.append(('cache-control-8', ['bench_cache', '--producers', '8', '--consumers', '8']))
    common = ['--count', str(args.count), '--warmup', str(args.warmup), '--trials', str(args.trials), '--timeout', str(args.timeout)]
    if args.cpus:
        common += ['--cpus', args.cpus]
    if args.qos != 'default':
        common += ['--qos', args.qos]
    if args.affinity_tag is not None:
        common += ['--affinity-tag', str(args.affinity_tag)]
    failed = False
    try:
        for name, command in conditions:
            executable = build / command[0]
            if not executable.is_file():
                raise FileNotFoundError(f'Build first: {executable}')
            argv = [str(executable), *command[1:], *common, '--out', str(root / name)]
            print('RUN', name, flush=True)
            started = dt.datetime.now(dt.timezone.utc).isoformat()
            with (root / f'{name}.log').open('wb') as log:
                # The C controller enforces its own owned-child watchdog.
                # No shell, broad process killer, or external payload transport.
                proc = subprocess.run(argv, stdout=log, stderr=subprocess.STDOUT, check=False)
            provenance['runs'].append({'name': name, 'argv': argv, 'utc_started': started, 'exit_code': proc.returncode})
            (root / 'provenance.json').write_text(json.dumps(provenance, indent=2) + '\n', encoding='utf-8')
            if proc.returncode:
                failed = True
                break  # Keep the first failure; no best-of rerun.
    except (OSError, KeyboardInterrupt) as error:
        provenance['orchestrator_error'] = repr(error)
        failed = True
    finally:
        provenance['utc_finished'] = dt.datetime.now(dt.timezone.utc).isoformat()
        provenance['status'] = 'FAILED_OR_INCOMPLETE' if failed else 'COMPLETED_NATIVE_CONDITIONS'
        (root / 'provenance.json').write_text(json.dumps(provenance, indent=2) + '\n', encoding='utf-8')
        manifest(root)
    return 1 if failed else 0

if __name__ == '__main__':
    raise SystemExit(main())
