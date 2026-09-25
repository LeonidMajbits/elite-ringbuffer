#!/usr/bin/env python3
"""Adversarial mutations of a native matrix receipt, plus topology-only oracles.

Pass ELITE_MATRIX_FIXTURE to an independently generated result.json. The retained
small fixture is only a replay fixture, never an admitted performance run.
"""
import copy
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
from verify_matrix_results import verify_record, verify, digest, load, Invalid, geometry, topology, locality, cache_bounds
BASE=Path(os.environ.get('ELITE_MATRIX_FIXTURE',ROOT/'tests/fixtures/matrix-v1/result.json'))
class RecordTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.dir=Path(self.tmp.name)/'trial';shutil.copytree(BASE.parent,self.dir)
        self.path=self.dir/'result.json';self.data=load(self.path)
    def tearDown(self):self.tmp.cleanup()
    def save(self):self.path.write_text(json.dumps(self.data)+'\n')
    def bad(self):
        self.save()
        with self.assertRaises((Invalid,KeyError,OSError,ValueError)):verify_record(self.path)
    def test_valid(self):self.assertEqual(verify_record(self.path)['messages'],self.data['messages'])
    def test_elapsed(self):self.data['elapsed_ns']*=2;self.bad()
    def test_rate(self):self.data['messages_per_second']*=1.01;self.bad()
    def test_bandwidth(self):self.data['delivered_GB_per_second']*=2;self.bad()
    def test_negative_tick(self):self.data['t0']=-1;self.bad()
    def test_boolean_arrival(self):self.data['arrival']=True;self.bad()
    def test_zero_timebase(self):self.data['timebase_denom']=0;self.bad()
    def test_max_integer(self):self.data['end']=2**64;self.bad()
    def test_runtime(self):self.data['runtime']='python_invented';self.bad()
    def test_no_final_tokens(self):self.data['all_tokens_returned']=False;self.bad()
    def test_warmup_reset(self):self.data['warmup_same_generation']=False;self.bad()
    def test_missing_worker(self):self.data['workers_info'].pop();self.bad()
    def test_duplicate_worker(self):self.data['workers_info'][1]['index']=0;self.bad()
    def test_wrong_quantile(self):self.data['native_latency']['nanoseconds_ceiling']['p99']+=1;self.bad()
    def test_wrong_sample_count(self):self.data['offered_latency']['samples']-=1;self.bad()
    def test_wrong_segment(self):self.data['segment_bytes']+=16384;self.bad()
    def test_bad_capacity(self):self.data['capacity']=3;self.bad()
    def test_invented_pe(self):self.data['locality_request']='perf_to_efficiency';self.bad()
    def test_invented_cross_numa(self):self.data['locality_request']='cross_numa';self.bad()
    def test_invented_residency(self):self.data['physical_cache_residency']='IN_L1';self.bad()
    def test_invented_uncertainty(self):self.data['absolute_clock_uncertainty']='ZERO';self.bad()
    def test_affinity_lie(self):
        self.data['workers_info'][0]['placement'].update(requested_cpu=1023,verified_cpu_before=1023,verified_cpu_after=1023,affinity_error=0,affinity_enforced=True);self.bad()
    def test_eligible_scope(self):self.data['hardware_topology']['cpus'][0]['cpu_id']=1023;self.bad()
    def test_synthetic_hardware(self):self.data['hardware_topology']['source_kind']='SYNTHETIC_FIXTURE';self.bad()
    def test_duplicate_json_key(self):
        self.path.write_text('{"schema":"bad",'+self.path.read_text()[1:])
        with self.assertRaises(Invalid):verify_record(self.path)
    def test_nonfinite(self):self.data['elapsed_ns']=float('nan');self.bad()
    def test_truncated_trace(self):
        p=self.dir/'producer-00.trace';p.write_bytes(p.read_bytes()[:-1]);self.bad()
    def test_shifted_offer(self):
        p=self.dir/'producer-00.trace';b=bytearray(p.read_bytes());v=struct.unpack_from('<Q',b,8)[0];struct.pack_into('<Q',b,8,v+1);p.write_bytes(b);self.bad()
    def test_bad_read_timestamp(self):
        for p in self.dir.glob('consumer-*.trace'):
            if p.stat().st_size:
                b=bytearray(p.read_bytes());struct.pack_into('<Q',b,8,0);p.write_bytes(b);break
        self.bad()
    def test_missing_membership(self):
        p=self.dir/'consumer-00.bitmap';p.write_bytes(bytes(p.stat().st_size));self.bad()
    def test_duplicate_membership(self):
        p=self.dir/'consumer-01.bitmap';p.write_bytes((self.dir/'consumer-00.bitmap').read_bytes());self.bad()
    def test_wrong_union(self):
        p=self.dir/'union.bitmap';p.write_bytes(bytes(p.stat().st_size));self.bad()
    def test_retained_phase(self):
        p=self.dir/'final-reconcile.json';r=load(p);r['free_entries'][0]['status_word']|=1;p.write_text(json.dumps(r));self.bad()
    def test_duplicate_free_token(self):
        p=self.dir/'final-reconcile.json';r=load(p);r['free_entries'][1]['block']=r['free_entries'][0]['block'];p.write_text(json.dumps(r));self.bad()
    def test_epoch_mismatch(self):
        p=self.dir/'final-reconcile.json';r=load(p);r['free_entries'][0]['epoch']+=1;p.write_text(json.dumps(r));self.bad()
    def test_cli_under_optimized_python(self):
        self.data['messages_per_second']*=2;self.save()
        result=subprocess.run([sys.executable,'-O',str(ROOT/'tools/verify_matrix_results.py'),str(self.path)],capture_output=True)
        self.assertEqual(result.returncode,1)
    def test_external_binary(self):
        with self.assertRaises(Invalid):verify_record(self.path,sys.executable)
    def test_retention_not_omitted(self):self.data['hold_ns']=1000000000;self.bad()
    def test_fixed_schedule_not_closed_loop(self):self.data['arrival']=0;self.bad()
    def test_geometry(self):
        self.assertEqual(geometry('spsc',1024,64,2),294912)
        self.assertEqual(geometry('ncq',1024,64,16),557056)

class CampaignTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
        self.trial=self.root/'one'/'trial-000';self.trial.parent.mkdir();shutil.copytree(BASE.parent,self.trial)
        x=load(self.trial/'result.json')
        keys=('mode','messages','warmup_messages','producers','consumers','capacity','payload_bytes','arrival','hold_ns','hold_every','period_ns','burst','checksum','runtime','os_yield','locality_request')
        self.cell={k:x[k] for k in keys};self.cell.update(id='one',status='RUN')
        self.plan={'schema':'elite-matrix-plan-v1','trials':1,'cells':[self.cell]}
        self.save()
    def tearDown(self):self.tmp.cleanup()
    def save(self):
        (self.root/'matrix-plan.json').write_text(json.dumps(self.plan))
        (self.root/'MATRIX_COMPLETE.json').write_text(json.dumps({'plan_sha256':digest(self.root/'matrix-plan.json'),'completed_records':1,'skipped_cells':[]}))
    def bad(self):
        with self.assertRaises((Invalid,OSError,KeyError,ValueError)):verify(self.root)
    def test_valid_campaign(self):self.assertEqual(verify(self.root)['records'],1)
    def test_missing_completion(self):(self.root/'MATRIX_COMPLETE.json').unlink();self.bad()
    def test_missing_trial(self):self.plan['trials']=2;self.save();self.bad()
    def test_renamed_trial(self):self.trial.rename(self.trial.parent/'trial-007');self.bad()
    def test_duplicate_plan(self):self.plan['cells'].append(self.cell);self.save();self.bad()
    def test_changed_plan_workload(self):self.cell['payload_bytes']=128;self.save();self.bad()
    def test_skip_without_reason(self):self.cell['status']='SKIP_UNSUPPORTED';self.save();self.bad()
    def test_bad_plan_hash(self):(self.root/'matrix-plan.json').write_text(json.dumps({**self.plan,'trials':2}));self.bad()
    def test_invented_completed_count(self):
        p=self.root/'MATRIX_COMPLETE.json';x=load(p);x['completed_records']=2;p.write_text(json.dumps(x));self.bad()
    def test_manifest_guards_inventory(self):
        paths=sorted(p for p in self.root.rglob('*') if p.is_file())
        (self.root/'MANIFEST.sha256').write_text(''.join(digest(p)+'  '+p.relative_to(self.root).as_posix()+'\n' for p in paths))
        self.assertEqual(verify(self.root)['records'],1)
        (self.root/'unlisted.txt').write_text('altered evidence');self.bad()

if __name__=='__main__':unittest.main(verbosity=2)
