#!/usr/bin/env python3
"""Exact-state BFS and no-progress-cycle analysis; stdlib only.

Dictionary keys retain full immutable states; Python hash collisions do not
merge unequal states. There is no bitstate search, random sampling, or POR.
Any resource cap is INCOMPLETE. Counterexamples have a shortest BFS prefix.
"""
from __future__ import annotations
import argparse, dataclasses, hashlib, json, sys, time
from pathlib import Path
from collections import deque
sys.dont_write_bytecode=True
from models.ncq import Model


def run(model, max_states=500000, seconds=120):
    begin=time.monotonic();initial=model.initial()
    seen={initial:0};states=[initial];parents=[(-1,'')];edges=[[]]
    bfs=deque([0]);transitions=0;terminals=0;max_depth=0;depths=[0]
    witness=None;status='PASS_WITHIN_SCOPE';reason=None
    lag_seen=False;deadends=0
    while bfs:
        if time.monotonic()-begin>seconds:
            status='INCOMPLETE';reason='wall_budget';break
        sid=bfs.popleft();s=states[sid]
        terminal=model.terminal(s)
        if terminal:terminals+=1
        any_successor=False
        if hasattr(s,"heads") and any(h>t for h,t in zip(s.heads,s.tails)):lag_seen=True
        for label,t,progress,violation in model.successors(s):
            any_successor=True
            transitions+=1
            violation=violation or model.invariant(t)
            if violation:
                witness={'property':violation[0],'details':violation[1],'steps':[]}
                path=[];cur=sid
                while parents[cur][0]>=0:
                    parent,why=parents[cur];path.append((why,dataclasses.asdict(states[cur])));cur=parent
                path.reverse()
                witness['initial']=dataclasses.asdict(initial)
                witness['steps']=[{'event':why,'state':st} for why,st in path]
                witness['steps'].append({'event':label,'state':dataclasses.asdict(t)})
                status='FAIL_WITNESS';break
            tid=seen.get(t)
            if tid is None:
                if len(states)>=max_states:
                    status='INCOMPLETE';reason='state_budget';break
                tid=len(states);seen[t]=tid;states.append(t);parents.append((sid,label));edges.append([])
                depths.append(depths[sid]+1);max_depth=max(max_depth,depths[-1]);bfs.append(tid)
            edges[sid].append((tid,progress))
        if status!='PASS_WITHIN_SCOPE':break
        if not any_successor and not terminal:
            deadends+=1;status='FAIL_DEADLOCK'
            path=[];cur=sid
            while parents[cur][0]>=0:
                parent,why=parents[cur]
                st=states[cur]
                path.append({'event':why,'state':dataclasses.asdict(st) if dataclasses.is_dataclass(st) else st});cur=parent
            path.reverse()
            witness={'property':'NONTERMINAL_DEADLOCK','initial':dataclasses.asdict(initial) if dataclasses.is_dataclass(initial) else initial,'steps':path}
            break
    no_progress=None
    if status=='PASS_WITHIN_SCOPE':
        # Kahn elimination on the graph with LP/return edges removed. A nonempty
        # residue is exact existence of a no-progress directed cycle; terminal
        # states have no implicit scheduler-stutter edges.
        indegree=[0]*len(states)
        for es in edges:
            for target,progress in es:
                if not progress:indegree[target]+=1
        ready=deque(i for i,d in enumerate(indegree) if d==0);removed=0
        while ready:
            i=ready.popleft();removed+=1
            for j,p in edges[i]:
                if not p:
                    indegree[j]-=1
                    if indegree[j]==0:ready.append(j)
        residue=len(states)-removed
        no_progress={'algorithm':'exact topological elimination of nonprogress edges',
                     'residue_states':residue,'cycle_exists':residue!=0,
                     'progress':'queue LP or empty/operation response; not application throughput',
                     'scheduler':'active transition sequences only; no implicit stutter'}
        if residue:status='FAIL_PROGRESS_CYCLE'
    h=hashlib.sha256()
    for s in states:h.update(repr(s).encode());h.update(b'\n')
    return dict(schema='elite-formal-explorer-v1', model=model.name,config=model.config,
                status=status,cutoff=reason,states=len(states),transitions=transitions,
                terminals=terminals,max_shortest_depth=max_depth,frontier_remaining=len(bfs),
                exhaustive=status=='PASS_WITHIN_SCOPE',nonterminal_deadends=deadends,time_seconds=time.monotonic()-begin,
                state_sequence_sha256=h.hexdigest(),legal_head_ahead_of_tail_reached=lag_seen,
                no_progress_analysis=no_progress,witness=witness)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--model', choices=['ncq','spsc'], default='ncq')
    p.add_argument('--capacity',type=int,default=4);p.add_argument('--producers',type=int,default=2)
    p.add_argument('--consumers',type=int,default=2);p.add_argument('--operations',type=int,default=1)
    p.add_argument('--mutation',choices=['none','split_head','stale_head_retry','overwrite_current','tail_before_entry','post_lp_write','early_return','publish_early'],default='none')
    p.add_argument('--no-abort',action='store_true');p.add_argument('--max-states',type=int,default=500000)
    p.add_argument('--seconds',type=float,default=120);p.add_argument('--out',required=True)
    a=p.parse_args()
    if a.model=='spsc':
        from models.spsc import Model as Chosen
    else:Chosen=Model
    m=Chosen(a.capacity,a.producers,a.consumers,a.operations,a.mutation,not a.no_abort)
    result=run(m,a.max_states,a.seconds)
    Path(a.out).write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:result[k] for k in ('status','states','transitions','terminals','cutoff','time_seconds')}))
    return 0 if result['status']=='PASS_WITHIN_SCOPE' else (1 if result['status'].startswith('FAIL') else 2)

if __name__=='__main__':raise SystemExit(main())
