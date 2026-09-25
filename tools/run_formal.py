#!/usr/bin/env python3
"""Run a declared exact-state/RA-fragment campaign. A cutoff is never PASS."""
from __future__ import annotations
import argparse,hashlib,json,os,subprocess,sys,time
from pathlib import Path
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
BASELINES=[
 ('spsc_n2_eight',['--model','spsc','--capacity','2','--producers','1','--consumers','1','--operations','8']),
 ('spsc_n4_six',['--model','spsc','--capacity','4','--producers','1','--consumers','1','--operations','6']),
 ('ncq_n2_eight',['--capacity','2','--producers','1','--consumers','1','--operations','8']),
 ('ncq_n4_2p2c',['--capacity','4','--producers','2','--consumers','2','--operations','1']),
 ('ncq_n4_2p1c',['--capacity','4','--producers','2','--consumers','1','--operations','2','--no-abort']),
 ('ncq_n4_1p2c',['--capacity','4','--producers','1','--consumers','2','--operations','4']),
 ('ncq_n8_2p2c',['--capacity','8','--producers','2','--consumers','2','--operations','1']),
]
MUTATIONS=['split_head','stale_head_retry','overwrite_current','tail_before_entry','post_lp_write','early_return']

def sources():
    names=[p for p in (ROOT/'formal').rglob('*') if p.is_file() and '__pycache__' not in p.parts]
    names += [ROOT/'tools/run_formal.py',ROOT/'tools/verify_formal_results.py',ROOT/'tools/check_formal_binding.py']
    return {str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(names)}

def run(out,extended=False):
    out.mkdir(parents=True,exist_ok=False)
    plan={'schema':'elite-formal-plan-v1','baseline':BASELINES,'negative':MUTATIONS,
          'states_cap':1200000,'time_budget_per_exploration_seconds':120,
          'scope':'finite data-plane models plus separate handoff graphs and boundary proofs',
          'extended':extended,'source_sha256':sources()}
    (out/'plan.json').write_text(json.dumps(plan,indent=2)+'\n');records=[]
    def execute(name,args,expected_exit,expected_status):
        start=time.monotonic();cmd=[sys.executable]+args;timed=False
        try:r=subprocess.run(cmd,cwd=ROOT,text=True,capture_output=True,timeout=180,env={**os.environ,'PYTHONDONTWRITEBYTECODE':'1'})
        except subprocess.TimeoutExpired as e:
            timed=True;r=subprocess.CompletedProcess(cmd,124,e.stdout or '',e.stderr or '')
        for field in ('stdout','stderr'):
            text=getattr(r,field)
            if isinstance(text,bytes):text=text.decode(errors='replace')
            (out/(name+'.'+field)).write_text(text)
        path=out/(name+'.json');result=json.loads(path.read_text()) if path.exists() else {}
        match=r.returncode==expected_exit and (expected_status is None or result.get('status')==expected_status)
        rec=dict(name=name,command=cmd,returncode=r.returncode,expected_exit=expected_exit,
                 expected_status=expected_status,expectation_met=match,outer_timeout=timed,
                 wall_seconds=time.monotonic()-start,result_file=path.name if path.exists() else None)
        if path.exists():rec['result_sha256']=hashlib.sha256(path.read_bytes()).hexdigest()
        records.append(rec);print(name,r.returncode,result.get('status'),flush=True)
        (out/'execution.json').write_text(json.dumps(records,indent=2)+'\n')
    for name,args in BASELINES:
        execute(name,['formal/explore.py',*args,'--out',str(out/(name+'.json')),
                     '--max-states','1200000','--seconds','120'],0,'PASS_WITHIN_SCOPE')
    for mutation in MUTATIONS:
        name='mutant_'+mutation
        execute(name,['formal/explore.py','--capacity','4','--producers','2','--consumers','2',
                     '--operations','1','--mutation',mutation,'--no-abort','--max-states','300000',
                     '--seconds','60','--out',str(out/(name+'.json'))],1,'FAIL_WITNESS')
    execute('spsc_publish_early',['formal/explore.py','--model','spsc','--capacity','2',
            '--producers','1','--consumers','1','--operations','1','--mutation','publish_early',
            '--out',str(out/'spsc_publish_early.json')],1,'FAIL_WITNESS')
    execute('cutoff_control',['formal/explore.py','--max-states','5','--out',str(out/'cutoff_control.json')],2,'INCOMPLETE')
    for name,script in [('handoff','formal/handoff.py'),('boundaries','formal/boundaries.py'),('targeted','formal/targeted.py')]:
        execute(name,[script,'--out',str(out/(name+'.json'))],0,None if name!='handoff' else 'PASS')
    if extended:
        execute('extended_n4_2p2c_eight',['formal/explore.py','--operations','8',
            '--max-states','1200000','--seconds','120','--out',str(out/'extended_n4_2p2c_eight.json')],0,'PASS_WITHIN_SCOPE')
    unchanged=sources()==plan['source_sha256'];success=unchanged and all(x['expectation_met'] for x in records)
    summary={'schema':'elite-formal-campaign-v1','status':'PASS_WITHIN_DECLARED_SCOPE' if success else 'INCOMPLETE_OR_FAILED',
             'source_unchanged':unchanged,'records':len(records),'record_names':[r['name'] for r in records],
             'baseline_population':len(BASELINES),'negative_expectations':len(MUTATIONS)+2,
             'external_spin_executed':False,'external_genmc_executed':False,'full_turn5_v1_qualification':False}
    (out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    manifest={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(out.iterdir()) if p.is_file()}
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    return 0 if success else 2
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',required=True);p.add_argument('--extended',action='store_true');a=p.parse_args()
    try:raise SystemExit(run(Path(a.out).resolve(),a.extended))
    except (ValueError,OSError) as e:print('FAIL:',e,file=sys.stderr);raise SystemExit(1)
