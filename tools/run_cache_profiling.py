#!/usr/bin/env python3
"""New-directory cache v2 campaign with receipts and owned-process watchdog.

This is a diagnostic runner. No privilege escalation or system tuning occurs.
A restricted/unavailable PMU is preserved; a malformed receipt fails the run.
"""
from __future__ import annotations
import argparse
import datetime as dt
from fractions import Fraction
import hashlib
import json
import os
from pathlib import Path
import platform
import signal
import statistics
import subprocess
import sys
from verify_cache_provenance import load, verify_path


def filehash(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()


def interrupted(signum, _frame):
    raise InterruptedError(f'signal {signum}')


def main():
    signal.signal(signal.SIGTERM, interrupted)
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--binary',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--workers',default='1,2,4,8')
    p.add_argument('--iterations',type=int,default=5000000)
    p.add_argument('--trials',type=int,default=5)
    p.add_argument('--modes',default='off,count')
    p.add_argument('--cpus')
    p.add_argument('--qos',choices=('default','user-initiated','background'),default='default')
    p.add_argument('--affinity-tag',type=int,default=0)
    p.add_argument('--virtual-counter',choices=('off','probe'),default='off')
    p.add_argument('--timeout',type=int,default=60,help='seconds per layout, native watchdog')
    a=p.parse_args();workers=[int(x) for x in a.workers.split(',')];modes=a.modes.split(',')
    if len(workers)!=len(set(workers)) or not workers or any(x not in (1,2,4,8,16) for x in workers):p.error('workers must be unique choices from 1,2,4,8,16')
    if len(modes)!=len(set(modes)) or not modes or any(x not in ('off','count') for x in modes):p.error('modes must be off,count or one of these')
    if not 1<=a.trials<=100 or not 1<=a.iterations<=100000000 or any(a.iterations%n for n in workers):p.error('invalid trials/iterations or iterations not divisible by workers')
    if not 1<=a.timeout<=3600:p.error('timeout outside 1..3600')
    binary=a.binary.resolve(strict=True);root=a.out.resolve();root.mkdir(mode=0o700,exist_ok=False)
    source=Path(__file__).resolve().parents[1]
    info={'schema':'elite-cache-sweep-v2','status':'INCOMPLETE','utc_started':dt.datetime.now(dt.timezone.utc).isoformat(),
          'platform':platform.platform(),'python':sys.version,'binary_sha256':filehash(binary),
          'argv':sys.argv,'runs':[],'interpretation':'counter control, not IPC; no inferred coherence invalidations'}
    info['cpu_count'] = os.cpu_count()
    if hasattr(os, 'sched_getaffinity'):
        info['allowed_cpus'] = sorted(os.sched_getaffinity(0))
    info['host_observations'] = {}
    for name in ('/proc/sys/kernel/perf_event_paranoid','/sys/fs/cgroup/cpu.max','/proc/cpuinfo'):
        try:info['host_observations'][name] = Path(name).read_text()[:32768]
        except OSError:pass
    if sys.platform == 'darwin':
        for name in ('hw.model','machdep.cpu.brand_string','hw.ncpu','hw.perflevel0.physicalcpu',
                     'hw.perflevel1.physicalcpu','hw.tbfrequency'):
            query=subprocess.run(['/usr/sbin/sysctl','-n',name],capture_output=True,text=True)
            info['host_observations'][name]={'exit_code':query.returncode,'stdout':query.stdout,'stderr':query.stderr}
    failed=False
    try:
        for mode in modes:
            for n in workers:
                directory=root/f'{mode}-{n}';command=[str(binary),'--producers',str(n),'--consumers',str(n),
                    '--count',str(a.iterations),'--trials',str(a.trials),'--warmup','0','--pmc',mode,
                    '--virtual-counter',a.virtual_counter,'--timeout',str(a.timeout),'--out',str(directory),
                    '--qos',a.qos]
                if a.affinity_tag:command+=['--affinity-tag',str(a.affinity_tag)]
                if a.cpus:command+=['--cpus',a.cpus]
                entry={'command':command,'directory':directory.name,'status':'STARTED'};info['runs'].append(entry)
                (root/'campaign.json').write_text(json.dumps(info,indent=2)+'\n')
                print('RUN',mode,n,flush=True)
                with (root/f'{mode}-{n}.log').open('wb') as log:
                    process=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
                    try:code=process.wait(timeout=2*a.trials*a.timeout+30)
                    except BaseException:
                        if process.poll() is None:
                            try:os.killpg(process.pid,signal.SIGKILL)
                            except ProcessLookupError:pass
                        process.wait();entry['status']='INTERRUPTED_OR_TIMEOUT';raise
                entry['exit_code']=code
                if code:raise RuntimeError(f'native benchmark failed: {code}')
                summaries=verify_path(directory,binary,source)
                (directory/'VERIFIED.json').write_text(json.dumps(summaries,indent=2)+'\n')
                paired=[]
                for i in range(a.trials):
                    d={s:load(directory/f'cache-{n}-workers-stride-{s}-trial-{i}.json') for s in (8,128)}
                    # Same n and operation quota: ratio is exact duration ratio.
                    duration={s:Fraction(d[s]['delta_ticks']*d[s]['timebase_numer'],d[s]['timebase_denom']) for s in d}
                    paired.append(float(duration[8]/duration[128]))
                entry.update(status='PASS_RECEIPTS',paired_isolated_over_packed_ratios=paired,
                             median_paired_ratio=statistics.median(paired),records=len(summaries),
                             pmc_statuses=sorted({x['pmc_status'] for x in summaries}),
                             coherence_causality='NOT_ESTABLISHED')
    except (Exception,KeyboardInterrupt) as exc:
        info['error']=repr(exc);failed=True
    finally:
        info['status']='FAILED_OR_INCOMPLETE' if failed else 'COMPLETE_VERIFIED_WITHIN_SCOPE'
        info['utc_finished']=dt.datetime.now(dt.timezone.utc).isoformat()
        (root/'campaign.json').write_text(json.dumps(info,indent=2)+'\n')
        entries=[p for p in root.rglob('*') if p.is_file() and p.name!='RUN_MANIFEST.sha256']
        (root/'RUN_MANIFEST.sha256').write_text(''.join(f'{filehash(p)}  {p.relative_to(root).as_posix()}\n' for p in sorted(entries)))
    print(info['status'],flush=True)
    return int(failed)
if __name__=='__main__':raise SystemExit(main())
