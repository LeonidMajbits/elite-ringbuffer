#!/usr/bin/env python3
"""Predeclare and execute a bounded topology-aware contrast matrix.

Eight audit axes, not an implied exhaustive Cartesian product. Unsupported
P/E pinning, absent NUMA nodes, and insufficient memory are explicit plan cells.
No sudo, cgroup modification, global cache flush or production-core rewrite.
"""
import argparse
from fractions import Fraction
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time
from verify_matrix_results import (topology, val, cpuset, geometry, verify, verify_record, digest, load, need)

AXES={
 'locality':['unconstrained','same_l2','cross_numa','intra_perf','perf_to_efficiency'],
 'population':['1/1','2/2','4/4','8/8','16/1','1/16','16/16'],
 'pool_size':['small','l2_exceeding_candidate','llc_exceeding_candidate'],
 'arrival':['closed_loop','saturation','fixed_rate','bursts'],
 'retention':['immediate','held_view','all_tokens_retained'],
 'runtime':['native_c','python_helper','python_bytecode','python_gc'],
 'os_faults':['observed_host_quota','yield_requests','host_pressure_requires_isolated_host'],
 'integrity':['NONE','CRC64','owned_corruption_existing_adversarial_lane']}


def machine_budget(t,maximum):
    limits=[maximum]
    mem=val(t['physical_memory_bytes'])
    if mem:limits.append(mem//2)
    for g in t['cgroups']:
        limit=val(g['memory_max']);used=val(g['memory_current'])
        if limit is not None:limits.append(max(0,(limit-(used or 0))//2))
    return min(limits)


def plan_cells(t,count,small,max_bytes,python):
    cpus=topology(t);allowed=sorted(cpus);darwin=t['platform']=='Darwin'
    quota=[]
    for g in t['cgroups']:
        q,p=val(g['quota_us']),val(g['period_us'])
        if q is not None and p:quota.append(Fraction(q,p))
    # Limits select a finite affinity pool, not an exclusive-core allocation.
    pool=allowed[:max(1,min(len(allowed),int(min(quota))))] if quota else allowed
    default=dict(mode='ncq',messages=count,warmup_messages=min(1024,count),producers=1,consumers=1,
                 capacity=32,payload_bytes=64,checksum=0,arrival=1,burst=32,period_ns=10000,
                 hold_ns=0,hold_every=16,os_yield=0,runtime='native_c',locality_request='unconstrained',
                 cpus=pool,qos=1 if darwin else 0)
    cells=[]
    def add(ident,axis,**changes):
        cell={**default,**changes,'id':ident,'axis':axis,'status':'RUN'}
        if cell['runtime']!='native_c' and not python:cell.update(status='SKIP_UNSUPPORTED',reason='Python explicitly disabled for this campaign')
        p=cell['producers'];cell['messages']=max(p,(cell['messages']//p)*p);cell['warmup_messages']=(cell['warmup_messages']//p)*p
        k=p+cell['consumers'];backing=geometry(cell['mode'],cell['capacity'],cell['payload_bytes'],k)
        evidence=24*cell['messages']*(cell['consumers']+1)+80*cell['messages']+64*1024*1024
        cell['resource_estimate_bytes']=backing+evidence
        if backing>max_bytes or backing+evidence>max_bytes:cell.update(status='SKIP_RESOURCE',reason='Declared backing+trace+analysis budget exceeds live resource policy')
        cells.append(cell);return cell
    add('spsc-closed-small','arrival',mode='spsc',arrival=0)
    for p,c in ((1,1),(2,2),(4,4),(8,8),(16,1),(1,16),(16,16)):
        add(f'ncq-pop-{p}-{c}','population',producers=p,consumers=c)
    add('ncq-closed-small','arrival',arrival=0)
    add('spsc-saturation-small','population',mode='spsc')
    add('ncq-fixed-rate','arrival',messages=2048,warmup_messages=64,arrival=2)
    add('ncq-bursts','arrival',messages=2048,warmup_messages=64,arrival=3,period_ns=320000)
    for mode in ('spsc','ncq'):
        add(mode+'-retained','retention',mode=mode,messages=1024,warmup_messages=64,capacity=2,hold_ns=100000,hold_every=1)
        add(mode+'-crc64','integrity',mode=mode,messages=1024,warmup_messages=64,payload_bytes=4096,checksum=1)
        add(mode+'-helper-c-1m','runtime',mode=mode,messages=1024,warmup_messages=16,capacity=8,payload_bytes=1048576)
        add(mode+'-python-1m','runtime',mode=mode,messages=1024,warmup_messages=16,capacity=8,payload_bytes=1048576,runtime='python_helper')
        add(mode+'-python-bytecode','runtime',mode=mode,messages=512,warmup_messages=32,runtime='python_bytecode',arrival=0)
        add(mode+'-python-retained-gc','runtime',mode=mode,messages=256,warmup_messages=32,runtime='python_gc',hold_ns=100000,hold_every=4)
        add(mode+'-llc-candidate','pool_size',mode=mode,messages=16 if small else 64,warmup_messages=2,capacity=2,payload_bytes=67108864)
    add('ncq-l2-candidate','pool_size',capacity=1024,payload_bytes=4096,messages=4096,warmup_messages=1024)
    add('ncq-yield-requests','os_faults',producers=4,consumers=4,messages=4096,os_yield=1)
    add('python-asymmetric-1-16','population',runtime='python_helper',producers=1,consumers=16,messages=512,warmup_messages=32)
    add('python-asymmetric-16-1','population',runtime='python_helper',producers=16,consumers=1,messages=512,warmup_messages=32)
    pair=None
    for i in allowed:
        for ca in cpus[i]['caches']:
            if val(ca['level'])==2:
                group=sorted(cpuset(ca['shared_cpus'])&set(allowed))
                if len(group)>=2:
                    j=group[1] if group[0]==i else group[0]
                    if any(val(cb['level'])==2 and {i,j}<=cpuset(cb['shared_cpus']) for cb in cpus[j]['caches']):pair=[i,j];break
        if pair:break
    same=add('reported-same-l2','locality',mode='spsc',arrival=0,locality_request='same_l2',cpus=pair or [])
    if not pair:same.update(status='SKIP_UNSUPPORTED',reason='No mutually reported shared-L2 CPU pair with enforceable IDs')
    nodes={}
    for i,v in cpus.items():
        if v['numa_node']>=0:nodes.setdefault(v['numa_node'],[]).append(i)
    cross=add('reported-cross-numa','locality',mode='spsc',arrival=0,locality_request='cross_numa',cpus=[nodes[n][0] for n in sorted(nodes)[:2]])
    if len(nodes)<2:cross.update(status='SKIP_UNSUPPORTED',reason='Fewer than two NUMA nodes in eligible CPU mapping')
    for name in ('intra_perf','perf_to_efficiency'):
        cell=add(name,'locality',locality_request=name)
        cell.update(status='SKIP_UNSUPPORTED',reason='Aggregate perflevel geometry does not expose an enforced per-worker P/E map')
    for ident,axis,reason in (
        ('all-tokens-retained','retention','A concurrent all-token lease fixture requires a separate barrier schedule; not fabricated from periodic retention'),
        ('host-memory-pressure','os_faults','System pressure is not injected into an unisolated host; quota and fault observations are retained'),
        ('owned-corruption','integrity','Run the existing test_adversarial integrity negative lane; no success-throughput result is manufactured')):
        cell=add(ident,axis);cell.update(status='SKIP_UNSUPPORTED',reason=reason)
    return cells


def execute(command,cwd,env,log,timeout):
    Path(str(log)+'.command.json').write_text(json.dumps({'argv':list(map(str,command)),'cwd':str(cwd),'watchdog_seconds':timeout},indent=2)+'\n')
    with Path(log).open('wb') as f:
        proc=subprocess.Popen(command,cwd=cwd,env=env,stdout=f,stderr=subprocess.STDOUT,start_new_session=True)
        try:return proc.wait(timeout)
        except subprocess.TimeoutExpired:
            # Coordinator's SIGTERM handler owns cleanup. Escalate only its new process group.
            os.killpg(proc.pid,signal.SIGTERM)
            try:proc.wait(5)
            except subprocess.TimeoutExpired:os.killpg(proc.pid,signal.SIGKILL);proc.wait()
            raise TimeoutError('owned campaign watchdog expired')


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build',type=Path,default=Path('build/native'));p.add_argument('--out',type=Path,required=True)
    p.add_argument('--trials',type=int,default=3);p.add_argument('--count',type=int,default=20000);p.add_argument('--timeout',type=float,default=120)
    p.add_argument('--max-memory-mib',type=int,default=512);p.add_argument('--no-python',action='store_true');p.add_argument('--small',action='store_true');p.add_argument('--plan-only',action='store_true')
    a=p.parse_args();need(1<=a.trials<=100 and 32<=a.count<=1000000 and 0<a.timeout<=86400 and 128<=a.max_memory_mib<=2048,'invalid limits')
    root=Path(__file__).resolve().parents[1];build=a.build.resolve();out=a.out.resolve();out.mkdir(parents=True,exist_ok=False)
    top=load_json_command([str(build/'bench_topology')]);budget=machine_budget(top,a.max_memory_mib*1048576)
    cells=plan_cells(top,a.count,a.small,budget,not a.no_python)
    plan={'schema':'elite-matrix-plan-v1','axes':AXES,'coverage':'predeclared contrast design; NOT full Cartesian qualification','trials':a.trials,'resource_budget_bytes':budget,'hardware_topology':top,'cells':cells}
    (out/'matrix-plan.json').write_text(json.dumps(plan,indent=2)+'\n');(out/'hardware_topology.json').write_text(json.dumps(top,indent=2)+'\n')
    if a.plan_only:print('PLAN_ONLY',len(cells));return
    env=dict(os.environ);env['PYTHONPATH']=os.pathsep.join([str(build/'python'),str(root/'bindings/python')]);env['ELITE_LIBRARY']=str(build/('libelite_ringbuffer.'+('dylib' if sys.platform=='darwin' else 'so')))
    n=0
    for cell in cells:
        if cell['status']!='RUN':continue
        print('RUN',cell['id'],flush=True);dest=out/cell['id']
        if cell['runtime']=='native_c':
            command=[str(build/'bench_matrix'),'--mode',cell['mode'],'--producers',str(cell['producers']),'--consumers',str(cell['consumers']),
                '--count',str(cell['messages']),'--warmup',str(cell['warmup_messages']),'--capacity',str(cell['capacity']),'--payload',str(cell['payload_bytes']),
                '--checksum',str(cell['checksum']),'--arrival',str(cell['arrival']),'--period-ns',str(cell['period_ns']),'--burst',str(cell['burst']),
                '--hold-ns',str(cell['hold_ns']),'--hold-every',str(cell['hold_every']),'--os-yield',str(cell['os_yield']),
                '--locality',cell['locality_request'],'--trials',str(a.trials),'--out',str(dest),'--timeout',str(int(a.timeout))]
            if cell['cpus']:command+=['--cpus',','.join(map(str,cell['cpus']))]
            if top['platform']=='Darwin':command+=['--qos','user-initiated']
            result=execute(command,root,env,out/(cell['id']+'.log'),a.timeout*a.trials+10)
            need(result==0,'native condition failed: '+cell['id'])
        else:
            dest.mkdir();cfg=out/(cell['id']+'.config.json');cfg.write_text(json.dumps(cell,indent=2)+'\n')
            # Separate coordinators give fresh managed generations for repetitions.
            for trial in range(a.trials):
                tmp=dest/f'run-{trial:03d}'
                command=[sys.executable,str(root/'bindings/python/matrix_worker.py'),'--config',str(cfg),'--build',str(build),'--out',str(tmp),'--timeout',str(a.timeout)]
                result=execute(command,root,env,out/(cell['id']+f'-{trial}.log'),a.timeout+10)
                need(result==0,'Python condition failed: '+cell['id'])
                (tmp/'trial-000').rename(dest/f'trial-{trial:03d}');tmp.rmdir()
        for record in sorted(dest.glob('trial-*/result.json')):
            verified=verify_record(record,build/'bench_matrix' if cell['runtime']=='native_c' else None,root)
            (record.parent/'VERIFIED.json').write_text(json.dumps(verified,indent=2)+'\n');n+=1
    (out/'MATRIX_COMPLETE.json').write_text(json.dumps({'plan_sha256':digest(out/'matrix-plan.json'),'completed_records':n,'skipped_cells':[c['id'] for c in cells if c['status']!='RUN']},indent=2)+'\n')
    result=verify(out,build/'bench_matrix',root)
    (out/'REPLAY.json').write_text(json.dumps(result,indent=2)+'\n')
    files=sorted(p for p in out.rglob('*') if p.is_file());(out/'MANIFEST.sha256').write_text(''.join(f'{digest(p)}  {p.relative_to(out).as_posix()}\n' for p in files))
    verify(out,build/'bench_matrix',root)
    print('PASS MATRIX',n,'records',result['messages'],'messages',flush=True)


def load_json_command(command):
    return json.loads(subprocess.check_output(command,text=True))

if __name__=='__main__':
    try:main()
    except (OSError,ValueError,KeyError,TimeoutError,subprocess.SubprocessError) as exc:
        print('FAILED_CAMPAIGN: '+str(exc),file=sys.stderr);sys.exit(1)
