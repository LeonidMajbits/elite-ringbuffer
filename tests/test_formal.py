#!/usr/bin/env python3
"""Checker self-tests and negative controls, not extra production guarantees."""
import copy,dataclasses,hashlib,json,sys,tempfile,unittest
from pathlib import Path
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(ROOT/'formal'),str(ROOT/'tools')]
from models.ncq import Model as NCQ,put
from models.spsc import Model as SPSC
from explore import run
from handoff import spsc,ncq,ticket_atomicity,closure
from boundaries import bounds,wrapping,gate
from targeted import scenario,stale_consumer
from verify_formal_results import replay_witness
from check_formal_binding import check

class FormalTests(unittest.TestCase):
    def test_finite_deadend_rejected(self):
        class Dead:
            name='checker-negative-deadend';config={}
            def initial(self):return 0
            def terminal(self,s):return False
            def invariant(self,s):return None
            def successors(self,s):return iter(())
        r=run(Dead(),10,10)
        self.assertEqual(r['status'],'FAIL_DEADLOCK');self.assertEqual(r['nonterminal_deadends'],1)
    def test_nonprogress_cycle_detector(self):
        class Loop:
            name='checker-negative-loop';config={}
            def initial(self):return 0
            def terminal(self,s):return False
            def invariant(self,s):return None
            def successors(self,s):yield 'loop',s,False,None
        self.assertEqual(run(Loop(),10,10)['status'],'FAIL_PROGRESS_CYCLE')
    def test_progress_cycle_not_deadlock(self):
        class Loop:
            name='checker-positive-try-returns';config={}
            def initial(self):return 0
            def terminal(self,s):return False
            def invariant(self,s):return None
            def successors(self,s):yield 'completed empty try',s,True,None
        self.assertEqual(run(Loop(),10,10)['status'],'PASS_WITHIN_SCOPE')
    def test_hash_collision_does_not_merge(self):
        @dataclasses.dataclass(frozen=True)
        class Node:
            value:int
            def __hash__(self):return 0
        class PathModel:
            name='collision-selftest';config={}
            def initial(self):return Node(0)
            def terminal(self,s):return s.value==2
            def invariant(self,s):return None
            def successors(self,s):
                if s.value<2:yield 'next',Node(s.value+1),True,None
        self.assertEqual(run(PathModel(),10,10)['states'],3)
    def test_source_anchors(self):self.assertEqual(check(),8)
    def test_invalid_capacity(self):
        for n in (0,1,3):
            with self.assertRaises(ValueError):NCQ(n,1,1)
    def test_overbooked_endpoints(self):
        with self.assertRaises(ValueError):NCQ(2,2,2)
    def test_negative_spsc_role(self):
        with self.assertRaises(ValueError):SPSC(2,2,1)
    def test_inapplicable_mutation(self):
        with self.assertRaises(ValueError):SPSC(mutation='split_head')
        with self.assertRaises(ValueError):NCQ(mutation='publish_early')
    def test_spsc_complete(self):
        r=run(SPSC(2,1,1,3),10000,10)
        self.assertTrue(r['exhaustive']);self.assertGreater(r['terminals'],0)
    def test_ncq_complete(self):
        r=run(NCQ(2,1,1,2),10000,10)
        self.assertTrue(r['exhaustive']);self.assertTrue(r['legal_head_ahead_of_tail_reached'])
    def test_budget_fails_closed(self):
        r=run(NCQ(),3,10)
        self.assertEqual(r['status'],'INCOMPLETE');self.assertFalse(r['exhaustive'])
    def test_time_cutoff(self):
        self.assertEqual(run(NCQ(),1000,-1)['status'],'INCOMPLETE')
    def test_initial_conservation(self):self.assertIsNone(NCQ().invariant(NCQ().initial()))
    def test_duplicate_membership_rejected(self):
        m=NCQ();s=m.initial();s=dataclasses.replace(s,entries=put(s.entries,1,0))
        self.assertIsNotNone(m.invariant(s))
    def test_tail_ahead_rejected(self):
        m=NCQ();s=m.initial();s=dataclasses.replace(s,tails=(5,4))
        self.assertEqual(m.invariant(s)[0],'QUEUE_FRONTIER_ORDER')
    def test_alias_transfer_rejected(self):
        m=NCQ();s=m.initial();a=dataclasses.replace(s.actors[0],block=0,alias=True)
        s=dataclasses.replace(s,actors=put(s.actors,0,a))
        self.assertEqual(m.invariant(s)[0],'RECLAIM_WITH_LIVE_ALIAS')
    def test_stale_head_witness(self):
        r=run(NCQ(4,2,2,1,'stale_head_retry',False),200000,20)
        self.assertEqual(r['status'],'FAIL_WITNESS');self.assertTrue(replay_witness(json.loads(json.dumps(r))))
    def test_fake_witness_rejected(self):
        r=run(SPSC(2,1,1,1,'publish_early'),1000,10);r=json.loads(json.dumps(r))
        r['witness']['steps'][-1]['event']='fabricated'
        with self.assertRaises(ValueError):replay_witness(r)
    def test_all_structural_mutants(self):
        for mut in ('split_head','overwrite_current','tail_before_entry','post_lp_write','early_return'):
            with self.subTest(mut=mut):
                r=run(NCQ(4,2,2,1,mut,False),200000,20)
                self.assertEqual(r['status'],'FAIL_WITNESS');replay_witness(json.loads(json.dumps(r)))
    def test_hb_transitive_closure(self):self.assertTrue(closure(3,[(0,1),(1,2)])[0] & 4)
    def test_spsc_ra_graphs(self):
        r=spsc(2,4);self.assertEqual(r['status'],'PASS_WITHIN_SCOPE');self.assertEqual(r['consistent'],18)
    def test_spsc_publication_relaxed_races(self):
        r=spsc(2,4,'relaxed');self.assertEqual(r['status'],'FAIL_WITNESS');self.assertTrue(r['witness']['races'])
    def test_spsc_recycle_relaxed_races(self):self.assertEqual(spsc(2,4,'release','relaxed')['status'],'FAIL_WITNESS')
    def test_ncq_reference_hb(self):self.assertEqual(ncq()['status'],'PASS_WITHIN_SCOPE')
    def test_ncq_relaxed_install_epoch_race(self):
        r=ncq('relaxed');self.assertIn(['P_epoch','C_epoch_BEFORE_status'],r['witness']['races'])
        self.assertNotIn(['P_payload','C_payload'],r['witness']['races'])
    def test_ncq_relaxed_observation_epoch_race(self):self.assertEqual(ncq('seq_cst','relaxed')['status'],'FAIL_WITNESS')
    def test_relaxed_CAS_retains_atomicity(self):
        r=ticket_atomicity();self.assertEqual(r['status'],'PASS_WITHIN_SCOPE');self.assertEqual(len(r['histories']),4)
    def test_counter_bounds(self):self.assertGreater(bounds()['scalar_checks'],10000)
    def test_wrap_witness(self):
        r=wrapping();self.assertEqual(len(r['steps']),260);self.assertEqual(r['baseline']['stops_at'],251)
        self.assertTrue(r['at_256']['stale_CAS_comparison_would_match'])
    def test_gate_races(self):self.assertEqual(gate()['schedules'],6)
    def test_stale_consumer_across_laps(self):
        r=stale_consumer();self.assertTrue(r['stale_CAS_failed']);self.assertEqual(r['successful_peer_removals'],9)
    def test_stopped_writer_private(self):self.assertEqual(scenario()['healthy_messages'],8)
    def test_stopped_writer_published(self):self.assertEqual(scenario(True)['healthy_messages'],9)

if __name__=='__main__':unittest.main(verbosity=2)
