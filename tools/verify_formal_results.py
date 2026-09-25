#!/usr/bin/env python3
"""Verify campaign integrity and replay structural witnesses.
--replay-baselines repeats exact-state baseline searches. Without it, exhaustion
is a retained tool result, not re-executed by this invocation.
"""
from __future__ import annotations
import argparse,dataclasses,hashlib,json,math,sys
from pathlib import Path
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'formal'))
from models.ncq import Model as NCQ
from models.spsc import Model as SPSC
from explore import run
from run_formal import BASELINES,MUTATIONS,sources
from handoff import spsc as handoff_spsc, ncq as handoff_ncq, ticket_atomicity
from boundaries import bounds,wrapping,starvation,gate
from targeted import scenario,stale_consumer

def load(path):
    def pairs(items):
        result={}
        for key,value in items:
            if key in result:raise ValueError("duplicate JSON key")
            result[key]=value
        return result
    def constant(value):raise ValueError("nonfinite JSON constant: "+value)
    result=json.loads(path.read_text(),object_pairs_hook=pairs,parse_constant=constant)
    def finite(x):
        if isinstance(x,float) and not math.isfinite(x):raise ValueError('nonfinite numeric overflow')
        if isinstance(x,dict):
            for v in x.values():finite(v)
        elif isinstance(x,list):
            for v in x:finite(v)
    finite(result)
    return result

def normalize(x):return json.loads(json.dumps(x))
def model_for(result):
    c=result['config'];typ=SPSC if result['model'].startswith('spsc') else NCQ
    return typ(c['capacity'],c.get('producers',1),c.get('consumers',1),
          c.get('reservations',c.get('reservations_per_producer',1)),c['mutation'],c['allow_abort'])
def replay_witness(result):
    m=model_for(result);w=result['witness'];s=m.initial()
    if normalize(dataclasses.asdict(s))!=w['initial']:raise ValueError('wrong initial state')
    found=None
    for i,item in enumerate(w['steps']):
        choices=[x for x in m.successors(s) if x[0]==item['event'] and normalize(dataclasses.asdict(x[1]))==item['state']]
        if len(choices)!=1:raise ValueError('witness is not a model transition')
        _,s,_,error=choices[0];error=error or m.invariant(s)
        if error:
            if i!=len(w['steps'])-1:raise ValueError('execution continued past failure')
            found=error[0]
    if found!=w['property']:raise ValueError('unconfirmed violation')
    return True

def expected_model(name):
    cases=dict(BASELINES)
    if name in cases:
        args=cases[name]
    elif name.startswith('mutant_'):
        return NCQ(4,2,2,1,name[len('mutant_'):],False)
    elif name=='spsc_publish_early':return SPSC(2,1,1,1,'publish_early',True)
    elif name=='cutoff_control':return NCQ()
    elif name=='extended_n4_2p2c_eight':return NCQ(4,2,2,8)
    else:return None
    def value(flag,default):return int(args[args.index(flag)+1]) if flag in args else default
    cls=SPSC if '--model' in args and args[args.index('--model')+1]=='spsc' else NCQ
    return cls(value('--capacity',4),value('--producers',2),value('--consumers',2),value('--operations',1),allow_abort='--no-abort' not in args)

def expected_aux(name):
    if name=='handoff':
        return dict(schema='elite-handoff-fragment-v1',status='PASS',
          scope='finite handoff skeletons, not full ISO-C11/RC11 model checking',
          results=[handoff_spsc(2,4),handoff_spsc(2,4,'relaxed'),handoff_spsc(2,4,'release','relaxed'),
                   handoff_spsc(2,5),handoff_ncq(),handoff_ncq('relaxed'),handoff_ncq('seq_cst','relaxed'),ticket_atomicity()])
    if name=='boundaries':return dict(schema='elite-formal-boundaries-v1',bounds=bounds(),wrap_mutant=wrapping(),starvation=starvation(),admission_gate=gate())
    if name=='targeted':return dict(schema='elite-formal-targeted-v1',results=[scenario(),scenario(True),stale_consumer()])
    raise ValueError('unknown auxiliary case')

def verify(directory,replay=False):
    manifest=load(directory/'manifest.json')
    actual={p.name for p in directory.iterdir() if p.is_file()}-{'manifest.json'}
    if actual!=set(manifest):raise ValueError('campaign inventory mismatch')
    for name,digest in manifest.items():
        if Path(name).name!=name or hashlib.sha256((directory/name).read_bytes()).hexdigest()!=digest:raise ValueError('campaign hash mismatch')
    plan=load(directory/'plan.json');execution=load(directory/'execution.json');summary=load(directory/'summary.json')
    if plan.get('schema')!='elite-formal-plan-v1' or summary.get('schema')!='elite-formal-campaign-v1':raise ValueError('unknown schema')
    if plan['baseline']!=normalize(BASELINES) or plan['negative']!=MUTATIONS:raise ValueError('plan differs from declared source configuration')
    if plan.get('source_sha256')!=sources():raise ValueError('incomplete or changed source identity set')
    if type(plan.get('extended')) is not bool or plan.get('states_cap')!=1200000 or plan.get('time_budget_per_exploration_seconds')!=120:raise ValueError('altered declared campaign bounds')
    if summary.get('source_unchanged') is not True or summary.get('external_spin_executed') is not False or summary.get('external_genmc_executed') is not False or summary.get('full_turn5_v1_qualification') is not False:raise ValueError('unsupported qualification assertion')
    if summary['status']!='PASS_WITHIN_DECLARED_SCOPE':raise ValueError('incomplete/failed campaign')
    expected=[x[0] for x in plan['baseline']]+['mutant_'+x for x in plan['negative']]+['spsc_publish_early','cutoff_control','handoff','boundaries','targeted']
    if plan['extended']:expected+=['extended_n4_2p2c_eight']
    if [x['name'] for x in execution]!=expected or summary['record_names']!=expected:raise ValueError('missing or duplicate expected run')
    if summary.get('records')!=len(expected) or summary.get('baseline_population')!=len(BASELINES) or summary.get('negative_expectations')!=len(MUTATIONS)+2:raise ValueError('population count mismatch')
    for name,digest in plan['source_sha256'].items():
        p=ROOT/name
        if not p.resolve().is_relative_to(ROOT) or hashlib.sha256(p.read_bytes()).hexdigest()!=digest:raise ValueError('source identity mismatch '+name)
    witnesses=0;baselines=0
    for ex in execution:
        wanted_exit=2 if ex['name']=='cutoff_control' else (1 if ex['name'].startswith('mutant_') or ex['name']=='spsc_publish_early' else 0)
        if ex['expected_exit']!=wanted_exit:raise ValueError('tampered expected exit')
        if ex['outer_timeout'] or not ex['expectation_met'] or ex['returncode']!=ex['expected_exit']:raise ValueError('run outcome mismatch')
        if ex['result_file']!=ex['name']+'.json':raise ValueError('wrong result pathname')
        r=load(directory/ex['result_file'])
        m=expected_model(ex['name'])
        if m is None:
            if r!=normalize(expected_aux(ex['name'])):raise ValueError('auxiliary replay disagrees')
        else:
            if r.get('schema')!='elite-formal-explorer-v1' or r.get('model')!=m.name or r.get('config')!=normalize(m.config):raise ValueError('result model/bounds mismatch')
            expected_status='INCOMPLETE' if wanted_exit==2 else ('FAIL_WITNESS' if wanted_exit==1 else 'PASS_WITHIN_SCOPE')
            if r.get('status')!=expected_status or ex.get('expected_status')!=expected_status:raise ValueError('wrong expected disposition')
            for key in ('states','transitions','terminals','max_shortest_depth','frontier_remaining','nonterminal_deadends'):
                if type(r.get(key)) is not int or r[key]<0:raise ValueError('invalid integer '+key)
            if r['states']<1 or r['terminals']>r['states']:raise ValueError('invalid state population')
            if wanted_exit==2 and (r.get('exhaustive') is not False or r.get('cutoff') is None):raise ValueError('cutoff control did not cut off')
        if hashlib.sha256((directory/ex['result_file']).read_bytes()).hexdigest()!=ex['result_sha256']:raise ValueError('result hash mismatch')
        if ex['expected_status'] is not None and r['status']!=ex['expected_status']:raise ValueError('unexpected checker disposition')
        if r.get('schema')=='elite-formal-explorer-v1':
            if r['status']=='PASS_WITHIN_SCOPE':
                if not r['exhaustive'] or r['cutoff'] is not None or r['frontier_remaining']!=0 or r['nonterminal_deadends']!=0 or r['no_progress_analysis']['cycle_exists']:raise ValueError('unexhausted result labeled pass')
                if replay:
                    new=run(model_for(r),plan['states_cap'],300)
                    for key in ('status','states','transitions','terminals','state_sequence_sha256','no_progress_analysis','nonterminal_deadends'):
                        if new[key]!=r[key]:raise ValueError('baseline replay disagreement: '+key)
                    baselines+=1
            elif r['status']=='FAIL_WITNESS':replay_witness(r);witnesses+=1
    return witnesses,baselines
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('directory');p.add_argument('--replay-baselines',action='store_true');a=p.parse_args()
    try:
        w,b=verify(Path(a.directory).resolve(),a.replay_baselines)
        print(f'PASS: campaign/source integrity; {w} structural witnesses and all auxiliary results replayed; {b} baseline searches re-executed')
    except (OSError,ValueError,KeyError,TypeError) as e:print('FAIL:',e,file=sys.stderr);raise SystemExit(1)
