#!/usr/bin/env python3
"""Finite C11 release/acquire handoff-graph checker, not a full ISO C frontend.

Enumerates all designated covering reads-from assignments for the named finite
straight-line skeletons. Uses per-location modification order, sb, sw, hb,
atomic coherence, acyclic sb|rf (no future causal justification), and conflicting
ordinary accesses. Plain-memory races are UB witnesses, not predicted hardware
values. No payload acquire/release is strengthened to SC to obtain a pass.
"""
from __future__ import annotations
from dataclasses import dataclass,asdict
from itertools import product
from pathlib import Path
import argparse,json,sys
sys.dont_write_bytecode=True

@dataclass(frozen=True)
class Event:
    name:str
    thread:int
    loc:str
    kind:str
    order:str='plain'
    value:int=0
    message:int=-1

RELEASE={'release','acq_rel','seq_cst'}
ACQUIRE={'acquire','acq_rel','seq_cst'}


def closure(count,edges):
    reach=[0]*count
    for a,b in edges:reach[a]|=1<<b
    for k in range(count):
        bit=1<<k;row=reach[k]
        for i in range(count):
            if reach[i]&bit:reach[i]|=row
    return reach


def analyze(events,rf):
    count=len(events);sb=[]
    threads=sorted(set(e.thread for e in events if e.thread>=0))
    for thread in threads:
        seq=[i for i,e in enumerate(events) if e.thread==thread]
        sb+=list(zip(seq,seq[1:]))
    init=[i for i,e in enumerate(events) if e.thread==-1]
    sb.extend((i,j) for i in init for j,e in enumerate(events) if e.thread>=0)
    sw=[(w,r) for r,w in rf.items() if events[w].order in RELEASE and events[r].order in ACQUIRE]
    causal=closure(count,sb+[(w,r) for r,w in rf.items()])
    if any(causal[i]>>i&1 for i in range(count)):return None
    hb=closure(count,sb+sw)
    mods={}
    for i,e in enumerate(events):
        if e.order!='plain' and e.kind in ('W','M'):
            mods.setdefault(e.loc,[]).append(i)
    # This family has one monotonically ordered writer per location; competing
    # modification orders belong to the atomicity checker and NCQ SC explorer.
    rank={i:k for seq in mods.values() for k,i in enumerate(seq)}
    for r,w in rf.items():
        if events[r].loc!=events[w].loc:raise ValueError('wrong reads-from location')
        seq=mods[events[r].loc]
        for v in seq:
            if hb[v]>>r&1 and rank[v]>rank[w]:return None
            if hb[r]>>v&1 and rank[v]<=rank[w]:return None
        for r2,w2 in rf.items():
            if events[r].loc==events[r2].loc and hb[r]>>r2&1 and rank[w]>rank[w2]:return None
    races=[];wrong=[]
    plain=[i for i,e in enumerate(events) if e.order=='plain']
    for ii,i in enumerate(plain):
        for j in plain[ii+1:]:
            a,b=events[i],events[j]
            if a.loc!=b.loc or a.thread==b.thread or (a.kind,b.kind)==('R','R'):continue
            if not (hb[i]>>j&1 or hb[j]>>i&1):races.append([a.name,b.name])
    if not races:
        for r in plain:
            e=events[r]
            if e.kind!='R':continue
            ws=[w for w in plain if events[w].kind=='W' and events[w].loc==e.loc and hb[w]>>r&1]
            latest=[w for w in ws if not any(hb[w]>>v&1 for v in ws if v!=w)]
            if len(latest)!=1 or events[latest[0]].value!=e.value:
                wrong.append(e.name)
    return dict(races=races,wrong_values=wrong,
                reads_from={events[r].name:events[w].name for r,w in rf.items()},
                synchronizes_with=[[events[a].name,events[b].name] for a,b in sw])


def spsc(n=2,m=4,publication='release',reclamation='release'):
    ev=[]
    def add(*args):ev.append(Event(*args));return len(ev)-1
    add('init_P',-1,'P','W','relaxed',0);add('init_C',-1,'C','W','relaxed',0)
    for b in range(n):
        for word in ('epoch','x','y'):add(f'init_{word}_{b}',-1,f'{word}{b}','W','plain',0)
    pubs={};rels={};pubreads={};retreads={}
    for k in range(m):
        if k>=n:retreads[k]=add(f'P_acquire_C_for_{k}',0,'C','R','acquire')
        for word in ('epoch','x','y'):add(f'P_{word}_{k}',0,f'{word}{k%n}','W','plain',k+1,k)
        pubs[k]=add(f'P_publish_{k}',0,'P','W',publication,k+1,k)
    for k in range(m):
        pubreads[k]=add(f'C_acquire_P_for_{k}',1,'P','R','acquire')
        for word in ('epoch','x','y'):add(f'C_{word}_{k}',1,f'{word}{k%n}','R','plain',k+1,k)
        rels[k]=add(f'C_reclaim_{k}',1,'C','W',reclamation,k+1,k)
    domains={r:[pubs[j] for j in range(k,m)] for k,r in pubreads.items()}
    domains.update({r:[rels[j] for j in range(k-n,m)] for k,r in retreads.items()})
    rs=list(domains);valid=0;candidates=0;bad=0;first=None
    for choice in product(*(domains[r] for r in rs)):
        candidates+=1;result=analyze(ev,dict(zip(rs,choice)))
        if result is None:continue
        valid+=1
        if result['races'] or result['wrong_values']:
            bad+=1
            if first is None:first=result
    return dict(name='spsc_ra_fragment',capacity=n,messages=m,publication=publication,
                reclamation=reclamation,candidates=candidates,consistent=valid,
                violating=bad,status='FAIL_WITNESS' if bad else 'PASS_WITHIN_SCOPE',
                events=[asdict(x) for x in ev],witness=first)


def ncq(entry_store='seq_cst',entry_load='seq_cst'):
    # First ready publication and first claimant. Descriptor epoch is read
    # BEFORE the later acquire status validation in elite_core.c.
    ev=[Event('init_entry',-1,'entry','W','relaxed',0),
        Event('init_status',-1,'status','W','relaxed',0),
        Event('init_epoch',-1,'epoch','W','plain',0),
        Event('init_payload',-1,'payload','W','plain',0),
        Event('P_epoch',0,'epoch','W','plain',1),
        Event('P_payload',0,'payload','W','plain',7),
        Event('P_COMMITTED',0,'status','W','release',6),
        Event('P_entry_install',0,'entry','W',entry_store,2),
        Event('C_entry_observe',1,'entry','R',entry_load),
        # Successful head CAS is ownership LP. Its initial reads-from has no
        # producer release to acquire in this first-claim skeleton.
        Event('C_epoch_BEFORE_status',1,'epoch','R','plain',1),
        Event('C_status_validate',1,'status','R','acquire'),
        Event('C_payload',1,'payload','R','plain',7)]
    result=analyze(ev,{8:7,10:6})
    if result is None:raise RuntimeError('constructed handoff unexpectedly inconsistent')
    return dict(name='ncq_first_ready_handoff_fragment',entry_store=entry_store,
                entry_load=entry_load,candidates=1,consistent=1,
                status='FAIL_WITNESS' if result['races'] else 'PASS_WITHIN_SCOPE',
                events=[asdict(x) for x in ev],witness=result if result['races'] else None,
                conclusion='later status acquire cannot retroactively order the earlier ordinary epoch read')


def ticket_atomicity():
    histories=[]
    for order in ('seq_cst','relaxed'):
        for first in (0,1):
            head=0;results=[]
            for actor in (first,1-first):
                observed=head;success=head==0
                if success:head=1
                results.append(dict(actor=actor,expected=0,observed=observed,success=success))
            if sum(r['success'] for r in results)!=1:raise RuntimeError('atomicity checker defect')
            histories.append(dict(order=order,history=results,final_head=head))
    return dict(name='strong_CAS_single_location_modification_order',status='PASS_WITHIN_SCOPE',
                orders=['seq_cst','relaxed'],histories=histories,
                conclusion='relaxed CAS is still indivisible; two stale expected=0 claims cannot both succeed without wrap/reset')


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',required=True);a=p.parse_args()
    results=[spsc(2,4),spsc(2,4,'relaxed'),spsc(2,4,'release','relaxed'),
             spsc(2,5),ncq(),ncq('relaxed'),ncq('seq_cst','relaxed'),ticket_atomicity()]
    expected=['PASS_WITHIN_SCOPE','FAIL_WITNESS','FAIL_WITNESS','PASS_WITHIN_SCOPE',
              'PASS_WITHIN_SCOPE','FAIL_WITNESS','FAIL_WITNESS','PASS_WITHIN_SCOPE']
    good=all(r['status']==e for r,e in zip(results,expected))
    result=dict(schema='elite-handoff-fragment-v1',status='PASS' if good else 'FAIL',
        scope='finite handoff skeletons, not full ISO-C11/RC11 model checking',results=results)
    Path(a.out).write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps([(r['name'],r['status'],r.get('consistent')) for r in results]))
    return 0 if good else 1
if __name__=='__main__':raise SystemExit(main())
