"""Cross-platform offline stop/termination decoding, including continued traps."""
from pathlib import Path
import copy
import importlib.util
import sys
import unittest
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('chaos_wait_verifier', ROOT/'tools/verify_chaos_results.py')
v = importlib.util.module_from_spec(spec)
spec.loader.exec_module(v)

def fixture(name='claim'):
    return [v.loads(s) for s in (ROOT/'tests/fixtures/chaos'/f'{name}.jsonl').read_text().splitlines()]

def to_darwin(data):
    data=copy.deepcopy(data)
    data[0]['wait_status_abi']='darwin'
    codes={'SIGKILL':9,'SIGSTOP':17,'SIGCONT':19,'SIGUSR1':30,'SIGALRM':14}
    for e in data:
        if e['event']=='stopped_observed':e['wait_status']=(17<<8)|0x7f
        if e['event']=='signal_sent':e['signal_number']=codes[e['signal']]
    return data

class WaitStatusTests(unittest.TestCase):
    def test_linux_stop(self): self.assertEqual(v.wait_status(0x137f,'linux'),('stopped',19))
    def test_darwin_stop(self): self.assertEqual(v.wait_status(0x117f,'darwin'),('stopped',17))
    def test_darwin_continued_is_not_linux_stop(self): self.assertEqual(v.wait_status(0x137f,'darwin'),('continued',None))
    def test_linux_continued(self): self.assertEqual(v.wait_status(0xffff,'linux'),('continued',None))
    def test_terminal_sigkill(self):
        for abi in ('linux','darwin'):self.assertEqual(v.wait_status(9,abi),('signaled',9))
    def test_exits(self):
        for abi in ('linux','darwin'):
            self.assertEqual(v.wait_status(0,abi),('exited',0))
            self.assertEqual(v.wait_status(256,abi),('exited',1))
    def test_invalid_encodings(self):
        for status,abi in [(True,'linux'),(-1,'linux'),(65536,'linux'),(0x7f,'linux'),(0,'guess')]:
            with self.subTest(status=status,abi=abi),self.assertRaises(ValueError):v.wait_status(status,abi)
    def test_legacy_linux_fixture(self):self.assertEqual(v.verify_events(fixture())['status'],'PASS_WITHIN_SCOPE')
    def test_explicit_linux_fixture(self):
        x=fixture();x[0]['wait_status_abi']='linux';self.assertEqual(v.verify_events(x)['status'],'PASS_WITHIN_SCOPE')
    def test_synthetic_darwin_fixtures(self):
        for name in ('claim','read','resume','late','storm','async'):
            with self.subTest(name=name):self.assertEqual(v.verify_events(to_darwin(fixture(name)))['status'],'PASS_WITHIN_SCOPE')
    def test_continued_cannot_fence(self):
        x=to_darwin(fixture());next(e for e in x if e['event']=='stopped_observed')['wait_status']=0x137f
        with self.assertRaises(ValueError):v.verify_events(x)
    def test_stop_is_not_death(self):
        x=to_darwin(fixture());next(e for e in x if e['event']=='terminal_observed')['wait_status']=0x117f
        with self.assertRaises(ValueError):v.verify_events(x)
    def test_unknown_abi(self):
        x=fixture();x[0]['wait_status_abi']='invented'
        with self.assertRaises(ValueError):v.verify_events(x)
    def test_mismatched_signal(self):
        x=to_darwin(fixture());next(e for e in x if e['event']=='signal_sent')['signal_number']=99
        with self.assertRaises(ValueError):v.verify_events(x)

if __name__=='__main__':unittest.main(verbosity=2)
