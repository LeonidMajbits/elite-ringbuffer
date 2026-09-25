#!/usr/bin/env python3
"""Replay exact window RTT ranks, directional rates and cleanup from JSONL.

No native code is imported. This checks receipt consistency, not trusted hardware
attestation or an absence-of-ownership-bugs theorem. CANCELLED_CLEAN requires an
explicit CLI flag and is never a completed planned measurement.
"""
from __future__ import annotations
import argparse
from fractions import Fraction
import json
import math
from pathlib import Path
import sys
sys.dont_write_bytecode = True


def need(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def pairs(items):
    out = {}
    for k, v in items:
        need(k not in out, 'duplicate JSON key')
        out[k] = v
    return out


def reject_constant(value):
    raise ValueError('nonfinite JSON constant: ' + value)


def integer(v, name, low=0, high=(1 << 64)-1):
    need(type(v) is int and low <= v <= high, 'invalid integer: ' + name)
    return v


def verify(path: Path, allow_cancelled: bool = False) -> dict:
    need(path.stat().st_size <= 512*1024*1024, 'receipt exceeds 512 MiB replay budget')
    with path.open(encoding='utf-8') as f:
        def record(line):
            x = json.loads(line, object_pairs_hook=pairs, parse_constant=reject_constant)
            need(type(x) is dict, 'JSON object required')
            return x
        head = record(next(f))
        need(head.get('schema') == 'elite-live-demo-v1', 'wrong schema')
        need(head.get('version') == '1.1.0' and head.get('metric') == 'RTT', 'wrong version/metric')
        need(head.get('mode') in {'SPSC','NCQ-SC64'}, 'wrong mode')
        need(integer(head.get('bytes_per_direction'),'payload size') == 64, 'wrong payload size')
        p = integer(head.get('origin_pid'), 'origin_pid', 1)
        q = integer(head.get('responder_pid'), 'responder_pid', 1)
        need(p != q, 'not distinct processes')
        numer = integer(head.get('timebase_numer'), 'numer', 1, (1<<32)-1)
        denom = integer(head.get('timebase_denom'), 'denom', 1, (1<<32)-1)
        windows = integer(head.get('planned_windows'), 'windows', 1, 10000)
        batch = integer(head.get('messages_per_window'), 'batch', 1, 1000000)
        total = completed = 0
        previous_end = 0
        final = None
        partial = False
        for line in f:
            need(final is None, 'data after final receipt')
            x = record(line)
            if 'status' in x:
                final = x
                continue
            need(not partial and completed < windows, 'extra window after partial/planned end')
            need(integer(x.get('window'),'window identity') == completed, 'window identity')
            n = integer(x.get('roundtrips'), 'roundtrips', 1, batch)
            begin = integer(x.get('start_tick'), 'start_tick')
            end = integer(x.get('end_tick'), 'end_tick', begin+1)
            need(begin >= previous_end, 'window time decreases')
            data = x.get('samples')
            need(type(data) is list and len(data) == n, 'sample population')
            deltas = []
            cursor = begin
            for sample in data:
                need(type(sample) is list and len(sample) == 2, 'sample shape')
                start = integer(sample[0], 'sample start', cursor, end)
                dt = integer(sample[1], 'sample delta', 0, end-start)
                cursor = start+dt
                deltas.append(dt)
            need(cursor == end, 'window ends after last completed RTT')
            deltas.sort()
            for label, percent in [('p50_rtt_ns',50),('p99_rtt_ns',99)]:
                rank = (percent*n+99)//100-1
                expected = (deltas[rank]*numer+denom-1)//denom
                need(integer(x.get(label),label) == expected, label+' mismatch')
            expected = Fraction(2*n*10**9*denom, (end-begin)*numer)
            rate = x.get('directional_mps')
            need(type(rate) in (int,float) and math.isfinite(rate) and rate > 0, 'invalid rate')
            # Native expression uses several binary64 operations. Four ULPs
            # cover serialization/operation rounding, not physical uncertainty.
            need(abs(Fraction(rate)-expected) <= Fraction(4*math.ulp(float(expected))), 'rate mismatch')
            completed += 1
            total += n
            previous_end = end
            partial = n != batch
        need(final is not None, 'missing final receipt')
        status = final.get('status')
        need(status in {'COMPLETE','CANCELLED_CLEAN'}, 'invalid final status')
        if status == 'COMPLETE':
            need(completed == windows and total == windows*batch, 'incomplete planned count')
        else:
            need(allow_cancelled, 'cancelled run is not complete')
        need(integer(final.get('windows'),'final windows') == completed, 'window total')
        for key in ['roundtrips','child_checked']:
            need(integer(final.get(key),key) == total, 'final quota mismatch')
        need(integer(final.get('directional_records'),'directions') == 2*total, 'direction total')
        need(final.get('leases_ended') is True and final.get('objects_destroyed') is True, 'missing cleanup')
        return {'status':status,'windows':completed,'roundtrips':total,'directional_records':2*total}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('receipt', type=Path)
    p.add_argument('--allow-cancelled', action='store_true')
    a=p.parse_args()
    print('PASS_WITHIN_SCOPE:',json.dumps(verify(a.receipt,a.allow_cancelled),sort_keys=True))
    return 0

if __name__ == '__main__':
    try: sys.exit(main())
    except (OSError,ValueError,KeyError,TypeError,StopIteration,OverflowError) as e:
        print('FAIL:',e,file=sys.stderr);sys.exit(1)
