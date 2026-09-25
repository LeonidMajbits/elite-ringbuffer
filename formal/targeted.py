#!/usr/bin/env python3
"""Deterministic stopped-publisher histories over the same transition model."""
import argparse,dataclasses,json
from pathlib import Path
from models.ncq import Model

def scenario(after_publication=False):
    m=Model(4,2,1,8,allow_abort=False);s=m.initial();trace=[]
    def step(i):
        nonlocal s
        choices=[x for x in m.successors(s) if x[0].startswith(str(i)+':')]
        if len(choices)!=1:raise RuntimeError('targeted schedule ambiguous or stuck')
        label,t,_,error=choices[0]
        error=error or m.invariant(t)
        if error:raise RuntimeError(str(error))
        trace.append({'event':label,'state':dataclasses.asdict(t)})
        s=t
    def until(i,pc):
        for _ in range(300):
            if s.actors[i].pc==pc:return
            step(i)
        raise RuntimeError('targeted step bound')
    until(0,23 if after_publication else 7)
    stopped_state=dataclasses.asdict(s);stopped_index=len(trace)
    delivered=0
    if after_publication:
        until(2,13);until(2,24);step(2);delivered+=1
    for _ in range(8):
        until(1,23);until(1,0)
        until(2,13);until(2,24);step(2);delivered+=1
    healthy_end=len(trace)
    before=(s.epochs,s.phases,s.word0,s.word1)
    if after_publication:
        until(0,0)
        if (s.epochs,s.phases,s.word0,s.word1)!=before:
            raise RuntimeError('late publisher touched transferred bytes')
    return dict(status='PASS_WITHIN_SCOPE',scenario='post-publication pause' if after_publication else 'private-write pause',
        paused_actor=0,pause_step=stopped_index,healthy_end_step=healthy_end,
        healthy_messages=delivered,stopped_state=stopped_state,trace=trace,
        scope='targeted schedule, not exhaustive exploration with an explicit death transition')

def stale_consumer():
    """Pause a consumer after its QR entry read; perform >2 physical laps."""
    m=Model(4,2,2,8,allow_abort=False);s=m.initial();trace=[]
    def step(i):
        nonlocal s
        xs=[x for x in m.successors(s) if x[0].startswith(str(i)+':')]
        if len(xs)!=1:raise RuntimeError('targeted stale schedule is ambiguous')
        label,t,_,err=xs[0];err=err or m.invariant(t)
        if err:raise RuntimeError(str(err))
        trace.append({'event':label,'state':dataclasses.asdict(t)});s=t
    def until(i,pc):
        for _ in range(300):
            if s.actors[i].pc==pc:return
            step(i)
        raise RuntimeError('stale schedule step bound')
    until(0,23);until(0,0)  # publish one message
    until(2,4)             # victim has observed it, but has not claimed it
    saved=s.actors[2].ticket
    until(3,13);until(3,24);step(3)
    for _ in range(8):
        until(1,23);until(1,0)
        until(3,13);until(3,24);step(3)
    advanced=s.heads[1]-saved
    before=(s.heads,s.tails,s.frontiers,s.entries,s.epochs,s.phases,s.status_epochs,s.word0,s.word1,s.owners)
    step(2)
    after=(s.heads,s.tails,s.frontiers,s.entries,s.epochs,s.phases,s.status_epochs,s.word0,s.word1,s.owners)
    if before!=after or s.actors[2].pc!=1 or s.actors[2].owns or advanced<=2*m.n:
        raise RuntimeError('stale head observation was not discarded')
    return dict(status='PASS_WITHIN_SCOPE',scenario='stale consumer across physical laps',
        saved_ticket=saved,successful_peer_removals=advanced,final_head=s.heads[1],
        stale_CAS_failed=True,ownership_unchanged=True,trace=trace,
        scope='targeted schedule through actual finite transition model, no wrap')

def main():
    p=argparse.ArgumentParser();p.add_argument('--out',required=True);a=p.parse_args()
    r={'schema':'elite-formal-targeted-v1','results':[scenario(),scenario(True),stale_consumer()]}
    Path(a.out).write_text(json.dumps(r,indent=2)+'\n');print('PASS: stopped private writer, stopped published writer, late metadata-only completion, stale consumer laps')
if __name__=='__main__':main()
