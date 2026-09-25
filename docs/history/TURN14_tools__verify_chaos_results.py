#!/usr/bin/env python3
"""Replay native owned-process chaos receipts. No third-party dependencies.

The oracle checks a finite trace and stable final inventory, NOT all future
executions, literal physical fencing from JSON alone, or exactly-once effects.
No assertion statements: optimization cannot disable validation.
"""
from __future__ import annotations
import argparse
import binascii
import hashlib
import json
import os
from pathlib import Path
import re
import sys
sys.dont_write_bytecode = True

HOOKS = {'write_claim':1, 'write_reserved':2, 'write_committed':3,
         'write_published':5, 'read_claim':6, 'read_returning':7,
         'read_returned':8, 'late_publication':4}
CALLER = {'write_partial':100, 'read_held':101, 'resume_write':100, 'resume_read':101, 'async_resume_write':100}
SCENARIOS = set(HOOKS) | set(CALLER) | {'storm', 'storm_restart', 'random_abort_kill'}
U64 = (1 << 64) - 1

def need(condition, text):
    if not condition:
        raise ValueError(text)

def integer(x, lo=0, hi=U64):
    need(type(x) is int and lo <= x <= hi, f'invalid integer {x!r}')
    return x

def object_pairs(pairs):
    out = {}
    for k, v in pairs:
        need(k not in out, 'duplicate JSON key: '+k)
        out[k] = v
    return out

def loads(text):
    return json.loads(text, object_pairs_hook=object_pairs,
                      parse_constant=lambda s: (_ for _ in ()).throw(ValueError('nonfinite: '+s)))

def sha(path):
    with Path(path).open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()

def prefix(record):
    s = record['immutable_prefix_hex']
    need(type(s) is str and re.fullmatch('[0-9a-f]{1024}', s), 'bad header evidence')
    b = bytes.fromhex(s)
    need(b[:8] == b'ELITEIPC', 'bad magic')
    need(int.from_bytes(b[8:12], 'little') == 0x10000, 'wrong wire ABI')
    crc = int.from_bytes(b[52:56], 'little')
    need(binascii.crc32(b[:52]+bytes(4)+b[56:]) == crc, 'immutable header CRC')
    need(int.from_bytes(b[64:72], 'little') == 64, 'capacity/header mismatch')
    need(int.from_bytes(b[32:36], 'little') == 128, 'isolation/header mismatch')
    return b, crc

def queue_members(head, tail, entries, n):
    integer(head, 0, U64-n-1);integer(tail, 0, U64-n-1)
    need(type(entries) is list and len(entries) == n, 'entry count')
    for x in entries:integer(x)
    frontier = tail + int(entries[tail % n] // n == tail // n)
    need(head <= frontier and frontier-head <= n and frontier-1 <= tail <= frontier,
         'NCQ publication frontier')
    members = []
    for ticket in range(head, frontier):
        entry = entries[ticket % n]
        need(entry // n == ticket // n, 'wrong live cycle')
        members.append(entry % n)
    need(len(set(members)) == len(members), 'duplicate queue token')
    return set(members), frontier

def verify_resource(record):
    need(record == {'schema':'elite-chaos-resource-v1',
        'scope':'single_process_authority_and_live_lease_negative', 'objects':4,
        'quarantines':2, 'third_quarantine_status':4, 'fifth_create_status':4,
        'premature_authority_destroy_status':4, 'retained_bytes_unchanged':True,
        'cleanup_complete':True}, 'resource negative contract')
    need(record['retained_bytes_unchanged'] is True and record['cleanup_complete'] is True,'resource boolean fields')
    # Avoid bool == int accepting malformed numeric fields.
    for key in ('objects','quarantines','third_quarantine_status','fifth_create_status',
                'premature_authority_destroy_status'):integer(record[key])
    return {'resource_negative':'PASS_WITHIN_SCOPE','scope':record['scope']}

def verify_events(events):
    need(type(events) is list and len(events) >= 3, 'missing trace')
    plan = events[0]
    need(plan.get('event') == 'plan' and plan.get('schema') == 'elite-chaos-v1', 'bad plan')
    scenario = plan['scenario'];need(scenario in SCENARIOS, 'unknown scenario')
    mode = integer(plan['mode'],1,2);count = integer(plan['messages'],1,4_000_000)
    hold = integer(plan['hold_ms'],1,1000);p = integer(plan['producers'],1,16);c = integer(plan['consumers'],1,16)
    need(plan['clock'] == 'CLOCK_MONOTONIC' and plan['timebase_numer'] == 1 and plan['timebase_denom'] == 1, 'timebase')
    integer(plan['reported_resolution_ns'],1)
    need(type(plan['hooks']) is bool, 'hook tag')
    if scenario in HOOKS:need(plan['hooks'] and mode == 2, 'required internal hook absent')
    else:need(not plan['hooks'], 'caller/no-hook lane mislabeled')
    storm = scenario in {'storm','storm_restart'}
    asynchronous = scenario == 'async_resume_write'
    resume = scenario in {'resume_write','resume_read','late_publication','async_resume_write'}
    random = scenario == 'random_abort_kill'
    read_victim = scenario.startswith('read_') or scenario == 'resume_read'
    victim = p if read_victim else 0
    if mode==1:need(p==c==1 and ((scenario in CALLER and not asynchronous) or storm), 'SPSC role contract')
    elif not storm:need((p,c) == ((2,3) if read_victim else (3,2)), 'controlled peer population')
    gs={};ws={};commands={};done={};bitmaps={};cohorts={};exits={};snaps={};destroyed=set();signals=[]
    stop_checks=[];suspicion=None;cut=None;quarantine=None;held=None;released=None;blocked=None;victim_done=None
    previous=0;terminal=None;last_event={};sum_messages=0;pending_messages={}
    for seq,e in enumerate(events,1):
        need(type(e) is dict,'record not object');need(integer(e['log_sequence'],1)==seq,'observer sequence gap')
        t=integer(e['observer_tick']);need(t>=previous,'observer time reversed');previous=t
        kind=e['event']
        if kind=='plan':need(seq==1,'repeated plan')
        elif kind=='generation':
            gi=integer(e['generation_id'],0,1);need(gi not in gs,'duplicate generation')
            need(gi==len(gs),'generation sequence');need(e['mode']==mode and e['capacity']==64,'generation shape')
            need((e['producers'],e['consumers'])==((p,c) if gi==0 else (1,1)), 'generation population')
            b,_=prefix(e);need(int.from_bytes(b[24:28],'little')==mode,'header profile')
            need(int.from_bytes(b[256:260],'little')==e['producers'] and int.from_bytes(b[260:264],'little')==e['consumers'], 'header roles')
            need(int.from_bytes(b[56:64],'little')==integer(e['bytes'],16384),'backing length')
            need(type(e['name']) is str and re.fullmatch('/el-[a-z2-7]{26}', e['name']), 'shm name')
            need(integer(e['observer_address'],1)%128==0,'mapping alignment')
            if gi:
                need(quarantine is not None, 'successor before quarantine')
                need(gs[0]['name']!=e['name'] and gs[0]['observer_address']!=e['observer_address'],'old capability redirection')
                need(bytes.fromhex(gs[0]['immutable_prefix_hex'])[160:176]!=b[160:176],'reused session identity')
                need(b[216:232]==bytes.fromhex(gs[0]['immutable_prefix_hex'])[160:176],'predecessor identity')
            gs[gi]=e
        elif kind=='worker_event':
            wid=integer(e['worker_id'],0,63);gi=wid//32;need(gi in gs,'worker without generation')
            need(wid%32 < gs[gi]['producers']+gs[gi]['consumers'],'worker range')
            v=e['values'];need(type(v) is list and len(v)==8,'worker event frame')
            for x in v:integer(x)
            tick=integer(e['tick']);rx=integer(e['received_tick']);need(tick<=rx<=t,'worker clock order')
            integer(e['sequence'],1)
            old=last_event.get(wid);need(e['sequence']==(old['sequence']+1 if old else 1),'worker sequence')
            if old:need(tick>=old['tick'],'worker time reverse')
            last_event[wid]=e
            ek=integer(e['kind'],1,10)
            if ek==1:
                need(wid not in ws and e['sequence']==1,'duplicate/missing READY')
                need(v[0]>0 and v[0]%128==0 and v[1]==gs[gi]['bytes'],'worker independent mapping')
                need(v[2]==integer(e['pid'],1),'child PID identity');ws[wid]=e
            else:need(wid in ws and e['pid']==ws[wid]['pid'] and wid not in exits,'invalid worker lifetime')
            if ek==3:
                need(wid not in done and wid in commands and commands[wid]['command']==1,'unexpected DONE')
                need(v[3]<=tick,'work-start after completion');done[wid]=e
                if wid%32<gs[gi]['producers']:need((v[0]<=commands[wid]['quota'] if asynchronous and gi==0 else v[0]==commands[wid]['quota']) and v[1]==v[2]==0,'producer quota')
                else:need(v[2]==(count+7)//8 and v[0]<=count and v[1]<=1,'consumer completion')
                integer(v[7],0,64)
                if not(asynchronous and gi==0):need(v[7]==0,'unexpected retained token')
            elif ek==4:
                need(wid==victim and cut is None and not random,'invalid cut witness');cut=e
                need(v[0]==(HOOKS.get(scenario) or CALLER[scenario]) and v[2]<64,'wrong cut point')
            elif ek==5:
                need(resume and wid==victim and held is not None,'invalid resumed transfer');victim_done=e
                expected=(3,5) if scenario in {'resume_write','resume_read','async_resume_write'} else (0,2)
                need(tuple(v[:2])==expected,'resumed transfer status/outcome')
            elif ek==7:need(commands.get(wid,{}).get('command')==4,'detach not requested')
            elif ek==8:
                need(gi==1 and commands.get(wid,{}).get('command')==6,'unexpected successor hold');held=e
            elif ek==9:
                need(gi==1 and held is not None and (not resume or victim_done is not None),'premature successor recheck');released=e
            elif ek==10:need(commands.get(wid,{}).get('command')==1,'unrequested workload start')
        elif kind=='command':
            wid=integer(e['worker_id'],0,63);need(wid in ws and wid not in exits,'command to unknown/dead child')
            command=integer(e['command'],1,8)
            for k in ('first','quota','total'):integer(e[k])
            if command==1:
                need(e['total']==count and wid not in done,'work size/repeated workload')
                need(e['allow_retirement'] is (asynchronous and wid//32==0),'retirement mode mismatch');commands[wid]=e
            elif command==5:
                need(wid%32 >= gs[wid//32]['producers'],'drain to producer')
                # Drain does not replace the original quota record.
                prods=[x for x in commands if x//32==wid//32 and x%32<gs[wid//32]['producers'] and commands[x]['command']==1]
                need(prods and all(x in done for x in prods),'drain before producer completion')
            else:commands[wid]=e
        elif kind=='membership':
            wid=integer(e['worker_id']);need(wid in done and wid not in bitmaps,'unexpected bitmap')
            need(type(e['hex']) is str and re.fullmatch('[0-9a-f]*',e['hex']),'bitmap encoding')
            raw=bytes.fromhex(e['hex']);need(len(raw)==done[wid]['values'][2],'bitmap length')
            if raw:need(int.from_bytes(raw,'little').bit_count()==done[wid]['values'][0],'bitmap population')
            bitmaps[wid]=raw
        elif kind=='signal_sent':
            wid=integer(e['worker_id']);need(wid in ws and wid not in exits,'signal to unowned/terminal PID')
            need(e['signal'] in {'SIGKILL','SIGSTOP','SIGCONT','SIGUSR1','SIGALRM'},'unknown signal')
            integer(e['signal_number'],1,127);signals.append(e)
        elif kind=='stopped_observed':
            wid=integer(e['worker_id']);need(wid in ws and wid not in exits,'stop lifetime')
            need(os.WIFSTOPPED(integer(e['wait_status'],0,65535)), 'not stop wait status')
            need((cut is not None and wid==victim) or any(s['worker_id']==wid and s['signal']=='SIGSTOP' for s in signals),'unrequested stop')
        elif kind=='stop_not_death':need(e['status']==8 and e['worker_id'] in ws,'stop treated as death');stop_checks.append(e)
        elif kind=='heartbeat_overdue':
            need(not storm and suspicion is None and e['worker_id']==victim,'wrong overdue owner')
            le=last_event[victim];need(e['last_sequence']==le['sequence'] and e['last_received_tick']==le['received_tick'],'heartbeat custody')
            need(e['deadline_tick']==e['last_received_tick']+hold*1_000_000 and t>=e['deadline_tick'],'premature timeout')
            need(e['death_inferred'] is False and e['slot_reclaimed'] is False,'timeout theft');suspicion=e
        elif kind=='terminal_observed':
            wid=e['worker_id'];need(wid==victim and not resume and wid not in exits,'unexpected terminal victim')
            need(any(s['worker_id']==wid and s['signal']=='SIGKILL' for s in signals),'no kill receipt')
            need(e['status']==0 and e['signal']=='SIGKILL' and os.WIFSIGNALED(e['wait_status']),'bad terminal evidence')
            need(os.WTERMSIG(e['wait_status'])==next(s['signal_number'] for s in signals if s['signal']=='SIGKILL'),'terminal signal mismatch')
            exits[wid]=e;terminal=e
            if mode==2:need(0 in cohorts,'failure notification before required peer-progress witness')
        elif kind=='clean_exit':
            wid=e['worker_id'];need(wid not in exits and last_event[wid]['kind']==7,'no detached receipt')
            need(e['wait_status']==0,'nonzero exit');exits[wid]=e
        elif kind=='quarantined':
            need(not storm and e['generation_id']==0 and quarantine is None,'quarantine sequence')
            need(e['tokens_retained_by_generation']==64 and suspicion is not None,'lost quarantine capacity')
            if not resume:need(terminal is not None,'kill quarantine before terminal notification')
            quarantine=e
        elif kind=='unfenced_destroy_blocked':need(resume and e['status']==4 and held is None,'unsafe early destroy');blocked=e
        elif kind=='cohort_reconciled':
            gi=e['generation_id'];need(gi in gs and gi not in cohorts,'duplicate cohort')
            members=[wid for wid in done if wid//32==gi];expected={32*gi+i for i in range(gs[gi]['producers']+gs[gi]['consumers'])}
            if gi==0 and not storm:expected.remove(victim)
            need(set(members)==expected and expected <= bitmaps.keys(),'missing completed endpoint')
            union=0;sent=0;received=0;special=0;starts=[];published=0
            for wid in members:
                v=done[wid]['values']
                if wid%32<gs[gi]['producers']:
                    sent+=v[0];starts.append((commands[wid]['first'],commands[wid]['quota']))
                    published|=((1<<v[0])-1)<<commands[wid]['first']
                else:
                    b=int.from_bytes(bitmaps[wid],'little');need(union&b==0,'cross-consumer duplicate');union|=b
                    received+=v[0];special+=v[1]
            pos=0
            for first,quota in sorted(starts):need(first==pos,'producer identity range gap');pos+=quota
            need(pos==count,'assigned offer quota')
            need(e['retired_during_work'] is (asynchronous and gi==0),'false async mode')
            if asynchronous and gi==0:
                need(quarantine is not None and received<=sent<=count and union&~published==0,'async delivery not committed')
                pending_messages[gi]=published^union
            else:need(sent==received==count and union==(1<<count)-1,'message set mismatch')
            need(e['sent']==sent and e['received']==received and e['sentinel_received']==special,'cohort scalar mismatch')
            need(special==(1 if gi==0 and scenario=='write_published' else 0),'unexpected/uncommitted sentinel delivery')
            if storm:
                need(e['storm_rounds']>0 and stop_checks,'storm was not applied')
                need(all(any(s['signal']==name for s in signals) for name in ('SIGUSR1','SIGALRM','SIGSTOP','SIGCONT')),'missing signal class')
                need(any(done[w]['values'][4] for w in members) and any(done[w]['values'][5] for w in members),'nonfatal signals never observed')
            cohorts[gi]=e;sum_messages+=received
        elif kind=='snapshot':
            gi=e['generation_id'];need(gi in gs and gi not in snaps,'snapshot identity')
            need(all(32*gi+i in exits for i in range(gs[gi]['producers']+gs[gi]['consumers'])),'snapshot before quiescence/fencing')
            need(e['scope']=='ALL_WORKERS_TERMINAL' and e['capacity']==64,'snapshot authority')
            b,crc=prefix(e);need(b==bytes.fromhex(gs[gi]['immutable_prefix_hex']) and e['header_crc32']==crc,'header corruption')
            gate=integer(e['admission']);need(gate&0xffffff00==0 and gate>>32 <= gs[gi]['producers']+gs[gi]['consumers'],'bad admission count/bits')
            need((gate&255)==(4 if gi==0 and not storm else 1),'wrong lifecycle state')
            integer(e['failure'],0,6)
            need(e['failure']==(3 if gi==0 and not storm and not resume else 0),'unexpected failure word')
            sts=e['status_words'];eps=e['epochs'];need(len(sts)==len(eps)==64,'descriptor count')
            for s,ep in zip(sts,eps):integer(s);integer(ep,0,(1<<62)-2)
            ids=e['message_ids'];need(len(ids)==64,'descriptor identity count')
            for x in ids:integer(x)
            if mode==2:
                need(len(e['cursors'])==4,'NCQ cursor width')
                f,uf=queue_members(*e['cursors'][:2],e['qf_entries'],64)
                q,ur=queue_members(*e['cursors'][2:],e['qr_entries'],64)
                need(not f&q,'free/ready double allocation');missing=set(range(64))-f-q
                for block in f:need(sts[block]&3==0 and sts[block]>>2==eps[block],'free phase/epoch')
                for block in q:need(sts[block]&3==2 and sts[block]>>2==eps[block],'ready phase/epoch')
                expected_missing = 0 if gi or storm or scenario in {'write_published','read_returned','late_publication'} else 1
                if gi==0 and asynchronous:
                    losses=[v['values'][7]-1 for wid,v in done.items() if wid//32==0 and v['values'][7]]
                    need(len(losses)==len(set(losses)) and cut['values'][2] not in losses,'duplicate retained ownership')
                    need(missing==set(losses)|{cut['values'][2]},'unaccounted async token')
                elif gi==0 and random:need(len(missing) in (0,1),'random loss exceeds one outstanding token')
                else:need(len(missing)==expected_missing,'unexpected orphan count')
                if missing and cut and not asynchronous:need(missing=={cut['values'][2]},'unaccounted orphan identity')
                if gi==0 and cut and missing:
                    expected_phase={'write_claim':0,'write_reserved':1,'write_committed':2,
                        'write_partial':1,'resume_write':1,'async_resume_write':1,
                        'read_claim':2,'read_held':3,'resume_read':3,'read_returning':0}[scenario]
                    block=cut['values'][2];expected_epoch=0 if scenario=='write_claim' else 1
                    need(sts[block]==(expected_epoch<<2)|expected_phase and eps[block]==expected_epoch,'orphan phase/epoch changed')
                if gi==0 and asynchronous:
                    for wid,rec in done.items():
                        if wid//32==0 and rec['values'][7]:
                            block=rec['values'][7]-1;phase=1 if wid%32<p else 3
                            need(sts[block]&3==phase and sts[block]>>2==eps[block],'cooperative retained phase')
                if gi==0 and random:
                    for block in missing:
                        need(sts[block]&3 in (0,1) and sts[block]>>2 <= (1<<62)-2 and
                             eps[block]-(sts[block]>>2) in (0,1),'random aborted transfer state')
                if gi==0 and asynchronous:
                    qids=[ids[b] for b in q];need(len(set(qids))==len(qids) and all(v<count for v in qids),'duplicate/invalid pending ID')
                    ready_bits=sum(1<<v for v in qids)
                    need(ready_bits==pending_messages.get(gi),'unaccounted committed message')
                else:
                    need(len(q)==(1 if gi==0 and scenario=='late_publication' else 0),'unexpected old publication membership')
                    if q and cut:need(q=={cut['values'][2]},'late LP wrong token')
                e={**e,'replayed_partition':{'free':sorted(f),'ready':sorted(q),'unavailable_orphan':sorted(missing),'qf_frontier':uf,'qr_frontier':ur}}
            else:
                need(len(e['cursors'])==2 and all(s==0 for s in sts),'SPSC dormant status')
                pub,ret=(integer(v) for v in e['cursors']);need(0<=pub-ret<=64,'SPSC capacity')
                expected=(count,count) if storm else ((count+1,count+1) if gi else ((1,0) if read_victim else (0,0)))
                need((pub,ret)==expected,'SPSC published/reclaimed outcome')
            snaps[gi]=e
        elif kind=='destroyed':need(e['generation_id'] in snaps and e['name_absent'] is True and e['generation_id'] not in destroyed,'unsafe/double destroy');destroyed.add(e['generation_id'])
        elif kind=='random_cut_interval_unknown':need(random,'false random cut')
        elif kind=='spsc_old_service_unavailable_no_role_replacement':need(mode==1 and not storm,'false SPSC service')
        elif kind=='successor_holds_validated_sentinel':need(held is not None,'missing native hold')
        elif kind=='successor_sentinel_unchanged':need(released is not None,'missing native recheck')
        elif kind=='complete':
            need(seq==len(events) and e['status']=='PASS_WITHIN_SCOPE' and e['all_children_reaped'] is True and e['all_owned_names_absent'] is True,'invalid completion')
        else:raise ValueError('unknown event: '+str(kind))
    need(events[-1]['event']=='complete','missing completion')
    need(len(gs)==(1 if storm else 2) and set(snaps)==set(gs)==destroyed,'incomplete generations')
    need(set(exits)==set(ws) and len(ws)==p+c+(0 if storm else 2),'holder accounting')
    need(set(cohorts)==({0} if storm else ({1} if mode==1 else {0,1})),'missing workload generation')
    if not storm:
        need(suspicion and quarantine and released,'recovery obligations absent')
        if resume:need(blocked and victim_done and terminal is None,'false-suspicion witness absent')
        else:need(terminal,'unconfirmed death')
        if not random:need(cut and stop_checks,'controlled cut not confirmed')
    recovery=None
    if not storm:
        start=terminal['observer_tick'] if terminal else suspicion['observer_tick']
        attach=max(ws[x]['received_tick'] for x in ws if x//32==1)
        recovery={'basis':'terminal_notification' if terminal else 'false_suspicion',
                  'successor_all_attached_ns':attach-start,
                  'successor_first_validated_read_ns':held['received_tick']-start}
    return {'status':'PASS_WITHIN_SCOPE','scenario':scenario,'mode':mode,'messages':sum_messages,
            'generations':len(gs),'worker_processes':len(ws),'signals_sent':len(signals),
            'recovery':recovery,'inventories':{str(k):v.get('replayed_partition',{'spsc_cursors':v['cursors']}) for k,v in snaps.items()}}

def verify_file(path):
    p=Path(path);need(p.stat().st_size<=128*1024*1024,'oversized trace')
    records=[loads(line) for line in p.read_text().splitlines() if line.strip()]
    if len(records)==1 and records[0].get('schema')=='elite-chaos-resource-v1':return verify_resource(records[0])
    return verify_events(records)

SOURCE_INPUTS = (
    'tests/test_chaos_multiprocess.c', 'tests/chaos_protocol.h',
    'tests/test_chaos_resources.c', 'tests/test_support.h',
    'tools/run_chaos.py', 'tools/verify_chaos_results.py',
    'src/elite_spsc.c', 'src/elite_mpmc_ncq.c', 'src/elite_core.c',
    'src/elite_shm.c', 'src/elite_format.c', 'src/elite_wait.c',
    'src/elite_version.c', 'src/elite_internal.h',
    'include/elite_ringbuffer.h', 'include/elite_api.h', 'include/elite_version.h',
    'Makefile')

def campaign_matrix(profile, trials, messages, storm_messages):
    need(profile in ('smoke','standard'), 'unknown campaign profile')
    integer(trials,1,1000);integer(messages,2,4_000_000);integer(storm_messages,16,4_000_000)
    need(messages%2==0 and storm_messages%16==0,'quota divisibility')
    configs=[]
    for scenario in HOOKS:
        configs.append((scenario,'ncq',1,1,'chaos_multiprocess_hooks',messages))
    for mode in ('ncq','spsc'):
        for scenario in CALLER:
            if scenario=='async_resume_write':
                if mode=='ncq':configs.append((scenario,mode,1,1,'chaos_multiprocess',1000000))
            else:configs.append((scenario,mode,1,1,'chaos_multiprocess',messages))
    configs.append(('random_abort_kill','ncq',1,1,'chaos_multiprocess',messages))
    for scenario in ('storm','storm_restart'):
        for mode,p,c in [('spsc',1,1),('ncq',1,1),('ncq',4,4),('ncq',16,1),('ncq',1,16),('ncq',16,16)]:
            configs.append((scenario,mode,p,c,'chaos_multiprocess',storm_messages))
    if profile=='smoke':
        configs=[c for c in configs if c[0] in ('write_claim','read_claim','resume_write','resume_read','late_publication','random_abort_kill') or c[:4]==('storm','ncq',4,4)]
    out=[]
    for trial in range(trials):
        hold=(1,10,100)[trial%3]
        for scenario,mode,p,c,binary,count in configs:
            name=f'{scenario}_{mode}_{p}p{c}c_t{trial:03d}'
            out.append({'id':name,'kind':'data','binary':binary,
                        'args':[scenario,mode,str(count),str(hold),str(p),str(c)]})
        out.append({'id':f'resources_t{trial:03d}','kind':'resource','binary':'chaos_resources','args':[]})
    return out

def verify_campaign(root, source_root=None, build=None):
    root=Path(root);plan=loads((root/'plan.json').read_text());result=loads((root/'COMPLETE.json').read_text())
    need(plan['schema']=='elite-chaos-campaign-v1' and result['status']=='COMPLETE','incomplete campaign')
    need(result['plan_sha256']==sha(root/'plan.json'),'changed plan')
    canonical=campaign_matrix(plan['profile'],plan['trials'],plan['messages'],plan['storm_messages'])
    need(len(canonical)==len(plan['cases']),'changed campaign population')
    for wanted,actual in zip(canonical,plan['cases']):
        need(all(actual[k]==v for k,v in wanted.items()),'changed predeclared case')
        need(Path(actual['command'][0]).name==wanted['binary'] and actual['command'][1:]==wanted['args'],'command workload substitution')
    need(set(plan['source_hashes'])==set(SOURCE_INPUTS),'missing source identity')
    need(set(plan['binaries'])=={'chaos_multiprocess','chaos_multiprocess_hooks','chaos_resources'},'missing binary identity')
    expected=[x['id'] for x in plan['cases']];need(len(expected)==len(set(expected)),'duplicate case identity')
    need(result['cases']==expected,'missing/extra complete case')
    reports=[]
    for case in plan['cases']:
        identity=case['id'];need(re.fullmatch('[a-z0-9_-]+',identity),'unsafe case path')
        run=loads((root/(identity+'.run.json')).read_text())
        need(run['returncode']==0 and run['timed_out'] is False,'case did not complete')
        need(run['command']==case['command'],'changed command')
        trace=root/(identity+'.jsonl');err=root/(identity+'.stderr')
        need(sha(trace)==run['trace_sha256'] and sha(err)==run['stderr_sha256'],'trace hash')
        need(err.stat().st_size==0,'unexpected native diagnostics')
        proof=verify_file(trace)
        if case['kind']=='data':
            p=loads(trace.read_text().splitlines()[0]);cmd=case['command']
            need(p['scenario']==cmd[1] and p['mode']==({'spsc':1,'ncq':2}[cmd[2]]) and p['messages']==int(cmd[3]) and p['hold_ms']==int(cmd[4]),'trace disagrees with frozen workload')
        reports.append(proof)
    if source_root is not None:
        for name,h in plan['source_hashes'].items():
            rel=Path(name);need(not rel.is_absolute() and '..' not in rel.parts,'unsafe source path')
            need(sha(Path(source_root)/name)==h,'source drift: '+name)
    if build is not None:
        for name,h in plan['binaries'].items():need(sha(Path(build)/name)==h,'binary drift: '+name)
    return {'status':'PASS_WITHIN_SCOPE','cases':len(reports),'measured_messages':sum(r.get('messages',0) for r in reports),'reports':reports}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('path');p.add_argument('--source-root');p.add_argument('--build');p.add_argument('--out');a=p.parse_args()
    out=verify_campaign(a.path,a.source_root,a.build) if Path(a.path).is_dir() else verify_file(a.path)
    if a.out:Path(a.out).write_text(json.dumps(out,indent=2)+'\n')
    print(json.dumps({k:v for k,v in out.items() if k not in {'reports','inventories'}}))
    return 0
if __name__=='__main__':
    try:raise SystemExit(main())
    except (ValueError,OSError,KeyError,TypeError,IndexError,OverflowError) as exc:
        print('FAIL:',exc,file=sys.stderr);raise SystemExit(1)
