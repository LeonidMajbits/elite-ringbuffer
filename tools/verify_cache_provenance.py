#!/usr/bin/env python3
"""Reconstruct elite-cache-control-v2 from integer receipts, fail closed.

No third-party modules, no assert-based checks, no execution of receipt paths.
Arithmetic consistency is not hardware authentication or causal attribution.
Use --binary and --source-root to additionally bind external artifact bytes.
"""
from __future__ import annotations
import argparse
from fractions import Fraction
import hashlib
import json
import math
from pathlib import Path, PurePosixPath
import re
import sys

U64 = (1 << 64) - 1
MEASURED = {'OK', 'MULTIPLEXED'}
UNAVAILABLE = {'NOT_REQUESTED', 'RESTRICTED_PARANOID', 'RESTRICTED_ACCESS',
               'UNSUPPORTED_EVENT', 'UNSUPPORTED_PLATFORM', 'OPEN_ERROR',
               'START_ERROR', 'STOP_ERROR', 'READ_ERROR', 'NEVER_SCHEDULED',
               'INVALID_READING'}
EVENTS = ['cpu_cycles', 'instructions', 'l1d_read_access', 'l1d_read_miss',
          'llc_read_access', 'llc_read_miss']
CONFIGS = [0, 1, 0, 65536, 2, 65538]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def integer(v, name, low=0, high=U64):
    require(type(v) is int and low <= v <= high, f'{name}: invalid integer')
    return v


def eq(v, expected, name):
    require(type(v) is type(expected) and v == expected, f'{name}: expected {expected!r}')


def digest(v, name):
    require(isinstance(v, str) and re.fullmatch('[0-9a-f]{64}', v) is not None,
            f'{name}: not SHA-256')
    return v


def floating(v, exact: Fraction, name: str) -> None:
    require(type(v) in (int, float), f'{name}: not finite numeric')
    try:
        actual, target = float(v), float(exact)
    except (OverflowError, ValueError) as exc:
        raise ValueError(f'{name}: nonrepresentable') from exc
    require(math.isfinite(actual) and math.isfinite(target), f'{name}: nonfinite')
    # Compare against the exact rational, not an already rounded duration.
    tolerance = Fraction.from_float(math.ulp(target))
    require(abs(Fraction.from_float(actual) - exact) <= tolerance,
            f'{name}: differs by more than one binary64 ULP')


def optional_metric(v, exact, name):
    if exact is None:
        require(v is None, f'{name}: must be null when not measurable')
    else:
        floating(v, exact, name)


def no_duplicates(pairs):
    out = {}
    for k, v in pairs:
        require(k not in out, f'duplicate JSON key: {k}')
        out[k] = v
    return out


def load(path: Path):
    require(path.is_file() and path.stat().st_size <= 8 * 1024 * 1024,
            f'{path}: missing or oversized receipt')
    def bad_constant(value):
        raise ValueError(f'nonfinite JSON constant: {value}')
    return json.loads(path.read_text(encoding='utf-8'), object_pairs_hook=no_duplicates,
                      parse_constant=bad_constant)


def cpu_snapshot(x, name):
    require(isinstance(x, dict), f'{name}: not an object')
    integer(x['error'], name+'.error', 0, (1 << 31)-1)
    for field in ('user_us', 'system_us'):
        integer(x[field], name+'.'+field)
    for field in ('minor_faults', 'major_faults', 'voluntary_switches', 'involuntary_switches'):
        integer(x[field], name+'.'+field, -1, (1 << 63)-1)
    integer(x['instant_usage'], name+'.instant_usage', 0, (1 << 31)-1)
    integer(x['instant_usage_scale'], name+'.instant_usage_scale', 0, (1 << 31)-1)


def counter_frame(v, ids, name):
    require(isinstance(v, list) and len(v) == 7, f'{name}: bad group read size')
    for i, x in enumerate(v):
        integer(x, f'{name}[{i}]')
    eq(v[0], 2, name+'.nr')
    require(v[2] <= v[1], f'{name}: running > enabled')
    require(v[4] != v[6] and {v[4], v[6]} == set(ids), f'{name}: bad IDs')
    return {v[4]: v[3], v[6]: v[5]}


def verify_pmc(x, platform, mode, name):
    eq(x['requested'], mode == 'count', name+'.requested')
    eq(x['sampling'], False, name+'.sampling')
    eq(x['scope'], 'calling_thread_user_only_no_inherit', name+'.scope')
    integer(x['owner_thread_id'], name+'.tid', 1)
    if x['paranoid'] is not None:
        integer(x['paranoid'], name+'.paranoid', -(1 << 31), (1 << 31)-1)
    groups = x['groups']
    require(isinstance(groups, list) and len(groups) == 3, name+': need 3 groups')
    out, statuses = [], []
    for i, g in enumerate(groups):
        tag = f'{name}.groups[{i}]'
        eq(g['group_id'], i, tag+'.id')
        s = g['status']
        require(s in MEASURED | UNAVAILABLE, tag+': unfinished or unknown status')
        require(s != 'INVALID_READING', tag+': malformed native counter evidence')
        statuses.append(s)
        error = integer(g['os_error'], tag+'.os_error', 0, (1 << 31)-1)
        if mode == 'off':
            eq(s, 'NOT_REQUESTED', tag+'.status')
        else:
            require(s != 'NOT_REQUESTED', tag+': silently omitted requested group')
        if s == 'RESTRICTED_PARANOID':
            require(x['paranoid'] is not None and x['paranoid'] > 2 and error in (1, 13),
                    tag+': no evidence for restrictive-paranoid classification')
        elif s == 'RESTRICTED_ACCESS':
            require(error in (1, 13), tag+': no access-denial errno')
        elif s in {'OPEN_ERROR','START_ERROR','STOP_ERROR','READ_ERROR','UNSUPPORTED_EVENT'}:
            require(error > 0, tag+': omitted failure errno')
        events = g['events']
        require(isinstance(events, list) and len(events) == 2, tag+': need 2 events')
        ids = []
        for j, e in enumerate(events):
            eq(e['name'], EVENTS[2*i+j], tag+'.event_name')
            integer(e['type'], tag+'.type', 0, (1 << 32)-1)
            integer(e['config'], tag+'.config')
            ids.append(integer(e['id'], tag+'.event_id'))
            if platform == 'Linux':
                eq(e['type'], 0 if i == 0 else 3, tag+'.type')
                eq(e['config'], CONFIGS[2*i+j], tag+'.config')
        te = integer(g['time_enabled_ns'], tag+'.time_enabled_ns')
        tr = integer(g['time_running_ns'], tag+'.time_running_ns')
        if s in MEASURED | {'NEVER_SCHEDULED'}:
            require(platform == 'Linux', tag+': unsupported native event backend')
            eq(error, 0, tag+'.measured errno')
            require(ids[0] != ids[1] and min(ids) > 0, tag+': bad measured IDs')
            before = counter_frame(g['before'], ids, tag+'.before')
            after = counter_frame(g['after'], ids, tag+'.after')
            require(g['after'][1] >= g['before'][1] and g['after'][2] >= g['before'][2],
                    tag+': cumulative times decreased')
            eq(te, g['after'][1]-g['before'][1], tag+'.enabled delta')
            eq(tr, g['after'][2]-g['before'][2], tag+'.running delta')
            require(tr <= te, tag+': running delta > enabled delta')
            expected_status = 'NEVER_SCHEDULED' if tr == 0 else ('MULTIPLEXED' if tr < te else 'OK')
            eq(s, expected_status, tag+'.scheduling status')
            pair = []
            for e in events:
                raw = after[e['id']] - before[e['id']]
                require(raw >= 0, tag+': counter decreased')
                if tr:
                    eq(integer(e['raw_delta'], tag+'.raw_delta'), raw, tag+'.raw_delta')
                    value = Fraction(raw * te, tr)
                    floating(e['scaled_estimate'], value, tag+'.scaled_estimate')
                    pair.append(value)
                else:
                    require(raw == 0, tag+': nonzero count with no scheduled interval')
                    require(e['raw_delta'] is None and e['scaled_estimate'] is None,
                            tag+': unscheduled event is not zero evidence')
            out.append(pair if tr else None)
        else:
            # Bad read frames, when present, remain raw diagnostic evidence.
            # They must not feed any estimated event value.
            for e in events:
                require(e['raw_delta'] is None and e['scaled_estimate'] is None,
                        tag+': unavailable count must be null')
            require(te == tr == 0 or s == 'INVALID_READING', tag+': unavailable time claim')
            for frame in ('before', 'after'):
                require(g[frame] is None or (isinstance(g[frame], list) and len(g[frame]) == 7),
                        tag+': malformed diagnostic frame')
            out.append(None)
    return out, statuses


def timer_sample(s, name):
    require(isinstance(s, dict), name+': missing timer sample')
    a = integer(s['mach_before'], name+'.mach_before')
    b = integer(s['mach_after'], name+'.mach_after')
    v = integer(s['count'], name+'.count')
    require(b >= a, name+': backwards bracket')
    return a, v, b


def timer_pair(first, last, hz, scale, name):
    a0, v0, b0 = timer_sample(first, name+'.before')
    a1, v1, b1 = timer_sample(last, name+'.after')
    require(v1 >= v0 and a1 >= b0, name+': decreasing counter or overlapping pairs')
    measured = Fraction((v1-v0)*10**9, hz)
    # Two nominal ticks is a declared compatibility tolerance, not a proven
    # effective-resolution/latency uncertainty budget.
    tolerance = Fraction(2*10**9, hz) + 2*scale
    return (a1-b0)*scale-tolerance <= measured <= (b1-a0)*scale+tolerance


def verify_record(d, binary: Path | None = None, source_root: Path | None = None):
    eq(d['schema'], 'elite-cache-control-v2', 'schema')
    eq(d['status'], 'PASS_COUNTER_RECONCILIATION_ONLY', 'status')
    eq(d['ipc_measurement'], False, 'ipc_measurement')
    eq(d['sampling_interrupts_enabled'], False, 'sampling_interrupts_enabled')
    eq(d['interval'], 'scheduled_t0_to_latest_worker_rmw_end', 'interval')
    eq(d['warmup_iterations'], 0, 'warmup')
    eq(d['timestamp_ordering'], 'inherited_ordered_clock_boundaries', 'timestamp ordering label')
    n = integer(d['workers'], 'workers', 1, 16)
    iterations = integer(d['iterations_per_worker'], 'iterations_per_worker', 1, 100000000)
    operations = n*iterations
    eq(integer(d['total_operations'], 'total_operations'), operations, 'total_operations')
    stride = integer(d['counter_stride'], 'counter_stride')
    require(stride in (8, 128), 'unrecognized stride')
    eq(d['allocation_alignment'], 128, 'allocation_alignment')
    trial = integer(d['trial'], 'trial', 0, 99)
    eq(d['layout_order'], 0 if (trial % 2 == 0) == (stride == 8) else 1, 'layout_order')
    t0, end = integer(d['t0'], 't0'), integer(d['end'], 'end')
    require(end > t0, 'nonpositive or wrapped interval')
    eq(integer(d['delta_ticks'], 'delta_ticks', 1), end-t0, 'delta_ticks')
    num = integer(d['timebase_numer'], 'numer', 1, (1 << 32)-1)
    den = integer(d['timebase_denom'], 'denom', 1, (1 << 32)-1)
    scale = Fraction(num, den)
    elapsed = (end-t0)*scale
    floating(d['nominal_tick_ns'], scale, 'nominal_tick_ns')
    floating(d['elapsed_ns'], elapsed, 'elapsed_ns')
    floating(d['operations_per_second'], Fraction(operations*10**9, 1)/elapsed,
             'operations_per_second')
    platform = d['platform']
    require(platform in ('Linux', 'Darwin'), 'unsupported platform label')
    if platform == 'Linux':
        eq(d['clock'], 'CLOCK_MONOTONIC_RAW', 'clock')
        eq(d['timebase_source'], 'nanosecond_OS_clock', 'timebase source')
        eq(d['resolution_status'], 'clock_getres_NOT_effective_resolution', 'resolution status')
        require(num == den == 1, 'Linux RAW nanoseconds require 1:1')
        integer(d['reported_resolution_ns'], 'reported_resolution_ns', 1)
    else:
        eq(d['clock'], 'mach_absolute_time', 'clock')
        eq(d['timebase_source'], 'mach_timebase_info', 'timebase source')
        eq(d['resolution_status'], 'NOT_REPORTED_FOR_THIS_CLOCK', 'resolution status')
        require(d['reported_resolution_ns'] is None, 'no fabricated Mach resolution')
    mode = d['pmc_mode']
    require(mode in ('off', 'count'), 'unknown PMC mode')
    b = d['binary']
    bh = digest(b['sha256'], 'binary.sha256')
    integer(b['bytes'], 'binary.bytes', 1)
    eq(b['rehash_after_run'], True, 'binary.rehash_after_run')
    require(isinstance(b['path'], str) and b['path'], 'missing binary path')
    if binary is not None:
        require(binary.stat().st_size == b['bytes'] and hashlib.sha256(binary.read_bytes()).hexdigest() == bh,
                'external executable differs')
    build = d['build']
    for k in ('compiler_path', 'compiler_version', 'cflags', 'cppflags'):
        require(isinstance(build[k], str) and build[k], 'build.'+k+': missing')
    for k in ('ldflags','ldlibs'):
        require(isinstance(build[k], str), 'build.'+k+': missing')
    digest(build['compiler_sha256'], 'compiler_sha256')
    digest(build['source_sha256'], 'source_sha256')
    if build['git_commit'] is not None:
        require(isinstance(build['git_commit'],str) and re.fullmatch('[0-9a-f]{40,64}',build['git_commit']), 'bad git identity')
    sources = build['source_files']
    require(isinstance(sources,dict) and 1 <= len(sources) <= 1000, 'missing source identities')
    material = []
    for name, value in sorted(sources.items()):
        path = PurePosixPath(name)
        require(not path.is_absolute() and '..' not in path.parts and '\\' not in name and '\n' not in name,
                'unsafe source path')
        digest(value, 'source file hash')
        material.append(f'{value}  {name}\n')
        if source_root is not None:
            actual = (source_root / name).resolve(strict=True)
            require(actual.is_relative_to(source_root.resolve()), 'source symlink escape')
            require(hashlib.sha256(actual.read_bytes()).hexdigest() == value, 'external source differs: '+name)
    eq(hashlib.sha256(''.join(material).encode()).hexdigest(), build['source_sha256'], 'source snapshot digest')
    probe = d['virtual_timer_probe']
    eq(probe['kind'], 'SYSTEM_TIMER_NOT_CPU_CYCLES', 'virtual counter meaning')
    require(probe['status'] in ('NOT_REQUESTED','UNSUPPORTED_TARGET','PROBE_FAILED','PROBE_INVALID','PROBE_SUCCEEDED_TIMER_ONLY'), 'bad timer probe status')
    integer(probe['error'], 'probe.error', 0, (1 << 31)-1)
    integer(probe['signal'], 'probe.signal', 0, 255)
    hz = integer(probe['cntfrq_hz'], 'cntfrq_hz')
    integer(probe['sysctl_tbfrequency_hz'], 'sysctl_tbfrequency_hz')
    integer(probe['sysctl_error'], 'sysctl_error', 0, (1 << 31)-1)
    virtual = probe['status'] == 'PROBE_SUCCEEDED_TIMER_ONLY'
    correlation = None
    if virtual:
        require(platform == 'Darwin' and hz > 0, 'virtual timer not admitted')
        correlation = timer_pair(probe['first'], probe['last'], hz, scale, 'probe')
    workers = d['per_worker']
    require(isinstance(workers,list) and len(workers) == n, 'missing/extra workers')
    sums = [[Fraction(0), Fraction(0)] for _ in range(3)]
    valid = [True]*3
    statuses = []
    ends = []
    thread_ids = set()
    for i, w in enumerate(workers):
        tag = f'worker[{i}]'
        eq(w['worker_id'], i, tag+'.id')
        eq(w['counter_offset'], i*stride, tag+'.offset')
        eq(integer(w['final_counter_value'], tag+'.counter'), iterations, tag+'.counter')
        start = integer(w['start_tick'], tag+'.start')
        finish = integer(w['end_tick'], tag+'.end')
        window0 = integer(w['window_start_tick'], tag+'.window_start')
        window1 = integer(w['window_end_tick'], tag+'.window_end')
        require(t0 <= window0 <= start <= finish <= window1, tag+': invalid time order')
        eq(integer(w['worker_delta_ticks'], tag+'.delta'), finish-start, tag+'.delta')
        ends.append(finish)
        place = w['placement']
        cpu = integer(w['requested_cpu'], tag+'.requested_cpu', -1, 1023)
        verified = integer(w['verified_cpu'], tag+'.verified_cpu', -1, 1023)
        eq(place['requested_cpu'], cpu, tag+'.placement CPU')
        eq(place['verified_cpu_before'], verified, tag+'.verified CPU')
        eq(w['affinity_tag'], place['darwin_affinity_tag'], tag+'.tag')
        eq(w['qos'], place['qos_request'], tag+'.qos')
        eq(place['p_core_pinning'], 'NOT_ESTABLISHED', tag+'.P core claim')
        if platform == 'Linux' and cpu >= 0:
            eq(place['affinity_enforced'], True, tag+'.affinity enforcement')
            eq(place['affinity_error'], 0, tag+'.affinity errno')
            eq(verified, cpu, tag+'.verified CPU')
            eq(place['verified_cpu_after'], cpu, tag+'.final CPU')
        else:
            eq(place['affinity_enforced'], False, tag+'.affinity enforcement')
            eq(verified, -1, tag+'.unknown CPU')
        values, states = verify_pmc(w['pmc'], platform, mode, tag+'.pmc')
        tid = w['pmc']['owner_thread_id']
        require(tid not in thread_ids, tag+': duplicate worker thread identity')
        thread_ids.add(tid)
        statuses.extend(states)
        for g in range(3):
            if values[g] is None:
                valid[g] = False
            else:
                for j in range(2):
                    sums[g][j] += values[g][j]
        cpu_snapshot(w['cpu_before'], tag+'.cpu_before')
        cpu_snapshot(w['cpu_after'], tag+'.cpu_after')
        x, y = w['cpu_before'], w['cpu_after']
        usage = None
        if x['error'] == y['error'] == 0:
            require(y['user_us'] >= x['user_us'] and y['system_us'] >= x['system_us'], tag+': thread CPU accounting decreased')
        if x['error'] == y['error'] == 0 and window1 > window0 and y['user_us'] >= x['user_us'] and y['system_us'] >= x['system_us']:
            usage = Fraction((y['user_us']-x['user_us']+y['system_us']-x['system_us'])*100000, 1)/((window1-window0)*scale)
        optional_metric(w['cpu_utilization_percent'], usage, tag+'.CPU percent')
        if virtual:
            correlation = timer_pair(w['virtual_before'],w['virtual_after'],hz,scale,tag+'.timer') and correlation
        else:
            require(w['virtual_before'] is None and w['virtual_after'] is None, tag+': unadmitted timer')
    eq(end, max(ends), 'end != maximum worker completion')
    measured = sum(x in MEASURED for x in statuses)
    expected_pmc = ('NOT_REQUESTED' if mode == 'off' else
                    'COLLECTED' if measured == len(statuses) else
                    'PARTIAL' if measured else statuses[0] if len(set(statuses)) == 1 else 'UNAVAILABLE_MIXED')
    eq(d['pmc_status'], expected_pmc, 'overall PMC status')
    m = d['metrics']
    optional_metric(m['instructions_per_cycle'], sums[0][1]/sums[0][0] if valid[0] and sums[0][0] else None, 'instructions_per_cycle')
    for g, label in ((1,'l1d'),(2,'llc')):
        optional_metric(m[label+'_read_misses_per_million_rmw'], sums[g][1]*1000000/operations if valid[g] else None, label+' misses/million')
        optional_metric(m[label+'_read_miss_fraction'], sums[g][1]/sums[g][0] if valid[g] and sums[g][0] else None, label+' miss fraction')
    require(m['coherence_invalidations'] is None, 'generic LLC miss count is not invalidation traffic')
    eq(m['coherence_status'], 'UNSUPPORTED_GENERIC_EVENT', 'coherence status')
    eq(m['causal_attribution'], 'NOT_ESTABLISHED', 'causality')
    envelope = d['process_envelope']
    require(integer(envelope['start_tick'],'process.start') <= t0 and
            integer(envelope['end_tick'],'process.end') >= max(w['window_end_tick'] for w in workers), 'bad process envelope')
    cpu_snapshot(envelope['cpu_before'],'process.cpu_before')
    cpu_snapshot(envelope['cpu_after'],'process.cpu_after')
    for key in ('os_accounted_before','os_accounted_after'):
        for field in ('error','cycles','instructions'):
            integer(envelope[key][field], 'process.'+key+'.'+field)
    before, after = envelope['os_accounted_before'], envelope['os_accounted_after']
    process_ipc = None
    if before['error'] == after['error'] == 0:
        require(platform == 'Darwin', 'unimplemented process-PMC backend claimed available')
        require(after['cycles'] >= before['cycles'] and after['instructions'] >= before['instructions'],
                'OS-accounted process counts decreased')
        cycles = after['cycles'] - before['cycles']
        if cycles:
            process_ipc = float(Fraction(after['instructions']-before['instructions'], cycles))
    cpu0, cpu1 = envelope['cpu_before'], envelope['cpu_after']
    if cpu0['error'] == cpu1['error'] == 0:
        require(cpu1['user_us'] >= cpu0['user_us'] and cpu1['system_us'] >= cpu0['system_us'],
                'process CPU accounting decreased')
    return {'status':'PASS_ARITHMETIC_AND_COUNTER_RECONCILIATION',
            'trial':trial, 'stride':stride, 'workers':n, 'operations':operations,
            'elapsed_ns':float(elapsed), 'operations_per_second':float(Fraction(operations*10**9,1)/elapsed),
            'pmc_status':expected_pmc, 'virtual_timer_rate_compatible':correlation,
            'coherence_causality':'NOT_ESTABLISHED',
            'process_envelope_instructions_per_cycle':process_ipc,
            'sysctl_matches_cntfrq':(probe['sysctl_tbfrequency_hz'] == hz) if virtual and probe['sysctl_error'] == 0 else None,
            'binary_bytes_checked':binary is not None, 'source_bytes_checked':source_root is not None}


def verify_path(path: Path, binary=None, source_root=None):
    if not path.is_dir():
        return [verify_record(load(path), binary, source_root)]
    campaign = load(path/'cache-campaign.json')
    eq(campaign['schema'],'elite-cache-campaign-v2','campaign schema')
    eq(campaign['status'],'COMPLETE','campaign status')
    trials = integer(campaign['trials'],'campaign.trials',1,100)
    eq(campaign['expected_records'], 2*trials, 'campaign.expected_records')
    records = sorted(path.glob('cache-*-workers-stride-*-trial-*.json'))
    require(len(records) == 2*trials, 'missing/extra cache records')
    summaries, seen = [], set()
    for p in records:
        d = load(p)
        for name in ('workers','iterations_per_worker','pmc_mode'):
            eq(d[name], campaign[name], 'campaign.'+name)
        eq(d['binary']['sha256'],campaign['binary_sha256'],'campaign.binary')
        key = (d['trial'],d['counter_stride'])
        require(key not in seen and key[0] < trials, 'duplicate or unexpected trial')
        seen.add(key)
        summaries.append(verify_record(d,binary,source_root))
    require(seen == {(i,s) for i in range(trials) for s in (8,128)}, 'incomplete paired population')
    return summaries


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('paths', nargs='+', type=Path)
    p.add_argument('--binary', type=Path)
    p.add_argument('--source-root', type=Path)
    a = p.parse_args()
    results = []
    for path in a.paths:
        results.extend(verify_path(path,a.binary,a.source_root))
    print(json.dumps({'status':'PASS', 'records':len(results), 'results':results},indent=2,allow_nan=False))
    return 0

if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (ValueError, KeyError, TypeError, OSError, OverflowError, RecursionError) as exc:
        print('FAIL: '+str(exc),file=sys.stderr)
        raise SystemExit(1)
