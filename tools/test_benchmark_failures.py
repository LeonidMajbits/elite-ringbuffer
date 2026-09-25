#!/usr/bin/env python3
"""Bounded negative controls; signal only this test's owned children."""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


def rejected(command: list[str], log: Path, timeout: int = 30) -> None:
    with log.open('wb') as output:
        r = subprocess.run(command, stdout=output, stderr=subprocess.STDOUT,
                           check=False, timeout=timeout)
    if r.returncode == 0:
        raise RuntimeError(f'negative control unexpectedly accepted: {command}')


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build', type=Path, default=Path('build/native'))
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--smoke', type=Path, required=True)
    a = p.parse_args()
    a.out.mkdir(mode=0o700, exist_ok=False)
    b = a.build.resolve()
    checks = [
        ('zero-count', ['--count', '0']),
        ('bad-capacity', ['--capacity', '3']),
        ('too-few-slots', ['--capacity', '2', '--producers', '2', '--consumers', '2']),
        ('spsc-not-mpmc', ['--mode', 'spsc', '--producers', '2']),
        ('quota-remainder', ['--count', '17', '--producers', '2']),
    ]
    for name, args in checks:
        rejected([str(b / 'bench_throughput'), *args, '--out', str(a.out / name)],
                 a.out / f'{name}.log')
    # Short controller watchdog during an intentionally huge trial: no successful
    # result is possible without completing every request and exporting its trace.
    rejected([str(b / 'bench_latency'), '--count', '100000000', '--warmup', '1000000',
              '--timeout', '1', '--out', str(a.out / 'watchdog')], a.out / 'watchdog.log')
    if list((a.out / 'watchdog').rglob('result.json')):
        raise RuntimeError('watchdog emitted an apparent completed-trial result')
    verify = str(Path(__file__).with_name('verify_results.py').resolve())
    analyzer = str(b / 'bench_analyze')
    good = next((a.smoke / 'goodput-ncq-4').rglob('result.json')).parent
    copy = a.out / 'duplicate-bitmap'
    shutil.copytree(good, copy)
    # Duplicate an existing consumer-0 bit into consumer-1. This bypasses a
    # directory hash oracle deliberately, so membership checking itself must fail.
    data = (copy / 'consumer-00.bitmap').read_bytes()
    for index, byte in enumerate(data):
        if byte:
            bit = byte & -byte
            with (copy / 'consumer-01.bitmap').open('r+b') as f:
                f.seek(index); old = f.read(1)[0]; f.seek(index); f.write(bytes([old | bit]))
            break
    else:
        raise RuntimeError('control requires nonempty consumer zero')
    rejected([sys.executable, verify, str(copy), '--analyzer', analyzer], a.out / 'duplicate-detected.log')
    rtt = next((a.smoke / 'rtt-spsc').rglob('result.json')).parent
    copy = a.out / 'bad-rtt'
    shutil.copytree(rtt, copy)
    with (copy / 'rtt_ticks.u64le').open('r+b') as f:
        f.write(((1 << 64) - 1).to_bytes(8, 'little'))
    rejected([sys.executable, verify, str(copy), '--analyzer', analyzer], a.out / 'bad-rtt-detected.log')
    copy = a.out / 'incomplete-campaign'
    copy.mkdir(); (copy / 'provenance.json').write_text(json.dumps({'status': 'FAILED_OR_INCOMPLETE', 'runs': []}))
    rejected([sys.executable, verify, str(copy), '--analyzer', analyzer], a.out / 'incomplete-detected.log')
    rejected([sys.executable, '-O', verify, str(good), '--analyzer', analyzer], a.out / 'python-O-rejected.log')
    print('PASS negative controls=10 (5 CLI, watchdog, duplicate bitmap, invalid RTT, incomplete campaign, Python -O)')
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
