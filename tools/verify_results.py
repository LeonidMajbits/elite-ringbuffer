#!/usr/bin/env python3
"""Independent bounded-memory membership verification and C-assisted RTT replay.
Does not invent uncertainty bounds, one-way times or a population-tail verdict.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def verify_membership(folder: Path, result: dict) -> None:
    count = result['messages']
    consumers = result['consumers']
    total_bytes = (count + 7) // 8
    paths = [folder / f'consumer-{i:02d}.bitmap' for i in range(consumers)]
    assert all(p.stat().st_size == total_bytes for p in paths), 'bitmap size'
    streams = [p.open('rb') for p in paths]
    union_stream = (folder / 'union.bitmap').open('rb')
    populations = [0] * consumers
    remaining = total_bytes
    try:
        while remaining:
            length = min(65536, remaining)
            union = 0
            for i, stream in enumerate(streams):
                raw = stream.read(length)
                assert len(raw) == length
                word = int.from_bytes(raw, 'little')
                assert not union & word, 'duplicate across consumers'
                union |= word
                populations[i] += word.bit_count()
            expected_bits = length * 8
            if remaining == length and count % 8:
                expected_bits -= 8 - count % 8
            assert union == (1 << expected_bits) - 1, 'missing/out-of-range membership'
            assert union_stream.read(length) == union.to_bytes(length, 'little'), 'saved union mismatch'
            remaining -= length
        assert union_stream.read(1) == b''
    finally:
        for stream in streams:
            stream.close()
        union_stream.close()
    expected = [r['count'] for r in result['workers_info'][result['producers']:]]
    assert populations == expected and sum(populations) == count, 'count reconciliation'
    assert all(r['count'] == count // result['producers'] for r in result['workers_info'][:result['producers']])


def replay(analyzer: Path, folder: Path, duration: str, starts: str, count: int, numer: int, denom: int) -> dict:
    args = [str(analyzer), str(folder / duration), '-' if starts == '-' else str(folder / starts), str(count), str(numer), str(denom), '0' if starts == '-' else '1']
    return json.loads(subprocess.check_output(args, text=True))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--analyzer', type=Path, default=Path('build/native/bench_analyze'))
    args = parser.parse_args()
    if not __debug__:
        raise RuntimeError('Verification must not disable assertions with Python -O')
    analyzer = args.analyzer.resolve()
    provenance_path = args.directory / 'provenance.json'
    if provenance_path.is_file():
        provenance = json.loads(provenance_path.read_text())
        assert provenance.get('status') == 'COMPLETED_NATIVE_CONDITIONS', 'incomplete orchestrator'
        assert all(run.get('exit_code') == 0 for run in provenance['runs']), 'failed condition'
    manifest = args.directory / 'RUN_MANIFEST.sha256'
    if manifest.is_file():
        root = args.directory.resolve()
        for line in manifest.read_text().splitlines():
            expected, relative = line.split('  ', 1)
            target = (root / relative).resolve()
            assert target.is_relative_to(root), 'manifest path escape'
            digest = hashlib.sha256()
            with target.open('rb') as stream:
                for chunk in iter(lambda: stream.read(1 << 20), b''):
                    digest.update(chunk)
            assert digest.hexdigest() == expected, f'checksum mismatch: {relative}'
    results = sorted(args.directory.rglob('result.json'))
    if not results:
        parser.error('no result.json: an incomplete run is not a passing run')
    for path in results:
        result = json.loads(path.read_text())
        assert result['status'] == 'PASS_WITHIN_SCOPE', path
        if result['schema'] == 'elite-bench-throughput-v1':
            verify_membership(path.parent, result)
        elif result['schema'] == 'elite-bench-rtt-v1':
            n, d = result['timebase_numer'], result['timebase_denom']
            actual = replay(analyzer, path.parent, 'rtt_ticks.u64le', 'start_ticks.u64le', result['roundtrips'], n, d)
            assert actual == result['latency'], 'latency ranks or ordering mismatch'
            for suffix in ('before', 'after'):
                expected = result[f'calibration_{suffix}']
                actual = replay(analyzer, path.parent, f'clock_{suffix}.u64le', '-', expected['samples'], n, d)
                assert actual == expected, 'calibration mismatch'
        else:
            raise ValueError(result['schema'])
        print('VERIFIED_RAW_WITHIN_SCOPE', path)
    # Any failure alongside success still blocks a blanket run pass.
    failures = list(args.directory.rglob('FAILED-*.txt'))
    assert not failures, f'Failed/incomplete records retained: {failures}'
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
