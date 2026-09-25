"""Independent negative controls mutate raw receipts, not just stale hashes."""
from __future__ import annotations
import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('chaos_verifier',ROOT/'tools/verify_chaos_results.py')
v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
FIX=ROOT/'tests/fixtures/chaos'

def records(name='claim'):
    return [v.loads(line) for line in (FIX/(name+'.jsonl')).read_text().splitlines()]

def event(data,event_name,**fields):
    return next(e for e in data if e['event']==event_name and all(e.get(k)==x for k,x in fields.items()))

class ChaosOracleTests(unittest.TestCase):
    def fails(self,data):
        with self.assertRaises((ValueError,KeyError,TypeError,IndexError)):
            v.verify_events(data)
    def test_all_saved_native_fixtures(self):
        for path in sorted(FIX.glob('*.jsonl')):
            with self.subTest(path=path.name):self.assertEqual(v.verify_file(path)['status'],'PASS_WITHIN_SCOPE')
    def test_missing_completion(self): self.fails(records()[:-1])
    def test_unreaped_complete(self):
        x=records();x[-1]['all_children_reaped']=False;self.fails(x)
    def test_missing_name_cleanup(self):
        x=records();x[-1]['all_owned_names_absent']=False;self.fails(x)
    def test_sequence_gap(self):
        x=records();x[4]['log_sequence']+=1;self.fails(x)
    def test_clock_reverse(self):
        x=records();x[4]['observer_tick']=0;self.fails(x)
    def test_child_future_clock(self):
        x=records();e=event(x,'worker_event');e['tick']=e['observer_tick']+1;self.fails(x)
    def test_child_sequence(self):
        x=records();event(x,'worker_event')['sequence']=2;self.fails(x)
    def test_foreign_pid(self):
        x=records();event(x,'worker_event',kind=4)['pid']+=1;self.fails(x)
    def test_timeout_does_not_fence(self):
        x=records();event(x,'heartbeat_overdue')['death_inferred']=True;self.fails(x)
    def test_timeout_does_not_reclaim(self):
        x=records();event(x,'heartbeat_overdue')['slot_reclaimed']=True;self.fails(x)
    def test_timeout_custody(self):
        x=records();event(x,'heartbeat_overdue')['last_received_tick']-=1;self.fails(x)
    def test_early_timeout(self):
        x=records();e=event(x,'heartbeat_overdue');e['deadline_tick']=e['observer_tick']+1;self.fails(x)
    def test_stop_not_terminal(self):
        x=records();event(x,'stop_not_death')['status']=0;self.fails(x)
    def test_exit_signal_wrong(self):
        x=records();event(x,'terminal_observed')['signal']='SIGSTOP';self.fails(x)
    def test_premature_destruction_allowed(self):
        x=records('resume');event(x,'unfenced_destroy_blocked')['status']=0;self.fails(x)
    def test_illegal_resumed_commit(self):
        x=records('resume');event(x,'worker_event',kind=5)['values'][:2]=[0,2];self.fails(x)
    def test_false_hook_claim(self):
        x=records();x[0]['hooks']=False;self.fails(x)
    def test_nohook_storm_with_hooks(self):
        x=records('storm');x[0]['hooks']=True;self.fails(x)
    def test_wrong_hook_cut(self):
        x=records();event(x,'worker_event',kind=4)['values'][0]=6;self.fails(x)
    def test_header_corruption(self):
        x=records();e=event(x,'snapshot');s=e['immutable_prefix_hex'];e['immutable_prefix_hex']='00'+s[2:];self.fails(x)
    def test_reused_session(self):
        x=records();event(x,'generation',generation_id=1)['name']=event(x,'generation',generation_id=0)['name'];self.fails(x)
    def test_reused_parent_address(self):
        x=records();event(x,'generation',generation_id=1)['observer_address']=event(x,'generation',generation_id=0)['observer_address'];self.fails(x)
    def test_wrong_snapshot_scope(self):
        x=records();event(x,'snapshot')['scope']='LIVE_RACING_SCAN';self.fails(x)
    def test_bad_gate_bits(self):
        x=records();event(x,'snapshot')['admission'] |=256;self.fails(x)
    def test_bad_gate_count(self):
        x=records();event(x,'snapshot')['admission'] +=99<<32;self.fails(x)
    def test_duplicate_free_token(self):
        x=records();e=event(x,'snapshot');h=e['cursors'][0];a=h%64;b=(h+1)%64
        e['qf_entries'][a]=(e['qf_entries'][a]&~63)|(e['qf_entries'][b]&63);self.fails(x)
    def test_wrong_live_cycle(self):
        x=records();e=event(x,'snapshot');e['qf_entries'][e['cursors'][0]%64]+=64;self.fails(x)
    def test_too_large_frontier(self):
        x=records();event(x,'snapshot')['cursors'][1]+=128;self.fails(x)
    def test_orphan_identity_changed(self):
        x=records();event(x,'worker_event',kind=4)['values'][2]=63;self.fails(x)
    def test_orphan_phase_corruption(self):
        x=records();e=event(x,'snapshot');b=event(x,'worker_event',kind=4)['values'][2];e['status_words'][b]=3;self.fails(x)
    def test_free_epoch_bad(self):
        x=records();e=event(x,'snapshot');b=e['qf_entries'][e['cursors'][0]%64]%64;e['epochs'][b]+=1;self.fails(x)
    def test_uncommitted_sentinel_delivered(self):
        x=records();event(x,'cohort_reconciled')['sentinel_received']=1;self.fails(x)
    def test_loss_hidden_by_count(self):
        x=records();e=next(e for e in x if e['event']=='membership' and e['hex']);e['hex']='00'*len(bytes.fromhex(e['hex']));self.fails(x)
    def test_count_wrong(self):
        x=records();event(x,'cohort_reconciled')['received']-=1;self.fails(x)
    def test_late_publication_omitted(self):
        x=records('late');e=event(x,'snapshot');e['cursors'][2]+=1;self.fails(x)
    def test_spsc_reclamation_lie(self):
        x=records('read');event(x,'snapshot')['cursors'][1]=1;self.fails(x)
    def test_spsc_status_mutation(self):
        x=records('read');event(x,'snapshot')['status_words'][0]=1;self.fails(x)
    def test_storm_no_deliveries(self):
        x=records('storm')
        for e in x:
            if e['event']=='worker_event' and e['kind']==3:e['values'][4]=e['values'][5]=0
        self.fails(x)
    def test_async_is_not_zero_loss(self):
        x=records('async');event(x,'cohort_reconciled')['retired_during_work']=False;self.fails(x)
    def test_async_dropped_pending_message(self):
        x=records('async');e=event(x,'snapshot');q,_=v.queue_members(*e['cursors'][2:],e['qr_entries'],64)
        self.assertTrue(q);e['message_ids'][next(iter(q))]=0;self.fails(x)
    def test_async_orphan_unrecorded(self):
        x=records('async');e=next(e for e in x if e['event']=='worker_event' and e['kind']==3 and e['worker_id']<32 and e['values'][7]);e['values'][7]=0;self.fails(x)
    def test_resource_budget(self):
        x={'schema':'elite-chaos-resource-v1','scope':'single_process_authority_and_live_lease_negative','objects':4,'quarantines':2,'third_quarantine_status':4,'fifth_create_status':4,'premature_authority_destroy_status':4,'retained_bytes_unchanged':True,'cleanup_complete':True}
        self.assertIn('resource_negative',v.verify_resource(x));x['fifth_create_status']=0
        with self.assertRaises(ValueError):v.verify_resource(x)
    def test_duplicate_json_keys(self):
        with self.assertRaises(ValueError):v.loads('{"count":1,"count":1}')
    def test_nonfinite(self):
        with self.assertRaises(ValueError):v.loads('{"count":NaN}')
    def test_bool_integer(self):
        x=records();x[0]['messages']=True;self.fails(x)
    def test_cli_optimized_rejects_mutant(self):
        x=records();event(x,'heartbeat_overdue')['slot_reclaimed']=True
        with tempfile.TemporaryDirectory() as td:
            path=Path(td)/'bad.jsonl';path.write_text(''.join(json.dumps(e)+'\n' for e in x))
            p=subprocess.run([sys.executable,'-O',str(ROOT/'tools/verify_chaos_results.py'),str(path)],capture_output=True,text=True)
            self.assertEqual(p.returncode,1);self.assertIn('timeout theft',p.stderr)
    def test_unknown_campaign_profile(self):
        with self.assertRaises(ValueError):v.campaign_matrix('fabricated',1,100,160)
    def test_standard_population(self):
        cases=v.campaign_matrix('standard',3,10000,32000)
        self.assertEqual(len(cases),93);self.assertEqual(len({c['id'] for c in cases}),93)
    def test_incomplete_campaign(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td);(root/'plan.json').write_text('{}')
            with self.assertRaises(OSError):v.verify_campaign(root)

if __name__=='__main__':unittest.main(verbosity=2)
