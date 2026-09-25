"""Independent synthetic receipt mutations; none are hardware measurements."""
from __future__ import annotations
import copy
from fractions import Fraction
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('cache_verifier', ROOT/'tools/verify_cache_provenance.py')
v = importlib.util.module_from_spec(spec)
spec.loader.exec_module(v)


def fixture():
    sha = hashlib.sha256(b'synthetic').hexdigest()
    cpu = dict(error=0,user_us=0,system_us=0,minor_faults=0,major_faults=0,
               voluntary_switches=0,involuntary_switches=0,instant_usage=0,instant_usage_scale=0)
    place = dict(requested_cpu=-1, verified_cpu_before=-1, verified_cpu_after=-1,
                 affinity_enforced=False,affinity_error=0,darwin_affinity_tag=0,
                 qos_request=0,p_core_pinning='NOT_ESTABLISHED')
    t0 = (1 << 53) + 1
    workers = []
    for i in range(2):
        groups = []
        for g in range(3):
            groups.append(dict(group_id=g,status='UNSUPPORTED_EVENT',os_error=2,
                               before=None,after=None,time_enabled_ns=0,time_running_ns=0,
                               events=[dict(name=v.EVENTS[2*g+j],type=0 if g==0 else 3,
                                            config=v.CONFIGS[2*g+j],id=0,raw_delta=None,
                                            scaled_estimate=None) for j in range(2)]))
        start,end=t0+10+i,t0+100+i
        workers.append(dict(worker_id=i,requested_cpu=-1,verified_cpu=-1,affinity_tag=0,qos=0,
                            counter_offset=i*8,start_tick=start,end_tick=end,worker_delta_ticks=end-start,
                            window_start_tick=t0+i,window_end_tick=end+10,final_counter_value=1000,
                            placement=copy.deepcopy(place),cpu_before=copy.deepcopy(cpu),
                            cpu_after=copy.deepcopy(cpu),cpu_utilization_percent=0,
                            virtual_before=None,virtual_after=None,
                            pmc=dict(requested=True,owner_thread_id=10+i,paranoid=2,sampling=False,
                                     scope='calling_thread_user_only_no_inherit',groups=groups)))
    return dict(synthetic_fixture=True,schema='elite-cache-control-v2',status='PASS_COUNTER_RECONCILIATION_ONLY',
                ipc_measurement=False,sampling_interrupts_enabled=False,
                interval='scheduled_t0_to_latest_worker_rmw_end',warmup_iterations=0,workers=2,
                iterations_per_worker=1000,total_operations=2000,counter_stride=8,allocation_alignment=128,
                trial=0,layout_order=0,t0=t0,end=t0+101,delta_ticks=101,timebase_numer=1,timebase_denom=1,
                nominal_tick_ns=1,elapsed_ns=101,operations_per_second=float(Fraction(2000*10**9,101)),
                platform='Linux',clock='CLOCK_MONOTONIC_RAW',reported_resolution_ns=1,pmc_mode='count',
                timebase_source='nanosecond_OS_clock',resolution_status='clock_getres_NOT_effective_resolution',
                timestamp_ordering='inherited_ordered_clock_boundaries',
                pmc_status='UNSUPPORTED_EVENT',binary=dict(path='/synthetic/not-executed',sha256=sha,
                                                        bytes=9,rehash_after_run=True),
                build=dict(compiler_path='/synthetic/compiler',compiler_version='fixture',compiler_sha256=sha,
                           cflags='-std=c11 -O3',cppflags='-Iinclude',ldflags='',ldlibs='',git_commit=None,
                           source_files={'synthetic.c':sha},
                           source_sha256=hashlib.sha256(f'{sha}  synthetic.c\n'.encode()).hexdigest()),
                virtual_timer_probe=dict(status='NOT_REQUESTED',kind='SYSTEM_TIMER_NOT_CPU_CYCLES',error=0,
                                         signal=0,cntfrq_hz=0,sysctl_tbfrequency_hz=0,sysctl_error=0,
                                         first=dict(mach_before=0,count=0,mach_after=0),
                                         last=dict(mach_before=0,count=0,mach_after=0)),
                per_worker=workers,
                metrics=dict(instructions_per_cycle=None,l1d_read_misses_per_million_rmw=None,
                             llc_read_misses_per_million_rmw=None,l1d_read_miss_fraction=None,
                             llc_read_miss_fraction=None,coherence_invalidations=None,
                             coherence_status='UNSUPPORTED_GENERIC_EVENT',causal_attribution='NOT_ESTABLISHED'),
                process_envelope=dict(start_tick=t0-100,end_tick=t0+200,cpu_before=cpu,cpu_after=copy.deepcopy(cpu),
                                      os_accounted_before=dict(error=95,cycles=0,instructions=0),
                                      os_accounted_after=dict(error=95,cycles=0,instructions=0)))


def measured(d, running=200):
    for w in d['per_worker']:
        for i,g in enumerate(w['pmc']['groups']):
            a,b=((200,600),(1000,50),(500,20))[i]
            g.update(status='OK' if running==200 else 'MULTIPLEXED' if running else 'NEVER_SCHEDULED',
                     os_error=0,before=[2,0,0,0,11,0,22],after=[2,200,running,b if running else 0,22,a if running else 0,11],
                     time_enabled_ns=200,time_running_ns=running)
            for j,e in enumerate(g['events']):
                e.update(id=11 if j==0 else 22, raw_delta=(a,b)[j] if running else None,
                         scaled_estimate=float(Fraction((a,b)[j]*200,running)) if running else None)
    d['pmc_status']='COLLECTED' if running else 'NEVER_SCHEDULED'
    if running:
        d['metrics'].update(instructions_per_cycle=3,
                            l1d_read_misses_per_million_rmw=float(Fraction(100*200*10**6,2000*running)),
                            llc_read_misses_per_million_rmw=float(Fraction(40*200*10**6,2000*running)),
                            l1d_read_miss_fraction=.05,llc_read_miss_fraction=.04)
    return d


class ProvenanceTests(unittest.TestCase):
    def bad(self, mutate):
        d=fixture();mutate(d)
        with self.assertRaises((ValueError,KeyError,TypeError)):
            v.verify_record(d)
    def test_raw_u64_subtraction_above_binary64_precision(self):
        self.assertEqual(v.verify_record(fixture())['elapsed_ns'],101)
    def test_elapsed_mismatch(self): self.bad(lambda d:d.update(elapsed_ns=100))
    def test_rate_mismatch(self): self.bad(lambda d:d.update(operations_per_second=1))
    def test_t0_wrap(self): self.bad(lambda d:d.update(end=d['t0']-1))
    def test_max_worker_end(self): self.bad(lambda d:d['per_worker'][1].update(end_tick=d['end']+1,worker_delta_ticks=91))
    def test_worker_delta(self): self.bad(lambda d:d['per_worker'][0].update(worker_delta_ticks=1))
    def test_duplicate_worker_thread(self):
        self.bad(lambda d:d['per_worker'][1]['pmc'].update(owner_thread_id=d['per_worker'][0]['pmc']['owner_thread_id']))
    def test_counter_loss(self): self.bad(lambda d:d['per_worker'][0].update(final_counter_value=999))
    def test_counter_duplicate(self): self.bad(lambda d:d['per_worker'][0].update(final_counter_value=1001))
    def test_missing_worker(self): self.bad(lambda d:d['per_worker'].pop())
    def test_total_operations(self): self.bad(lambda d:d.update(total_operations=1000))
    def test_bool_is_not_integer(self): self.bad(lambda d:d.update(workers=True))
    def test_zero_timebase(self): self.bad(lambda d:d.update(timebase_denom=0))
    def test_no_nan(self): self.bad(lambda d:d.update(elapsed_ns=float('nan')))
    def test_no_infinity(self): self.bad(lambda d:d.update(operations_per_second=float('inf')))
    def test_one_ulp_admitted(self):
        d=fixture();d['elapsed_ns']=math.nextafter(101.,math.inf);v.verify_record(d)
    def test_two_ulp_rejected(self):
        self.bad(lambda d:d.update(elapsed_ns=math.nextafter(math.nextafter(101.,math.inf),math.inf)))
    def test_fractional_mach_timebase(self):
        d=fixture();d.update(platform='Darwin',clock='mach_absolute_time',reported_resolution_ns=None,
                            timebase_numer=125,timebase_denom=3,nominal_tick_ns=125/3,
                            timebase_source='mach_timebase_info',resolution_status='NOT_REPORTED_FOR_THIS_CLOCK',
                            elapsed_ns=float(Fraction(101*125,3)),
                            operations_per_second=float(Fraction(2000*10**9*3,101*125)),pmc_status='UNSUPPORTED_PLATFORM')
        for w in d['per_worker']:
            for g in w['pmc']['groups']:g.update(status='UNSUPPORTED_PLATFORM',os_error=0)
        v.verify_record(d)
    def test_no_false_p_core_pinning(self):
        self.bad(lambda d:d['per_worker'][0]['placement'].update(p_core_pinning='VERIFIED'))
    def test_process_accounting_decrease(self):
        self.bad(lambda d:d['process_envelope']['cpu_before'].update(user_us=2))
    def test_incomplete_native_PMC_state(self):
        self.bad(lambda d:d['per_worker'][0]['pmc']['groups'][0].update(status='READY'))
    def test_source_snapshot_digest(self): self.bad(lambda d:d['build'].update(source_sha256='0'*64))
    def test_binary_hash_format(self): self.bad(lambda d:d['binary'].update(sha256='no'))
    def test_external_binary_and_source(self):
        d=fixture()
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);f=root/'synthetic.c';f.write_bytes(b'synthetic')
            v.verify_record(d,f,root)
            f.write_bytes(b'changed')
            with self.assertRaises(ValueError):v.verify_record(d,f,root)
    def test_no_zero_invented_for_unavailable_event(self):
        self.bad(lambda d:d['per_worker'][0]['pmc']['groups'][0]['events'][0].update(raw_delta=0))
    def test_all_supported_reordered_ids(self):v.verify_record(measured(fixture()))
    def test_multiplex_scaling(self):v.verify_record(measured(fixture(),100))
    def test_multiplex_incorrect_estimate(self):
        d=measured(fixture(),100);d['per_worker'][0]['pmc']['groups'][0]['events'][0]['scaled_estimate']=200
        with self.assertRaises(ValueError):v.verify_record(d)
    def test_never_scheduled(self):v.verify_record(measured(fixture(),0))
    def test_never_scheduled_nonzero(self):
        d=measured(fixture(),0);d['per_worker'][0]['pmc']['groups'][0]['after'][3]=1
        with self.assertRaises(ValueError):v.verify_record(d)
    def test_running_exceeds_enabled(self):
        d=measured(fixture());d['per_worker'][0]['pmc']['groups'][0]['after'][2]=300
        with self.assertRaises(ValueError):v.verify_record(d)
    def test_invalid_frame_rejected(self):
        self.bad(lambda d:d['per_worker'][0]['pmc']['groups'][0].update(status='INVALID_READING'))
    def test_duplicate_event_id(self):
        d=measured(fixture());d['per_worker'][0]['pmc']['groups'][0]['after'][4]=11
        with self.assertRaises(ValueError):v.verify_record(d)
    def test_cache_not_coherence(self): self.bad(lambda d:d['metrics'].update(coherence_invalidations=42))
    def test_no_unsupported_metrics(self): self.bad(lambda d:d['metrics'].update(instructions_per_cycle=0))
    def test_paranoid_classification_not_guessed(self):
        d=fixture();d['pmc_status']='RESTRICTED_PARANOID'
        for w in d['per_worker']:
            w['pmc']['paranoid']=3
            for g in w['pmc']['groups']:g.update(status='RESTRICTED_PARANOID',os_error=13)
        v.verify_record(d);d['per_worker'][0]['pmc']['paranoid']=2
        with self.assertRaises(ValueError):v.verify_record(d)
    def test_cli_malformed_and_optimized(self):
        with tempfile.TemporaryDirectory() as t:
            p=Path(t)/'receipt.json';p.write_text('{"schema":1,"schema":2}')
            for options in ([],['-O']):
                r=subprocess.run([sys.executable,*options,str(ROOT/'tools/verify_cache_provenance.py'),str(p)],capture_output=True)
                self.assertEqual(r.returncode,1)
            p.write_text(json.dumps(fixture()).replace('"elapsed_ns": 101','"elapsed_ns": NaN'))
            with self.assertRaises(ValueError):v.load(p)
    def test_campaign_missing_record(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);d=fixture();(root/'cache-2-workers-stride-8-trial-0.json').write_text(json.dumps(d))
            c=dict(schema='elite-cache-campaign-v2',status='COMPLETE',trials=1,expected_records=2,
                   workers=2,iterations_per_worker=1000,binary_sha256=d['binary']['sha256'],pmc_mode='count')
            (root/'cache-campaign.json').write_text(json.dumps(c))
            with self.assertRaises(ValueError):v.verify_path(root)
            d['counter_stride']=128;d['layout_order']=1
            for w in d['per_worker']:w['counter_offset']=128*w['worker_id']
            (root/'cache-2-workers-stride-128-trial-0.json').write_text(json.dumps(d))
            self.assertEqual(len(v.verify_path(root)),2)
    def test_timer_correlation_does_not_assume_frequency(self):
        a=dict(mach_before=1000,count=5000,mach_after=1001)
        b=dict(mach_before=2000,count=6000,mach_after=2001)
        self.assertTrue(v.timer_pair(a,b,24000000,Fraction(125,3),'fixture'))
        self.assertFalse(v.timer_pair(a,b,1000000000,Fraction(125,3),'fixture'))

if __name__=='__main__':unittest.main()
