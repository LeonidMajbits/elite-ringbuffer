"""Finite executable-demo and rehashed semantic receipt negative tests."""
import copy
import importlib.util
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import unittest
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('verify_live_demo',ROOT/'tools/verify_live_demo.py')
v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)

class LiveOracle(unittest.TestCase):
    def setUp(self):
        self.t=tempfile.TemporaryDirectory();self.addCleanup(self.t.cleanup)
        self.path=Path(self.t.name)/'receipt.jsonl'
        self.rows=[{'schema':'elite-live-demo-v1','version':'1.1.0','mode':'SPSC','metric':'RTT',
            'bytes_per_direction':64,'origin_pid':11,'responder_pid':12,'timebase_numer':125,
            'timebase_denom':3,'planned_windows':1,'messages_per_window':2},
            {'window':0,'roundtrips':2,'start_tick':2**54,'end_tick':2**54+6,
             'directional_mps':16000000.,'p50_rtt_ns':84,'p99_rtt_ns':125,
             'samples':[[2**54,2],[2**54+3,3]]},
            {'status':'COMPLETE','roundtrips':2,'directional_records':4,'windows':1,
             'child_checked':2,'leases_ended':True,'objects_destroyed':True}]
    def save(self): self.path.write_text(''.join(json.dumps(x)+'\n' for x in self.rows))
    def test_exact_rational_and_large_ticks(self): self.save();self.assertEqual(v.verify(self.path)['roundtrips'],2)
    def test_mutations(self):
        original=copy.deepcopy(self.rows)
        mutations=[(0,'timebase_denom',0),(0,'responder_pid',11),(1,'p99_rtt_ns',124),
                   (1,'directional_mps',32000000.),(1,'end_tick',2**54+7),
                   (2,'child_checked',1),(2,'objects_destroyed',False),
                   (2,'directional_records',2),(0,'metric','one-way')]
        for i,k,val in mutations:
            with self.subTest(k=k):
                self.rows=copy.deepcopy(original);self.rows[i][k]=val;self.save()
                with self.assertRaises(ValueError): v.verify(self.path)
    def test_boolean_window_rejected(self):
        self.rows[1]['window']=False;self.save();self.assertRaises(ValueError,v.verify,self.path)
    def test_float_payload_size_rejected(self):
        self.rows[0]['bytes_per_direction']=64.0;self.save();self.assertRaises(ValueError,v.verify,self.path)
    def test_truncated(self): self.rows.pop();self.save();self.assertRaises(ValueError,v.verify,self.path)
    def test_duplicate_key(self):
        self.save();self.path.write_text(self.path.read_text().replace('"metric": "RTT"','"metric":"RTT","metric":"RTT"'))
        self.assertRaises(ValueError,v.verify,self.path)
    def test_cancelled_not_complete(self):
        self.rows[2]['status']='CANCELLED_CLEAN';self.save()
        self.assertRaises(ValueError,v.verify,self.path);self.assertEqual(v.verify(self.path,True)['status'],'CANCELLED_CLEAN')
    def test_optimized_python_rejects(self):
        self.rows[2]['child_checked']=0;self.save()
        r=subprocess.run([sys.executable,'-O',str(ROOT/'tools/verify_live_demo.py'),str(self.path)],capture_output=True,timeout=10)
        self.assertEqual(r.returncode,1)

class LiveNative(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.binary=Path(os.environ.get('ELITE_DEMO_BINARY',ROOT/'build/native/live_throughput_demo')).resolve()
        if not cls.binary.is_file(): raise unittest.SkipTest('build demo / set ELITE_DEMO_BINARY for native lane')
    def test_modes(self):
        for mode in ['spsc','ncq']:
            with self.subTest(mode=mode),tempfile.TemporaryDirectory() as td:
                receipt=Path(td)/'native.jsonl'
                r=subprocess.run([str(self.binary),'--mode',mode,'--windows','3','--messages','2000','--plain','--json',str(receipt)],capture_output=True,text=True,timeout=30)
                self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(v.verify(receipt)['roundtrips'],6000)
                self.assertIn('RAW RTT',r.stdout)
    def test_existing_file_refused(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/'keep';p.write_text('unchanged')
            r=subprocess.run([str(self.binary),'--json',str(p)],capture_output=True,timeout=10)
            self.assertEqual(r.returncode,1);self.assertEqual(p.read_text(),'unchanged')
    def test_bad_arguments(self):
        for args in [['--messages','0'],['--mode','fake'],['--windows','10001'],['--timeout','-1']]:
            self.assertEqual(subprocess.run([str(self.binary),*args],capture_output=True,timeout=10).returncode,1)
    def test_cancel_between_records(self):
        with tempfile.TemporaryDirectory() as td:
            receipt=Path(td)/'cancel.jsonl'
            p=subprocess.Popen([str(self.binary),'--windows','10000','--messages','1000','--plain','--json',str(receipt)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,start_new_session=True)
            try:
                # Line-buffered pipe reader waits for a completed, flushed window.
                for _ in range(4):
                    line=p.stdout.readline()
                    self.assertTrue(line)
                p.send_signal(signal.SIGINT)
                out,err=p.communicate(timeout=20)
                self.assertEqual(p.returncode,130,err)
                self.assertEqual(v.verify(receipt,True)['status'],'CANCELLED_CLEAN')
            finally:
                if p.poll() is None: os.killpg(p.pid,signal.SIGKILL);p.wait()
