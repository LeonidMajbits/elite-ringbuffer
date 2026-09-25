#!/usr/bin/env python3
"""Adversarial verification tests, including rehashed semantic mutations.

The freshly executed Turn17 source-bound campaign is the fixture.
The original Turn13 receipts remain unchanged; changing a reviewed source binding
requires a new run rather than weakening the source-identity oracle. Rehashing is deliberate: a verifier
must reject invalid interpretations even when ordinary file integrity is valid.
No upstream evidence is modified.
"""
import hashlib,json,os,shutil,subprocess,sys,tempfile,unittest
from pathlib import Path
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from verify_formal_results import verify,load

class ReceiptTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.out=Path(self.tmp.name)/'campaign'
        shutil.copytree(ROOT/'evidence/turn17/formal_admitted',self.out)
    def change(self,name,fn):
        p=self.out/name;r=json.loads(p.read_text());fn(r);p.write_text(json.dumps(r,indent=2)+'\n')
        execution=self.out/'execution.json'
        if name not in ('plan.json','summary.json','execution.json'):
            es=json.loads(execution.read_text())
            for ex in es:
                if ex['result_file']==name:ex['result_sha256']=hashlib.sha256(p.read_bytes()).hexdigest()
            execution.write_text(json.dumps(es,indent=2)+'\n')
        self.rehash()
    def rehash(self):
        m={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(self.out.iterdir()) if p.is_file() and p.name!='manifest.json'}
        (self.out/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
    def reject(self):
        with self.assertRaises((ValueError,KeyError,TypeError,OSError)):verify(self.out)
    def test_valid_all_auxiliary_replay(self):self.assertEqual(verify(self.out),(7,0))
    def test_missing_case(self):
        (self.out/'mutant_split_head.json').unlink();self.rehash();self.reject()
    def test_wrong_bound(self):
        self.change('ncq_n4_2p2c.json',lambda r:r['config'].update(reservations_per_producer=0));self.reject()
    def test_laundered_expected_exit(self):
        self.change('execution.json',lambda r:r[0].update(expected_exit=2,returncode=2,expectation_met=True));self.reject()
    def test_removed_source_identity(self):
        self.change('plan.json',lambda r:r.update(source_sha256={}));self.reject()
    def test_claim_full_qualification(self):
        self.change('summary.json',lambda r:r.update(full_turn5_v1_qualification=True));self.reject()
    def test_external_tool_claim(self):
        self.change('summary.json',lambda r:r.update(external_spin_executed=True));self.reject()
    def test_changed_handoff_edge(self):
        self.change('handoff.json',lambda r:r['results'][1]['witness'].update(races=[]));self.reject()
    def test_changed_boundary_witness(self):
        self.change('boundaries.json',lambda r:r['wrap_mutant']['at_256'].update(current_representation=5));self.reject()
    def test_changed_targeted_trace(self):
        self.change('targeted.json',lambda r:r['results'][0].update(healthy_messages=999));self.reject()
    def test_fabricated_mutant_transition(self):
        self.change('mutant_split_head.json',lambda r:r['witness']['steps'][-1].update(event='fabricated'));self.reject()
    def test_incomplete_as_pass(self):
        self.change('ncq_n4_2p2c.json',lambda r:r.update(frontier_remaining=1));self.reject()
    def test_duplicate_keys(self):
        p=self.out/'bad.json';p.write_text('{"x": 1, "x": 2}')
        with self.assertRaises(ValueError):load(p)
    def test_numeric_infinity(self):
        p=self.out/'bad.json';p.write_text('{"x": 1e999}')
        with self.assertRaises(ValueError):load(p)
    def test_python_optimized_cli_rejects(self):
        self.change('summary.json',lambda r:r.update(external_genmc_executed=True))
        r=subprocess.run([sys.executable,'-O',str(ROOT/'tools/verify_formal_results.py'),str(self.out)],capture_output=True,text=True,timeout=30,env={**os.environ,'PYTHONDONTWRITEBYTECODE':'1'})
        self.assertEqual(r.returncode,1,r.stdout+r.stderr)

if __name__=='__main__':unittest.main(verbosity=2)
