#!/usr/bin/env python3
"""Mutate an actual v2 receipt and require independent CLI rejection."""
from __future__ import annotations
import argparse
import copy
import json
import math
from pathlib import Path
import subprocess
import sys


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--receipt',required=True,type=Path)
    p.add_argument('--out',required=True,type=Path)
    a=p.parse_args();a.out.mkdir(exist_ok=False)
    d=json.loads(a.receipt.read_text());verifier=Path(__file__).with_name('verify_cache_provenance.py')
    mutations=[
        ('delta',lambda x:x.update(delta_ticks=x['delta_ticks']+1)),
        ('duration',lambda x:x.update(elapsed_ns=x['elapsed_ns']*2)),
        ('rate',lambda x:x.update(operations_per_second=x['operations_per_second']*2)),
        ('workers',lambda x:x.update(workers=x['workers']+1)),
        ('lost_rmw',lambda x:x['per_worker'][0].update(final_counter_value=x['iterations_per_worker']-1)),
        ('extra_rmw',lambda x:x['per_worker'][0].update(final_counter_value=x['iterations_per_worker']+1)),
        ('end',lambda x:x.update(end=x['end']+1)),
        ('denom_zero',lambda x:x.update(timebase_denom=0)),
        ('source_identity',lambda x:x['build'].update(source_sha256='0'*64)),
        ('invalid_pmc',lambda x:x['per_worker'][0]['pmc']['groups'][0].update(status='INVALID_READING')),
        ('false_coherence',lambda x:x['metrics'].update(coherence_invalidations=11)),
        ('float_counter',lambda x:x['per_worker'][0].update(final_counter_value=float(x['iterations_per_worker']))),
        ('boolean_counter',lambda x:x['per_worker'][0].update(final_counter_value=True)),
        ('bad_worker_delta',lambda x:x['per_worker'][0].update(worker_delta_ticks=0)),
        ('fake_pinning',lambda x:x['per_worker'][0]['placement'].update(p_core_pinning='VERIFIED')),
        ('nan_duration',lambda x:x.update(elapsed_ns=math.nan)),
        ('outside_two_ulp',lambda x:x.update(elapsed_ns=math.nextafter(math.nextafter(math.nextafter(float(x['elapsed_ns']),math.inf),math.inf),math.inf))),
    ]
    results=[]
    for name,mutate in mutations:
        x=copy.deepcopy(d);mutate(x);path=a.out/(name+'.json');path.write_text(json.dumps(x)+'\n')
        run=subprocess.run([sys.executable,'-O',str(verifier),str(path)],capture_output=True,text=True)
        results.append(dict(case=name,exit_code=run.returncode,stderr=run.stderr,expected=1))
    path=a.out/'duplicate_key.json';path.write_text('{"schema":"elite-cache-control-v2","schema":null}')
    run=subprocess.run([sys.executable,'-O',str(verifier),str(path)],capture_output=True,text=True)
    results.append(dict(case='duplicate_key',exit_code=run.returncode,stderr=run.stderr,expected=1))
    ok=all(x['exit_code']==1 for x in results)
    (a.out/'NEGATIVE_RESULTS.json').write_text(json.dumps(dict(status='PASS_EXPECTED_REJECTIONS' if ok else 'FAIL',tests=results),indent=2)+'\n')
    print(f'{"PASS" if ok else "FAIL"}: {len(results)} deliberately invalid receipts; Python -O')
    return 0 if ok else 1
if __name__=='__main__':raise SystemExit(main())
