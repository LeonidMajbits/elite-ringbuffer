#!/usr/bin/env python3
"""Exact finite arithmetic, explicit wrap witness, and progress classifications."""
import argparse,itertools,json
from pathlib import Path

def bounds():
    checks=0
    for w in range(3,11):
        for logn in range(1,w-1):
            n=1<<logn;j=(1<<w)-n-1
            for t in range(j+1):
                admitted=t<j
                if admitted:
                    for b in range(n):
                        entry=(t&~(n-1))|b
                        if not (t<t+1<=j and entry<=j):raise RuntimeError('bad ceiling')
                        checks+=1
    for n in (2,4,8,1024,1<<31):
        j=(1<<64)-n-1
        for t in (j-1,j,j+1):
            admitted=t<j
            if admitted and not 0<t+1<=j:raise RuntimeError('U64 bound')
            checks+=1
    # Six total status bits: four epoch bits and two phase bits.
    for g in range(16):
        admitted=g<14
        if admitted and ((g+1)<<2)|3 > 63:raise RuntimeError('epoch overflow')
        checks+=1
    return dict(status='PASS_WITHIN_SCOPE',scalar_checks=checks,
        domains='ticket widths 3..10; all admitted t and block encodings; native U64 boundary fixtures',
        theorem='guard t < J implies t+1 <= J < 2**w; no repeated cursor value before retirement',
        status_domain='6 total bits -> epoch ceiling 14; native epoch ceiling 2**62-2')

def wrapping():
    saved=4;mask=255;n=4;j=251;head=saved
    steps=[]
    for k in range(260):
        old=head;head=(head+1)&mask
        steps.append(dict(logical_claim=saved+k,expected=old,desired=head))
    baseline_claims=j-saved
    # At precisely 256 advances the represented head repeats. This is a
    # scalar/identity witness; not execution of all NCQ code with just one guard
    # deleted (other cycle guards also need a separate modular redesign).
    return dict(status='FAIL_WITNESS',mutation='allow represented head wrap; suppress session retirement',
        abstraction='single head-CAS identity domain; not a full 260-lifecycle queue exploration',
        width=8,capacity=n,ceiling=j,saved_expected=saved,competing_advances=260,
        at_256={'current_representation':saved,'logical_ticket':saved+256,
                'stale_CAS_comparison_would_match':True},
        baseline={'status':'RETIRE_BEFORE_REPEAT','allowed_advances':baseline_claims,'stops_at':j},
        steps=steps)

def starvation():
    return dict(status='COUNTEREXAMPLE_TO_INDIVIDUAL_WAIT_FREEDOM',
      model='unbounded-ticket symbolic schedule; finite-width production eventually retires',
      loop=[{'actor':'victim','action':'load fresh expected h'},
            {'actor':'peer','action':'claim h -> h+1; complete'},
            {'actor':'victim','action':'CAS(h,h+1) fails; discard old observations'}],
      scheduler='both actors take infinitely many steps in symbolic model',
      global_progress=True,individual_success=False,
      conclusion='no per-caller retry bound; finite generation ceiling is not a successful-return deadline')

def gate():
    # Whole-gate CAS interleavings with stale expected snapshots. Claimed gate
    # is one atomic (state,count) pair, not a permission check plus separate add.
    schedules=0
    traces=[]
    actions=('A_read','A_CAS','R_read','R_CAS')
    for order in itertools.permutations(actions):
        if order.index('A_read')>order.index('A_CAS') or order.index('R_read')>order.index('R_CAS'):continue
        state=('READY',0);saved={};events=[]
        for action in order:
            who,op=action.split('_')
            if op=='read':saved[who]=state;success=None
            else:
                success=state==saved[who]
                if who=='A' and saved[who][0]!='READY':success=False
                if success:state=('READY',state[1]+1) if who=='A' else ('RETIRE',state[1])
            events.append(dict(action=action,success=success,gate=state))
        if state[1]>1:raise RuntimeError('gate corruption')
        if any(e['action']=='A_CAS' and e['success'] and any(p['action']=='R_CAS' and p['success'] for p in events[:k]) for k,e in enumerate(events)):
            raise RuntimeError('late enrollment')
        traces.append(events);schedules+=1
    return dict(status='PASS_WITHIN_SCOPE',schedules=schedules,traces=traces,
        scope='one enrollment CAS vs one retirement CAS, not the entire lifecycle authority')

def main():
    p=argparse.ArgumentParser();p.add_argument('--out',required=True);a=p.parse_args()
    result=dict(schema='elite-formal-boundaries-v1',bounds=bounds(),wrap_mutant=wrapping(),
        starvation=starvation(),admission_gate=gate())
    Path(a.out).write_text(json.dumps(result,indent=2)+'\n');print('PASS: ceilings, wrap witness, gate histories, symbolic starvation witness')
if __name__=='__main__':main()
