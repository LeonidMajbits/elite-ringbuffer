#!/usr/bin/env python3
"""Measured Python/ctypes IPC with native in-place pattern work, no payload copy.

This is NOT a measurement of pure-Python byte-by-byte serialization or native
C handoff latency. Python executes every reserve/export/commit/borrow/release.
The CPython benchmark helper constructs/checks every byte within the view.
"""
import argparse
import ctypes as C
import hashlib
import json
import os
import pathlib
import platform
import selectors
import signal
import subprocess
import sys
import time
import _elite_buffer
from elite_ringbuffer import EliteShm,EliteSpscProducer,EliteSpscConsumer,EliteNcqProducer,EliteNcqConsumer,cleanup_failures
from elite_ringbuffer import _native as N


def worker():
    req=json.loads(sys.stdin.readline());ep=globals()[req['class']](req['grant'])
    write=req['write'];size=req['size'];warmup=req['warmup'];count=req['count']
    def phase(first,count):
        for ident in range(first,first+count):
            lease=ep.try_reserve() if write else ep.try_borrow()
            while lease is None:
                time.sleep(0)
                lease=ep.try_reserve() if write else ep.try_borrow()
            with lease:
                with lease.buffer as view:
                    if write:_elite_buffer.fill_pattern(view,ident)
                    elif not _elite_buffer.verify_pattern(view,ident):raise RuntimeError('payload mismatch')
                if write:lease.commit(size,9,ident)
                elif lease.message_id!=ident or lease.length!=size or lease.message_type!=9:raise RuntimeError('identity mismatch')
    phase(0,warmup)
    print(json.dumps({'warmup_complete':warmup}),flush=True)
    start=json.loads(sys.stdin.readline())['start_ns']
    remaining=(start-time.perf_counter_ns())/1e9
    if remaining>0:time.sleep(remaining)
    entry=time.perf_counter_ns();phase(warmup,count);end=time.perf_counter_ns()
    receipt=ep.close()
    print(json.dumps({'start_ns':start,'entry_ns':entry,'end_ns':end,'count':count,
                      'write':write,'receipt':receipt,'cleanup_failures':cleanup_failures()}),flush=True)


def line(proc,seconds):
    end=time.monotonic()+seconds
    pending=getattr(proc,'_elite_pending',b'')
    with selectors.DefaultSelector() as selector:
        selector.register(proc.stdout,selectors.EVENT_READ)
        while b'\n' not in pending:
            remaining=end-time.monotonic()
            if remaining<=0 or not selector.select(remaining):
                raise TimeoutError('Python IPC worker did not return a complete record')
            raw=os.read(proc.stdout.fileno(),65536)
            if not raw:raise RuntimeError('Python worker exited before complete result')
            pending+=raw
            if len(pending)>1048576:raise RuntimeError('oversized worker result')
    record,pending=pending.split(b'\n',1)
    proc._elite_pending=pending
    return json.loads(record)


def benchmark(mode,count,size,warmup,timeout):
    population=[];result={};capacity=1024 if size<=4096 else 8
    with EliteShm(mode,capacity=capacity,max_payload=size) as shm:
        try:
            for i in range(2):
                proc=subprocess.Popen([sys.executable,__file__,'--worker'],stdin=subprocess.PIPE,
                    stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
                population.append(proc)
                grant=shm.grant(i,proc.pid)
                cls=('EliteSpsc' if mode=='spsc' else 'EliteNcq')+('Producer' if i==0 else 'Consumer')
                req={'class':cls,'grant':grant,'write':i==0,'count':count,'size':size,'warmup':warmup}
                proc.stdin.write(json.dumps(req)+'\n');proc.stdin.flush()
            for proc in population:
                if line(proc,timeout).get('warmup_complete')!=warmup:raise RuntimeError('bad warmup')
            start=time.perf_counter_ns()+100000000
            for proc in population:proc.stdin.write(json.dumps({'start_ns':start})+'\n');proc.stdin.flush()
            records=[line(proc,timeout) for proc in population]
            for proc,record in zip(population,records):
                if record['count']!=count or record['cleanup_failures']:raise RuntimeError('incomplete work')
                status=shm.reap(proc.pid,timeout)
                if status is None:raise TimeoutError('worker did not terminate')
                proc.returncode=os.waitstatus_to_exitcode(status)
                if proc.returncode!=0:raise RuntimeError(proc.stderr.read())
                shm.ack_cleanup(record['receipt'])
            end=max(record['end_ns'] for record in records)
            elapsed=(end-start)/1e9
            result={'mode':mode,'messages':count,'warmup':warmup,'payload_bytes':size,'capacity':capacity,
                'elapsed_seconds':elapsed,'messages_per_second':count/elapsed,'delivered_GB_per_second':count*size/elapsed/1e9,
                'oracle':'exact sequential identity and native comparison of every payload byte',
                'workload':'Python per-message ctypes calls + native direct pattern fill/check through memoryviews',
                'records':records,'status':'PASS_WITHIN_SCOPE'}
        finally:
            # Only this explicitly owned cohort is targeted. A failed trial is
            # never converted into a partial throughput success.
            for proc in population:
                if proc.returncode is None:
                    try:os.kill(proc.pid,signal.SIGKILL)
                    except ProcessLookupError:pass
                    status=shm.reap(proc.pid,timeout)
                    if status is not None:proc.returncode=os.waitstatus_to_exitcode(status)
                for stream in (proc.stdin,proc.stdout,proc.stderr):stream.close()
    return result


def main():
    p=argparse.ArgumentParser();p.add_argument('--worker',action='store_true');p.add_argument('--mode',choices=['spsc','ncq','both'],default='both')
    p.add_argument('--count',type=int,default=10000);p.add_argument('--size',type=int,default=64)
    p.add_argument('--warmup',type=int,default=1000);p.add_argument('--timeout',type=float,default=60);p.add_argument('--out',type=pathlib.Path)
    args=p.parse_args()
    if args.worker:worker();return
    if not 1<=args.count<=100000000 or not 0<=args.warmup<=100000000 or not 1<=args.size<=1048576 or not 0<args.timeout<=86400:p.error('invalid bounds')
    provenance={'python':sys.version,'platform':platform.platform(),'executable':sys.executable,
                'native_library':str(N.LIBRARY_PATH),'native_sha256':hashlib.sha256(N.LIBRARY_PATH.read_bytes()).hexdigest(),
                'clock':'time.perf_counter_ns; per-process monotonic clock; start/end only',
                'exporter_path':_elite_buffer.__file__,'exporter_sha256':hashlib.sha256(pathlib.Path(_elite_buffer.__file__).read_bytes()).hexdigest(),
                'source_sha256':{str(p.relative_to(pathlib.Path(__file__).resolve().parents[2])):hashlib.sha256(p.read_bytes()).hexdigest() for p in [pathlib.Path(__file__).resolve(),pathlib.Path(__file__).resolve().parent/'elite_buffer.c',pathlib.Path(__file__).resolve().parent/'elite_ringbuffer/__init__.py',pathlib.Path(N.__file__).resolve()]},
                'repetitions':1,'conditions':[]}
    for mode in (['spsc','ncq'] if args.mode=='both' else [args.mode]):
        provenance['conditions'].append(benchmark(mode,args.count,args.size,args.warmup,args.timeout))
    output=json.dumps(provenance,indent=2)+'\n'
    if args.out:
        with args.out.open('x') as f:f.write(output)
    print(output)

if __name__=='__main__':main()
