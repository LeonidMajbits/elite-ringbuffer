#!/usr/bin/env python3
"""Frozen owned-process V2/V8 chaos campaign. No third-party dependencies.

Every case gets a new application authority and process cohort. Failing records
remain on disk. COMPLETE is written only after every independent replay succeeds.
The signal target is the child process group we created, never a name search.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import signal
import subprocess
import sys
import time
sys.dont_write_bytecode=True
from verify_chaos_results import verify_file, sha, campaign_matrix, SOURCE_INPUTS


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build',default='build/native');p.add_argument('--out',required=True)
    p.add_argument('--trials',type=int,default=3);p.add_argument('--messages',type=int,default=10000)
    p.add_argument('--storm-messages',type=int,default=32000)
    p.add_argument('--smoke',action='store_true');p.add_argument('--timeout',type=float,default=75)
    a=p.parse_args()
    if not 1<=a.trials<=1000 or a.messages<2 or a.messages%2 or a.storm_messages<16 or a.storm_messages%16 or a.timeout<=0:
        p.error('trials 1..1000; messages positive/even; storm messages divisible by 16; timeout positive')
    root=Path(__file__).resolve().parents[1];build=Path(a.build).resolve(strict=True);out=Path(a.out).resolve();out.mkdir(parents=True,exist_ok=False)
    profile='smoke' if a.smoke else 'standard'
    cases=[]
    for c in campaign_matrix(profile,a.trials,a.messages,a.storm_messages):
        c={**c,'command':[str(build/c['binary']),*c['args']]};cases.append(c)
    inputs={name:sha(root/name) for name in SOURCE_INPUTS}
    binary_names=['chaos_multiprocess','chaos_multiprocess_hooks','chaos_resources']
    binary_hashes={name:sha(build/name) for name in binary_names}
    plan={'schema':'elite-chaos-campaign-v1','profile':profile,'trials':a.trials,
          'messages':a.messages,'storm_messages':a.storm_messages,'cases':cases,
          'source_hashes':inputs,'binaries':binary_hashes,
          'host':{'platform':platform.platform(),'machine':platform.machine(),'python':sys.version,
                  'cpu_count':os.cpu_count(),'affinity':sorted(os.sched_getaffinity(0)) if hasattr(os,'sched_getaffinity') else None},
          'timeout_seconds':a.timeout,'created_utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),
          'qualification':'FINITE_CHAOS_CASES_NOT_FULL_TURN5_POPULATION'}
    (out/'plan.json').write_text(json.dumps(plan,indent=2)+'\n');proofs=[]
    for case in cases:
        name=case['id'];trace=out/(name+'.jsonl');err=out/(name+'.stderr');start=time.monotonic_ns();timed_out=False
        with trace.open('wb') as f,err.open('wb') as e:
            proc=subprocess.Popen(case['command'],stdout=f,stderr=e,start_new_session=True)
            try:code=proc.wait(timeout=a.timeout)
            except subprocess.TimeoutExpired:
                timed_out=True
                # First ask only the owned coordinator to run its atexit cleanup.
                proc.terminate()
                try:code=proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    os.killpg(proc.pid,signal.SIGKILL);code=proc.wait()
        record={'command':case['command'],'returncode':code,'timed_out':timed_out,
                'wall_ns':time.monotonic_ns()-start,'trace_sha256':sha(trace),'stderr_sha256':sha(err)}
        (out/(name+'.run.json')).write_text(json.dumps(record,indent=2)+'\n')
        if code or timed_out:
            raise RuntimeError(f'{name}: failed/incomplete, exit={code}; retained {out}')
        proof=verify_file(trace)
        if err.stat().st_size:raise RuntimeError(f'{name}: unexpected stderr')
        (out/(name+'.verified.json')).write_text(json.dumps(proof,indent=2)+'\n')
        proofs.append(proof)
        print(f'{name}: PASS_WITHIN_SCOPE messages={proof.get("messages",0)}',flush=True)
    # Source/executable changes during the campaign invalidate completion.
    if inputs!={n:sha(root/n) for n in SOURCE_INPUTS} or binary_hashes!={n:sha(build/n) for n in binary_names}:
        raise RuntimeError('source/binary changed during execution')
    summary={'status':'COMPLETE','plan_sha256':sha(out/'plan.json'),'cases':[c['id'] for c in cases],
             'messages':sum(c.get('messages',0) for c in proofs)}
    (out/'COMPLETE.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary))
    return 0
if __name__=='__main__':
    try:raise SystemExit(main())
    except (ValueError,OSError,RuntimeError,KeyError) as e:
        print('FAIL/INCOMPLETE:',e,file=sys.stderr);raise SystemExit(1)
