#!/usr/bin/env python3
"""Independent matrix replay: exact membership, raw timing, topology, token state.

No third-party modules; checks do not use assert (also active under python -O).
A finite completed trace proves only that trace's accounting, not universal
lock-freedom, physical residency, clock uncertainty, or truthful execution.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import sys
from fractions import Fraction

class Invalid(ValueError):
    pass

def need(condition, message):
    if not condition:
        raise Invalid(message)

def integer(x, name, low=0, high=(1 << 64)-1):
    need(type(x) is int and low <= x <= high, name + ': invalid integer')
    return x

def pairs(values):
    result = {}
    for key, value in values:
        need(key not in result, 'duplicate JSON key: ' + key)
        result[key] = value
    return result

def load(path):
    path = Path(path)
    need(path.stat().st_size <= 64*1024*1024, 'JSON exceeds limit')
    def bad(value):
        raise Invalid('nonfinite JSON: ' + value)
    return json.loads(path.read_text(), object_pairs_hook=pairs, parse_constant=bad)

def digest(path):
    with Path(path).open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()

def near(value, expected, name):
    need(type(value) in (float, int) and math.isfinite(value), name + ': nonfinite/non-numeric')
    target = float(expected)
    need(math.isfinite(target), name + ': overflow')
    need(abs(Fraction(value)-expected) <= Fraction(math.ulp(target)), name + ': >1 ULP error')

def val(o):
    need(isinstance(o, dict) and o.get('status') in ('OBSERVED','UNAVAILABLE'), 'invalid scalar observation')
    integer(o['error'], 'observation errno', 0, 2**31-1)
    if o['status'] == 'UNAVAILABLE':
        need(o['value'] is None, 'unavailable value invented')
        return None
    need(o['error'] == 0, 'observed value with error')
    return integer(o['value'], 'observation value')

def cpuset(o):
    need(o['status'] in ('OBSERVED','UNAVAILABLE'), 'invalid set status')
    ids = o['cpus']
    need(isinstance(ids, list), 'cpuset array missing')
    for i in ids:
        integer(i,'cpu/node id',0,1023)
    need(ids == sorted(set(ids)), 'unsorted or duplicate CPU list')
    integer(o['error'],'cpuset errno',0,2**31-1)
    need((o['error'] == 0) == (o['status'] == 'OBSERVED'), 'set status mismatch')
    need(o['status'] == 'OBSERVED' or not ids, 'failed cpuset has guessed IDs')
    return set(ids)

def topology(t, synthetic=False):
    need(t['schema']=='elite-hardware-topology-v1','wrong topology schema')
    need(t['source_kind'] == ('SYNTHETIC_FIXTURE' if synthetic else 'LIVE_OS_VISIBLE'), 'synthetic/live topology mix')
    allowed, online = cpuset(t['allowed_cpus']), cpuset(t['online_cpus'])
    if not t['online_cpus']['error']:
        need(allowed <= online, 'allowed CPU not online in snapshot')
    for key in ('page_bytes','physical_memory_bytes','physical_cpus','logical_cpus',
                'memory_frequency_hz','memory_channels','system_l3_bytes','cache_line_bytes'):
        val(t[key])
    cpus = {}
    for c in t['cpus']:
        ident = integer(c['cpu_id'],'cpu id',0,1023)
        need(ident in allowed and ident not in cpus,'CPU scope/uniqueness mismatch')
        cpus[ident]=c
        for k in ('package_id','die_id','core_id','cluster_id'):
            val(c[k])
        siblings=cpuset(c['smt_siblings'])
        if not c['smt_siblings']['error']:
            need(ident in siblings,'CPU missing itself from sibling list')
        indices=set()
        for cache in c['caches']:
            ix=integer(cache['index'],'cache index',0,2**32-1)
            need(ix not in indices,'duplicate cache index');indices.add(ix)
            level,size,line=val(cache['level']),val(cache['bytes']),val(cache['line_bytes'])
            val(cache['id']);shared=cpuset(cache['shared_cpus'])
            if not cache['shared_cpus']['error']:
                need(ident in shared,'cache excludes reporting CPU')
            if level is not None:need(1<=level<=32,'bad cache level')
            if size is not None:need(size>0,'zero cache size')
            if line is not None:need(line>0 and line&(line-1)==0,'invalid line size')
    if t['platform']=='Linux' and not t['partial']:
        need(set(cpus)==allowed,'eligible topology records missing')
    nodes={}
    for n in t['nodes']:
        ident=integer(n['node_id'],'node id',0,2**31-1)
        need(ident not in nodes,'duplicate NUMA node');nodes[ident]=cpuset(n['cpus'])
    for i,c in cpus.items():
        node=c['numa_node'];integer(node,'cpu NUMA node',-2,2**31-1)
        if node>=0:need(node in nodes and i in nodes[node],'NUMA map mismatch')
    for g in t['cgroups']:
        q,p=val(g['quota_us']),val(g['period_us'])
        val(g['memory_max']);val(g['memory_current']);cpuset(g['cpus_effective']);cpuset(g['mems_effective'])
        if q is not None:need(q>0 and p is not None and p>0 and not g['quota_unlimited'],'bad quota')
    for level in t['perflevels']:
        for key in ('physical','logical','l1d_bytes','l2_bytes','l3_bytes','cpus_per_l2'):val(level[key])
        expected={'Performance':'TOPOLOGY_CLUSTER_PERF','Efficiency':'TOPOLOGY_CLUSTER_EFFICIENCY'}.get(level['name'],'TOPOLOGY_CLUSTER_UNKNOWN')
        need(level['cluster_class']==expected,'invented perflevel classification')
    return cpus

def locality(t, workers, request):
    cpus=topology(t)
    if t['platform']=='Darwin':
        for w in workers:
            p=w['placement'];need(not p['affinity_enforced'] and p['verified_cpu_before']==-1 and p['verified_cpu_after']==-1,'Darwin CPU pin invented')
        need(request in ('unconstrained','unknown'),'P/E placement not supplied by aggregate sysctls')
        return 'UNKNOWN_DARWIN_SCHEDULER_PLACEMENT'
    ids=[]
    for w in workers:
        p=w['placement'];requested=integer(p['requested_cpu'],'requested CPU',-1,1023)
        if requested==-1:
            need(not p['affinity_enforced'],'unrequested affinity claim')
            continue
        need(p['affinity_enforced'] is True and p['affinity_error']==0,'affinity not enforced')
        need(p['verified_cpu_before']==requested==p['verified_cpu_after'],'mask readback mismatch')
        need(requested in cpus,'CPU absent from topology')
        ids.append(requested)
    need(request in ('unconstrained','unknown','same_l2','cross_numa','same_numa'), 'unknown locality request')
    if request in ('unconstrained','unknown'):return 'UNQUALIFIED_LOCALITY'
    need(len(ids)==len(workers),'claimed locality has unpinned worker')
    if request=='same_l2':
        members=set(ids)
        for i in members:
            need(any(val(c['level'])==2 and members<=cpuset(c['shared_cpus']) for c in cpus[i]['caches']), 'not shared L2')
        return 'ENFORCED_CPU_MASKS_REPORTED_SHARED_L2'
    node_ids={cpus[i]['numa_node'] for i in ids};need(-1 not in node_ids and -2 not in node_ids,'unknown NUMA membership')
    if request=='same_numa':need(len(node_ids)==1,'multiple NUMA nodes');return 'ENFORCED_CPU_MASKS_REPORTED_SAME_NUMA'
    need(len(node_ids)>1,'cross-NUMA lacks two nodes')
    # Role-specific crossing must be genuine, not one incidental extra worker.
    return 'ENFORCED_CPU_MASKS_REPORTED_CROSS_NUMA_CPU_ONLY'

def geometry(mode,n,payload,k):
    ru=lambda x:((x+16383)//16384)*16384
    o=ru(16384+256*k)
    if mode=='ncq':o=ru(o+128*n);o=ru(o+128*n)
    return ru(ru(o+128*n)+n*((payload+127)//128)*128)

def cache_bounds(t, workers, pool, segment):
    ids={w['placement']['verified_cpu_before'] for w in workers};ids.discard(-1)
    selected=[c for c in t['cpus'] if not ids or c['cpu_id'] in ids]
    l2=[];domains={};complete=bool(selected) and not t['partial']
    for c in selected:
        levels=[x for x in c['caches'] if x['type'] in ('Data','Unified') and val(x['level']) is not None and val(x['bytes'])]
        l2.extend(val(x['bytes']) for x in levels if val(x['level'])==2)
        if not levels:complete=False;continue
        last=max(val(x['level']) for x in levels)
        for x in levels:
            if val(x['level'])!=last:continue
            key=(last,tuple(sorted(cpuset(x['shared_cpus']))),val(x['id']))
            if key in domains:need(domains[key]==val(x['bytes']),'inconsistent shared-cache capacity')
            domains[key]=val(x['bytes'])
    if t['platform']=='Darwin':
        # L2 is not silently renamed system-level/last-level cache.
        l2=[val(x['l2_bytes']) for x in t['perflevels'] if val(x['l2_bytes'])]
        complete=False
    return {'payload_pool_bytes':pool,'segment_bytes':segment,
            'min_observed_l2_bytes':min(l2) if l2 else None,
            'reported_selected_llc_sum_bytes':sum(domains.values()) if complete else None,
            'pool_capacity_below_all_reported_l2':bool(l2) and pool<=min(l2),
            'pool_capacity_above_reported_llc_sum':complete and pool>sum(domains.values()),
            'cache_residency':'NOT_ESTABLISHED','dram_saturation':'NOT_ESTABLISHED'}

def quantiles(values,scale):
    need(values,'empty latency population');s=sorted(values);n=len(s)
    names=('p50','p90','p99','p99_9','p99_99','max');fr=(50000,90000,99000,99900,99990,100000)
    q={name:s[(n*num+99999)//100000-1] for name,num in zip(names,fr)}
    return {'samples':n,'zeros':sum(x==0 for x in s),'minimum_nonzero_ticks':next((x for x in s if x),0),
            'raw_tick_quantiles':q,'nanoseconds_ceiling':{k:math.ceil(Fraction(v)*scale) for k,v in q.items()}}

def triples(path,count):
    path=Path(path);need(path.is_file() and not path.is_symlink(),'trace unavailable/symlink')
    need(path.stat().st_size==count*24,'trace size mismatch')
    with path.open('rb') as f:
        while True:
            data=f.read(24*4096)
            if not data:return
            yield from struct.iter_unpack('<QQQ',data)

def reconcile(path,mode,n,total,segment):
    x=load(path);need(x['status']=='PASS_WITHIN_SCOPE' and x['all_tokens_returned'] is True,'reconciliation failed')
    need(x['publications']==total and x['segment_bytes']==segment and x['capacity']==n,'reconcile identity')
    if mode=='spsc':need(x['published']==x['reclaimed']==total,'SPSC residual tokens')
    else:
        need(x['qf_head']==total and x['qf_tail']==total+n and x['qr_head']==x['qr_tail']==total+n,'NCQ cursors do not reconcile')
        entries=x['free_entries'];need(len(entries)==n,'missing tokens')
        seen=set()
        for ticket,e in enumerate(entries,total):
            need(e['ticket']==ticket and e['entry']//n==ticket//n,'free entry cycle mismatch')
            idx=e['entry']&(n-1);need(idx==e['block'] and idx not in seen,'duplicate free token');seen.add(idx)
            need(e['status_word']&3==0 and e['status_word']>>2==e['epoch'],'retained/nonempty descriptor')
        need(seen==set(range(n)),'incomplete token set')

def verify_record(path,binary=None,source_root=None):
    path=Path(path);root=path.parent;x=load(path)
    need(x['schema']=='elite-matrix-v1' and x['status']=='PASS_WITHIN_SCOPE','not a completed matrix record')
    n=integer(x['messages'],'messages',1,1000000);p=integer(x['producers'],'producers',1,16);c=integer(x['consumers'],'consumers',1,16)
    capacity=integer(x['capacity'],'capacity',2,65536);payload=integer(x['payload_bytes'],'payload',8,67108864)
    need(capacity&(capacity-1)==0 and p+c<=min(capacity,32) and n%p==0 and payload%8==0,'invalid workload geometry')
    need(x['mode'] in ('spsc','ncq') and (x['mode']!='spsc' or p==c==1),'invalid queue mode')
    integer(x['warmup_messages'],'warmup',0,1000000);need(x['warmup_messages']%p==0,'warmup quotas')
    integer(x['checksum'],'checksum',0,1);integer(x['arrival'],'arrival',0,3);integer(x['os_yield'],'yield',0,1)
    need(x['runtime'] in ('native_c','python_helper','python_bytecode','python_gc'),'unknown runtime')
    integer(x['burst'],'burst',1,1024);integer(x['period_ns'],'period',1,1000000000);integer(x['hold_ns'],'hold',0,1000000000);integer(x['hold_every'],'hold frequency',1,100000000)
    t0=integer(x['t0'],'t0');end=integer(x['end'],'end',t0+1);need(x['delta_ticks']==end-t0,'duration mismatch')
    num=integer(x['timebase_numer'],'timebase numerator',1,2**32-1);den=integer(x['timebase_denom'],'timebase denominator',1,2**32-1);scale=Fraction(num,den)
    near(x['elapsed_ns'],(end-t0)*scale,'elapsed_ns');near(x['messages_per_second'],Fraction(n*10**9,end-t0)/scale,'rate');near(x['delivered_GB_per_second'],Fraction(n*payload,end-t0)/scale,'GB/s')
    need(x['segment_bytes']==geometry(x['mode'],capacity,payload,p+c),'backing geometry mismatch')
    workers=x['workers_info'];need(len(workers)==p+c,'worker population mismatch')
    for i,w in enumerate(workers):
        need(w['index']==i,'worker identity order');integer(w['count'],'worker count',0,n)
        integer(w['start_tick'],'start',t0,end);integer(w['end_tick'],'end',w['start_tick'],end)
        if i<p:need(w['count']==n//p,'producer quota mismatch')
    need(sum(w['count'] for w in workers[p:])==n and max(w['end_tick'] for w in workers[p:])==end,'drain/count mismatch')
    kind=locality(x['hardware_topology'],workers,x['locality_request'])
    kind_after=locality(x['hardware_topology_after'],workers,x['locality_request']);need(kind_after==kind,'topology changed locality')
    if x['locality_request']=='cross_numa':
        topo={v['cpu_id']:v for v in x['hardware_topology']['cpus']}
        pn={topo[w['placement']['verified_cpu_before']]['numa_node'] for w in workers[:p]};cn={topo[w['placement']['verified_cpu_before']]['numa_node'] for w in workers[p:]}
        need(not pn&cn,'cross-NUMA roles overlap')
    need(x['physical_cache_residency']=='NOT_ESTABLISHED' and x['absolute_clock_uncertainty']=='NOT_QUALIFIED','unqualified residency/clock certificate')
    need(x['exact_membership'] is True and x['all_tokens_returned'] is True and x['no_payload_in_control_channel'] is True and x['warmup_same_generation'] is True,'required evidence predicates')
    need(x['trace_format']=='little-endian-u64-triples','unsupported trace format')
    need(isinstance(x['binary_sha256'],str) and len(x['binary_sha256'])==64 and all(a in '0123456789abcdef' for a in x['binary_sha256']),'bad binary hash')
    if binary:need(digest(binary)==x['binary_sha256'] and Path(binary).stat().st_size==x['binary_bytes'],'external binary mismatch')
    if x['runtime']!='native_c':
        identity=x['runtime_identity']
        for key in ('exporter_sha256','native_library_sha256','bridge_sha256','caller_source_sha256'):
            h=identity[key];need(isinstance(h,str) and len(h)==64 and all(v in '0123456789abcdef' for v in h),'invalid runtime identity')
        if source_root:need(digest(Path(source_root)/'bindings/python/matrix_worker.py')==identity['caller_source_sha256'],'Python caller changed')
    if source_root and x['runtime']=='native_c':
        for name,h in x['build_identity']['source_files'].items():
            pp=Path(name);need(not pp.is_absolute() and '..' not in pp.parts,'unsafe source path');need(digest(Path(source_root)/pp)==h,'source changed: '+name)
        material=''.join(f'{h}  {name}\n' for name,h in sorted(x['build_identity']['source_files'].items())).encode()
        need(hashlib.sha256(material).hexdigest()==x['build_identity']['source_sha256'],'source snapshot mismatch')
    offer=[0]*n;entry=[0]*n;quota=n//p
    for i in range(p):
        previous=0
        for j,(ident,a,b) in enumerate(triples(root/f'producer-{i:02d}.trace',quota)):
            need(ident==i*quota+j and t0<=a<=b<=workers[i]['end_tick'] and b>=previous,'bad producer trace')
            previous=b
            if x['arrival']>=2:
                order=j*p+i;batch=order//(x['burst'] if x['arrival']==3 else 1)
                expected=t0+math.ceil(Fraction(batch*x['period_ns']*den,num))
                need(a==expected,'schedule shifted after delay')
            offer[ident]=a;entry[ident]=b
    bytes_n=(n+7)//8;union=bytearray(bytes_n);native=[0]*n;service=[0]*n;returned=[0]*n;released=[0]*n
    for i in range(c):
        bm=(root/f'consumer-{i:02d}.bitmap').read_bytes();need(len(bm)==bytes_n,'bitmap size')
        need(sum(v.bit_count() for v in bm)==workers[p+i]['count'],'bitmap population mismatch')
        for j,v in enumerate(bm):need(not union[j]&v,'duplicate consumer membership');union[j]|=v
        seen=bytearray(bytes_n);last=0
        for ident,rd,ret in triples(root/f'consumer-{i:02d}.trace',workers[p+i]['count']):
            need(ident<n and entry[ident]<=rd<=ret<=workers[p+i]['end_tick'] and rd>=last,'consumer timing mismatch');last=ret
            bit=1<<(ident%8);need(bm[ident//8]&bit and not seen[ident//8]&bit,'trace membership duplicate');seen[ident//8]|=bit
            if x['hold_ns'] and ident%x['hold_every']==0:need((ret-rd)*scale>=x['hold_ns'],'retention omitted')
            native[ident]=rd-entry[ident];service[ident]=rd-offer[ident];returned[ident]=ret-entry[ident];released[ident]=ret
        need(seen==bm,'bitmap/trace difference')
    expected=bytes([255])*(n//8)+(bytes([(1<<(n%8))-1]) if n%8 else b'')
    need(bytes(union)==expected and (root/'union.bitmap').read_bytes()==expected,'missing data or padding bits')
    if x['arrival']==0:
        for i in range(p):
            for j in range(1,quota):need(offer[i*quota+j]>=released[i*quota+j-1],'closed loop did not await release')
    for key,values in [('native_latency',native),('offered_latency',service),('returned_latency',returned)]:
        need(x[key]==quantiles(values,scale),'latency summary mismatch: '+key)
    reconcile(root/'final-reconcile.json',x['mode'],capacity,n+x['warmup_messages'],x['segment_bytes'])
    return {'status':'PASS_WITHIN_SCOPE','messages':n,'runtime':x['runtime'],'mode':x['mode'],
            'locality':kind,'cache_bounds':cache_bounds(x['hardware_topology'],workers,capacity*((payload+127)//128)*128,x['segment_bytes']),
            'finite_run_token_accounting':'COMPLETE','universal_deadlock_freedom':'NOT_PROVED',
            'one_way_metrology_qualification':'NOT_QUALIFIED','messages_per_second':x['messages_per_second'],
            'p99_offered_ns':x['offered_latency']['nanoseconds_ceiling']['p99']}

def verify(path,binary=None,source_root=None):
    path=Path(path)
    if path.is_file():return verify_record(path,binary,source_root)
    if (path/'MANIFEST.sha256').exists():verify_manifest(path)
    plan=load(path/'matrix-plan.json');complete=load(path/'MATRIX_COMPLETE.json')
    need(plan['schema']=='elite-matrix-plan-v1','wrong plan schema')
    integer(plan['trials'],'trial count',1,100)
    need(complete['plan_sha256']==digest(path/'matrix-plan.json'),'plan identity mismatch')
    cells=plan['cells'];need(len({c['id'] for c in cells})==len(cells),'duplicate plan cell')
    reports=[];skips=[]
    for cell in cells:
        ident=cell['id'];need('/' not in ident and '\\' not in ident and ident not in ('.','..'),'unsafe cell path')
        need(cell['status'] in ('RUN','SKIP_UNSUPPORTED','SKIP_RESOURCE'),'unknown planned status')
        if cell['status']!='RUN':
            need(bool(cell.get('reason')),'unsupported cell lacks reason');skips.append(ident);continue
        cell_dir=path/ident
        records=sorted(cell_dir.glob('trial-*/result.json'))
        need([rec.parent.name for rec in records]==[f'trial-{i:03d}' for i in range(plan['trials'])],'missing, duplicate, or renamed trial')
        for record in records:
            x=load(record)
            for key in ('mode','producers','consumers','capacity','payload_bytes','arrival','hold_ns','hold_every','period_ns','burst','checksum','runtime','os_yield','locality_request'):
                need(x[key]==cell[key],'record does not match preregistered cell '+key)
            need(x['messages']==cell['messages'] and x['warmup_messages']==cell['warmup_messages'],'count does not match plan')
            reports.append(verify_record(record,binary if x['runtime']=='native_c' else None,source_root))
    need(complete['completed_records']==len(reports) and complete['skipped_cells']==skips,'completion marker mismatch')
    allowed={c['id'] for c in cells if c['status']=='RUN'}
    need({p.parent.parent.name for p in path.glob('*/trial-*/result.json')}<=allowed,'unplanned result')
    return {'status':'PASS_WITHIN_SCOPE','records':len(reports),'messages':sum(x['messages'] for x in reports),'skipped_cells':skips,'results':reports}

def verify_manifest(root):
    root=Path(root);seen=set()
    for line in (root/'MANIFEST.sha256').read_text().splitlines():
        need('  ' in line,'invalid manifest line');h,name=line.split('  ',1)
        pp=Path(name);need(not pp.is_absolute() and '..' not in pp.parts and name not in seen,'unsafe manifest member')
        seen.add(name);target=root/pp
        need(target.is_file() and not target.is_symlink() and digest(target)==h,'manifest mismatch: '+name)
    actual={p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_file() and p.name!='MANIFEST.sha256'}
    need(seen==actual,'manifest coverage mismatch')


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('path',type=Path);p.add_argument('--binary',type=Path);p.add_argument('--source-root',type=Path);p.add_argument('--output',type=Path)
    a=p.parse_args()
    try:
        result=verify(a.path,a.binary,a.source_root);text=json.dumps(result,indent=2)+'\n'
        if a.output:a.output.write_text(text)
        print(text);return 0
    except (OSError,ValueError,KeyError,TypeError,OverflowError,struct.error) as exc:
        print('FAIL: '+str(exc),file=sys.stderr);return 1
if __name__=='__main__':sys.exit(main())
