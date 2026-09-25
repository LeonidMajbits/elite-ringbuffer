#!/usr/bin/env python3
"""Matrix caller: Python public leases; native helpers only for byte work,
clock ordering, and test-control atomics. No native bulk ring loop.
"""
import argparse
from array import array
import ctypes as C
import gc
import hashlib
import json
import os
from pathlib import Path
import resource
import selectors
import signal
import subprocess
import sys
import time
from fractions import Fraction

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from verify_matrix_results import geometry,digest,need
import _elite_buffer
from elite_ringbuffer import EliteShm,EliteSpscProducer,EliteSpscConsumer,EliteNcqProducer,EliteNcqConsumer,cleanup_failures,_encode,_decode
from elite_ringbuffer import _native as N


def quantiles(values,scale):
    # Producer-side summary deliberately separate from the independent verifier.
    ordered=sorted(values);count=len(ordered)
    ranks={'p50':(1,2),'p90':(9,10),'p99':(99,100),'p99_9':(999,1000),
           'p99_99':(9999,10000),'max':(1,1)}
    ticks={k:ordered[(count*a+b-1)//b-1] for k,(a,b) in ranks.items()}
    ns={k:(v*scale.numerator+scale.denominator-1)//scale.denominator for k,v in ticks.items()}
    return {'samples':count,'zeros':sum(v==0 for v in ordered),
            'minimum_nonzero_ticks':min((v for v in ordered if v),default=0),
            'raw_tick_quantiles':ticks,'nanoseconds_ceiling':ns}


def bridge(build):
    ext='dylib' if sys.platform=='darwin' else 'so'
    lib=C.CDLL(str(Path(build)/('libelite_matrix_bridge.'+ext)))
    specs={
        'control_create':(C.c_void_p,[C.c_char_p]),'control_attach':(C.c_void_p,[C.c_char_p]),
        'control_close':(None,[C.c_void_p]),'control_get':(C.c_uint32,[C.c_void_p,C.c_uint32]),
        'control_set':(None,[C.c_void_p,C.c_uint32,C.c_uint32]),'tick':(C.c_uint64,[]),
        'numer':(C.c_uint32,[]),'denom':(C.c_uint32,[]),
        'placement':(None,[C.c_int,C.c_int,C.c_int,C.c_char_p,C.c_size_t]),
        'reconcile':(None,[C.POINTER(N.Grant),C.c_uint64,C.c_char_p])}
    for name,(rest,args) in specs.items():
        fn=getattr(lib,'elite_matrix_'+name);fn.restype=rest;fn.argtypes=args;setattr(lib,name,fn)
    return lib


def line(proc,deadline):
    pending=getattr(proc,'_pending',b'')
    with selectors.DefaultSelector() as sel:
        sel.register(proc.stdout,selectors.EVENT_READ)
        while b'\n' not in pending:
            left=deadline-time.monotonic()
            if left<=0 or not sel.select(left):raise TimeoutError('worker deadline')
            data=os.read(proc.stdout.fileno(),65536)
            if not data:raise RuntimeError('worker exited before complete record')
            pending+=data
            if len(pending)>1048576:raise ValueError('oversized control record')
    one,proc._pending=pending.split(b'\n',1)
    return json.loads(one)


def tell(p,value):
    p.stdin.write((json.dumps(value)+'\n').encode());p.stdin.flush()


def emit(value):
    print(json.dumps(value),flush=True)


def usage_diff(a,b):
    return {'user_us':round((b.ru_utime-a.ru_utime)*1000000),
            'system_us':round((b.ru_stime-a.ru_stime)*1000000),
            'minor_faults':b.ru_minflt-a.ru_minflt,'major_faults':b.ru_majflt-a.ru_majflt,
            'voluntary_context_switches':b.ru_nvcsw-a.ru_nvcsw,'involuntary_context_switches':b.ru_nivcsw-a.ru_nivcsw}


def worker():
    cfg=json.loads(sys.stdin.buffer.readline());b=bridge(cfg['build']);tick=b.tick
    index=cfg['index'];p=cfg['producers'];writer=index<p;count=cfg['messages'];warm=cfg['warmup_messages']
    clock_n,clock_d=b.numer(),b.denom()
    placement=C.create_string_buffer(2048);b.placement(cfg['cpu'],cfg['qos'],0,placement,len(placement))
    ctrl=b.control_attach(cfg['control'].encode());need(ctrl,'control attach')
    cls=globals()[('EliteSpsc' if cfg['mode']=='spsc' else 'EliteNcq')+('Producer' if writer else 'Consumer')]
    ep=cls(cfg['grant']);payload=cfg['payload_bytes'];mode=cfg['runtime']
    trace=array('Q',[0])*(3*(count//p if writer else count))
    bm=bytearray((max(warm,count)+7)//8);polls=0
    def wait(when):
        while tick()<when:pass
    def phase(total,base,start,measured):
        nonlocal polls
        quota=total//p;got=0
        def record(ident,a,bb):
            trace[got*3]=ident;trace[got*3+1]=a;trace[got*3+2]=bb
        if writer:
            for seq in range(quota):
                ident=index*quota+seq;offer=0
                if measured:
                    if cfg['arrival']==0:
                        while b.control_get(ctrl,index)!=seq:polls+=1
                        offer=tick()
                    elif cfg['arrival']==1:offer=tick()
                    else:
                        order=seq*p+index;batch=order//(cfg['burst'] if cfg['arrival']==3 else 1)
                        off=(batch*cfg['period_ns']*clock_d+clock_n-1)//clock_n
                        offer=start+off;wait(offer)
                entry=tick() if measured else 0
                w=ep.try_reserve()
                while w is None:polls+=1;w=ep.try_reserve()
                with w:
                    with w.buffer as v:
                        if mode=='python_bytecode':
                            pattern=((base+ident)^0xa5a5a5a5a5a5a5a5).to_bytes(8,'little')
                            for j in range(payload):v[j]=pattern[j%8]
                        else:_elite_buffer.fill_pattern(v,base+ident)
                        if mode=='python_gc' and ident%32==0:
                            cycle=[];cycle.append(cycle);del cycle;gc.collect()
                    w.commit(payload,7,base+ident)
                if measured:record(ident,offer,entry)
                if measured and cfg['os_yield'] and seq%32==0:os.sched_yield()
                got+=1
        else:
            while True:
                rd=ep.try_borrow()
                if rd is None:
                    polls+=1
                    if b.control_get(ctrl,index)==(2 if measured else 1):
                        rd=ep.try_borrow()
                        if rd is None:break
                    else:continue
                with rd:
                    ident=rd.message_id-base
                    need(0<=ident<total and rd.length==payload and rd.message_type==7,'bad identity')
                    with rd.buffer as v:
                        if mode=='python_bytecode':
                            pattern=((base+ident)^0xa5a5a5a5a5a5a5a5).to_bytes(8,'little')
                            need(all(v[j]==pattern[j%8] for j in range(payload)),'bad payload')
                        else:need(_elite_buffer.verify_pattern(v,base+ident),'bad payload')
                        read_tick=tick() if measured else 0
                        bit=1<<(ident%8);need(not bm[ident//8]&bit,'duplicate');bm[ident//8]|=bit
                        if measured and cfg['hold_ns'] and ident%cfg['hold_every']==0:
                            wait(tick()+(cfg['hold_ns']*clock_d+clock_n-1)//clock_n)
                        if mode=='python_gc' and ident%32==0:
                            cycle=[v];cycle.append(cycle);del cycle;gc.collect()
                release_tick=tick() if measured else 0
                if measured:
                    record(ident,read_tick,release_tick)
                    if cfg['arrival']==0:b.control_set(ctrl,ident//quota,ident%quota+1)
                    if cfg['os_yield'] and ident%32==0:os.sched_yield()
                got+=1
        return got
    emit({'event':'READY','index':index});need(json.loads(sys.stdin.buffer.readline())==1,'warm command')
    warm_got=phase(warm,100000000,0,False);bm[:]=b'\0'*len(bm);emit({'event':'WARM','index':index,'count':warm_got})
    t0=json.loads(sys.stdin.buffer.readline());wait(t0);start=tick();u0=resource.getrusage(resource.RUSAGE_SELF);polls=0
    got=phase(count,0,t0,True);end=tick();u1=resource.getrusage(resource.RUSAGE_SELF)
    b.placement(cfg['cpu'],cfg['qos'],1,placement,len(placement))
    result={'index':index,'count':got,'start_tick':start,'end_tick':end,'empty_polls':polls,
            'placement':json.loads(placement.value),'resources':usage_diff(u0,u1)}
    emit({'event':'DONE','result':result});need(json.loads(sys.stdin.buffer.readline())==0,'end command')
    receipt=ep.close();b.control_close(ctrl)
    directory=Path(cfg['directory']);kind='producer' if writer else 'consumer';ordinal=index if writer else index-p
    if sys.byteorder!='little':trace.byteswap()
    with (directory/f'{kind}-{ordinal:02d}.trace').open('xb') as f:trace[:3*got].tofile(f)
    if not writer:(directory/f'consumer-{ordinal:02d}.bitmap').write_bytes(bm[:(count+7)//8])
    emit({'event':'FINAL','index':index,'receipt':receipt,'cleanup_failures':cleanup_failures()})


def run(cfg,build,directory,topology_exe,timeout):
    build=Path(build).resolve();directory=Path(directory).resolve();directory.mkdir()
    b=bridge(build);clock_n,clock_d=b.numer(),b.denom();scale=Fraction(clock_n,clock_d)
    top=json.loads(subprocess.check_output([str(topology_exe)]));top_after=None
    control=directory/'control.bin';ctrl=b.control_create(str(control).encode());need(ctrl,'create control')
    procs=[];logs=[];records=[];grants=[];deadline=time.monotonic()+timeout
    ext='dylib' if sys.platform=='darwin' else 'so'
    try:
        with EliteShm(cfg['mode'],capacity=cfg['capacity'],max_payload=cfg['payload_bytes'],
                      producers=cfg['producers'],consumers=cfg['consumers'],
                      checksum=bool(cfg['checksum']),max_backing_bytes=536870912) as shm:
            try:
                for i in range(cfg['producers']+cfg['consumers']):
                    log=(directory/f'worker-{i:02d}.stderr').open('wb');logs.append(log)
                    proc=subprocess.Popen([sys.executable,__file__,'--worker'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=log)
                    procs.append(proc);grant=shm.grant(i,proc.pid);grants.append(grant)
                    request={**cfg,'index':i,'grant':grant,'build':str(build),'directory':str(directory),
                             'control':str(control),'cpu':cfg['cpus'][i%len(cfg['cpus'])] if cfg['cpus'] else -1}
                    tell(proc,request)
                for i,proc in enumerate(procs):need(line(proc,deadline)=={'event':'READY','index':i},'ready')
                for proc in procs:tell(proc,1)
                for i,proc in enumerate(procs[:cfg['producers']]):
                    out=line(proc,deadline);need(out['event']=='WARM' and out['count']==cfg['warmup_messages']//cfg['producers'],'warm producer')
                for i in range(cfg['producers'],len(procs)):b.control_set(ctrl,i,1)
                warm_total=0
                for proc in procs[cfg['producers']:]:
                    out=line(proc,deadline);need(out['event']=='WARM','warm consumer');warm_total+=out['count']
                need(warm_total==cfg['warmup_messages'],'warm count')
                t0=b.tick()+(100000000*clock_d+clock_n-1)//clock_n
                for proc in procs:tell(proc,t0)
                for proc in procs[:cfg['producers']]:
                    out=line(proc,deadline);need(out['event']=='DONE','done producer');records.append(out['result'])
                for i in range(cfg['producers'],len(procs)):b.control_set(ctrl,i,2)
                for proc in procs[cfg['producers']:]:
                    out=line(proc,deadline);need(out['event']=='DONE','done consumer');records.append(out['result'])
                for proc in procs:tell(proc,0)
                for i,proc in enumerate(procs):
                    out=line(proc,deadline);need(out['event']=='FINAL' and out['index']==i and not out['cleanup_failures'],'final')
                    status=shm.reap(proc.pid,max(0,deadline-time.monotonic()));need(status is not None,'exit missing');proc.returncode=os.waitstatus_to_exitcode(status);need(proc.returncode==0,'worker failed')
                    shm.ack_cleanup(out['receipt'])
                grant=_decode(N.Grant,grants[0]);b.reconcile(C.byref(grant),cfg['messages']+cfg['warmup_messages'],str(directory).encode())
            finally:
                for proc in procs:
                    if proc.returncode is None:
                        try:os.kill(proc.pid,signal.SIGKILL)
                        except ProcessLookupError:pass
                        status=shm.reap(proc.pid,5)
                        if status is not None:proc.returncode=os.waitstatus_to_exitcode(status)
                    proc.stdin.close();proc.stdout.close()
    finally:
        b.control_close(ctrl);control.unlink(missing_ok=True)
        for log in logs:log.close()
    top_after=json.loads(subprocess.check_output([str(topology_exe)]));count=cfg['messages'];end=max(w['end_tick'] for w in records[cfg['producers']:])
    nbytes=(count+7)//8;combined=bytearray(nbytes);offer=[0]*count;entry=[0]*count;native=[0]*count;service=[0]*count;returned=[0]*count
    from verify_matrix_results import triples
    for i in range(cfg['producers']):
        for ident,a,bb in triples(directory/f'producer-{i:02d}.trace',count//cfg['producers']):offer[ident]=a;entry[ident]=bb
    for i in range(cfg['consumers']):
        one=(directory/f'consumer-{i:02d}.bitmap').read_bytes()
        for j,v in enumerate(one):need(not v&combined[j],'duplicate');combined[j]|=v
        for ident,rd,ret in triples(directory/f'consumer-{i:02d}.trace',records[cfg['producers']+i]['count']):native[ident]=rd-entry[ident];service[ident]=rd-offer[ident];returned[ident]=ret-entry[ident]
    (directory/'union.bitmap').write_bytes(combined)
    result={k:cfg[k] for k in ('mode','messages','warmup_messages','producers','consumers','capacity','payload_bytes','checksum','arrival','burst','period_ns','hold_ns','hold_every','os_yield','runtime','locality_request')}
    result.update({'schema':'elite-matrix-v1','status':'PASS_WITHIN_SCOPE','t0':t0,'end':end,'delta_ticks':end-t0,
        'timebase_numer':clock_n,'timebase_denom':clock_d,'clock':'mach_absolute_time' if sys.platform=='darwin' else 'CLOCK_MONOTONIC_RAW',
        'segment_bytes':geometry(cfg['mode'],cfg['capacity'],cfg['payload_bytes'],cfg['producers']+cfg['consumers']),
        'elapsed_ns':float((end-t0)*scale),'messages_per_second':float(Fraction(count*10**9,end-t0)/scale),
        'delivered_GB_per_second':float(Fraction(count*cfg['payload_bytes'],end-t0)/scale),
        'hardware_topology':top,'hardware_topology_after':top_after,'workers_info':records,
        'physical_cache_residency':'NOT_ESTABLISHED','absolute_clock_uncertainty':'NOT_QUALIFIED',
        'memory_placement':'INITIALIZER_FIRST_TOUCH_NOT_VERIFIED','exact_membership':True,'all_tokens_returned':True,
        'no_payload_in_control_channel':True,'warmup_same_generation':True,'trace_format':'little-endian-u64-triples',
        'binary_sha256':digest(sys.executable),'binary_bytes':Path(sys.executable).stat().st_size,
        'runtime_identity':{'python':sys.version,'interpreter':sys.executable,'exporter_sha256':digest(_elite_buffer.__file__),
                            'native_library_sha256':digest(N.LIBRARY_PATH),'bridge_sha256':digest(build/('libelite_matrix_bridge.'+ext)),
                            'caller_source_sha256':digest(__file__)},
        'native_latency':quantiles(native,scale),'offered_latency':quantiles(service,scale),'returned_latency':quantiles(returned,scale)})
    (directory/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    from verify_matrix_results import verify_record
    verify_record(directory/'result.json')
    return result


def main():
    p=argparse.ArgumentParser();p.add_argument('--worker',action='store_true');p.add_argument('--config',type=Path);p.add_argument('--build',type=Path);p.add_argument('--out',type=Path);p.add_argument('--timeout',type=float,default=60)
    a=p.parse_args()
    if a.worker:worker();return
    cfg=json.loads(a.config.read_text());a.out.mkdir();run(cfg,a.build,a.out/'trial-000',a.build.resolve()/'bench_topology',a.timeout)
if __name__=='__main__':main()
