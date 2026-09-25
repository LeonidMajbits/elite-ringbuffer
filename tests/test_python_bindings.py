#!/usr/bin/env python3
"""Executable binding/lifetime tests; native chaos tests remain separate."""
import ctypes as C
import gc
import json
import os
import pathlib
import selectors
import signal
import struct
import subprocess
import sys
import threading
import time
import unittest
import weakref

BUDGET_FACTOR=float(os.environ.get("ELITE_TEST_TIMEOUT_SCALE","1"))
if not 1 <= BUDGET_FACTOR <= 10:raise ValueError("test budget scale must be in [1,10]")
def budget(seconds):return seconds*BUDGET_FACTOR

from elite_ringbuffer import (EliteShm, EliteSpscProducer, EliteSpscConsumer,
    EliteNcqProducer, EliteNcqConsumer, BusyError, WouldBlock, RetiredError,
    EliteError, cleanup_failures, __version__)
from elite_ringbuffer import _native as N


def child_main():
    request=json.loads(sys.stdin.readline())
    cls=globals()[request['class']]
    ep=cls(request['grant'])
    if request['action']=='crash_write':
        lease=ep.reserve();view=lease.buffer;struct.pack_into('<Q',view,0,991)
        print(json.dumps({'ready':True}),flush=True)
        while True: time.sleep(1)
    if request['action']=='pause_write':
        lease=ep.reserve();view=lease.buffer;view[0]=12
        print(json.dumps({'ready':True}),flush=True)
        sys.stdin.readline()
        view[0]=99;view.release()
        try:lease.commit(1)
        except RetiredError:lease.abandon()
    if request['action']=='uncertain_commit':
        lease=ep.reserve()
        with lease.buffer as view:struct.pack_into('<Q',view,0,876)
        original=N.lib.elite_write_commit
        def interrupted(*args):
            original(*args)
            raise KeyboardInterrupt('synthetic return-value loss AFTER real native publication')
        N.lib.elite_write_commit=interrupted
        try:lease.commit(8,message_id=876)
        except EliteError as exc:
            print(json.dumps({'uncertain':exc.status==12 and exc.outcome==6}),flush=True)
            os._exit(0)
        os._exit(3)
    if request['action']=='produce_n':
        for i in range(500):
            ident=request['grant']['endpoint_index']*500+i
            with ep.reserve(timeout=10) as lease:
                with lease.buffer as view:struct.pack_into('<Q',view,0,ident)
                lease.commit(8,1,ident)
    if request['action']=='consume_n':
        ids=[]
        for _ in range(500):
            with ep.borrow(timeout=10) as lease:
                with lease.buffer as view:
                    if struct.unpack_from('<Q',view)[0]!=lease.message_id:raise RuntimeError('bad bytes')
                ids.append(lease.message_id)
        receipt=ep.close();print(json.dumps({'receipt':receipt,'ids':ids}),flush=True);return
    if request['action']=='publish':
        with ep.reserve() as lease:
            with lease.buffer as view:struct.pack_into('<Q',view,0,123456)
            lease.commit(8,7,99)
    receipt=ep.close()
    print(json.dumps({'receipt':receipt}),flush=True)


def read_line(p, timeout=10):
    # select + TextIO.readline can still block after a partial line. Read only
    # currently available bytes, with one deadline and bounded result storage.
    end=time.monotonic()+budget(timeout)
    pending=getattr(p,'_elite_pending',b'')
    with selectors.DefaultSelector() as sel:
        sel.register(p.stdout,selectors.EVENT_READ)
        while b'\n' not in pending:
            remaining=end-time.monotonic()
            if remaining<=0 or not sel.select(remaining):
                raise TimeoutError('owned child did not return a complete record')
            part=os.read(p.stdout.fileno(),65536)
            if not part:raise RuntimeError('owned child exited before complete result')
            pending+=part
            if len(pending)>1048576:raise RuntimeError('oversized child record')
    record,pending=pending.split(b'\n',1)
    p._elite_pending=pending
    return json.loads(record)


def spawn(shm,index,cls,action):
    p=subprocess.Popen([sys.executable,__file__,'--child'],stdin=subprocess.PIPE,
                       stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    grant=shm.grant(index,p.pid)
    p.stdin.write(json.dumps({'class':cls,'grant':grant,'action':action})+'\n');p.stdin.flush()
    return p


def finish(shm,p,kill=False):
    if kill:os.kill(p.pid,signal.SIGKILL)
    status=shm.reap(p.pid,budget(10))
    if status is None:raise TimeoutError('registered child failed to exit')
    p.returncode=os.waitstatus_to_exitcode(status)
    for f in (p.stdin,p.stdout,p.stderr): f.close()
    return p.returncode


class BindingTests(unittest.TestCase):
    def setUp(self):
        self.baseline=len(cleanup_failures())
        self.shm=EliteShm('ncq',capacity=8,max_payload=256)
        self.p=self.shm.producer();self.c=self.shm.consumer()
    def tearDown(self):
        gc.collect()
        self.shm.close()
        self.assertEqual(len(cleanup_failures()),self.baseline)
    def send(self,data=b'hello',ident=1):
        w=self.p.reserve()
        with w.buffer as view:view[:len(data)]=data
        w.commit(len(data),7,ident)
    def test_pattern_helper_all_bytes(self):
        import _elite_buffer
        w=self.p.reserve()
        with w.buffer as m:
            _elite_buffer.fill_pattern(m,987)
            self.assertTrue(_elite_buffer.verify_pattern(m,987))
            for position in [0,7,8,63,127,255]:
                m[position]^=1
                self.assertFalse(_elite_buffer.verify_pattern(m,987))
                m[position]^=1
        w.commit(256)
        with self.c.borrow() as r:
            with r.buffer as m:
                self.assertTrue(_elite_buffer.verify_pattern(m,987))
                with self.assertRaises((TypeError,BufferError)):_elite_buffer.fill_pattern(m,987)
    def test_version_layouts(self):
        self.assertEqual(__version__,'1.1.0')
        self.assertEqual(N.lib.elite_version_string(),b'1.1.0')
        for i,typ in enumerate(N.LOCAL_TYPES,1):
            self.assertEqual(N.lib.elite_local_layout(i,0xffffffff),C.sizeof(typ))
            for j,(name,_) in enumerate(typ._fields_):
                self.assertEqual(N.lib.elite_local_layout(i,j),getattr(typ,name).offset)
    def test_direct_roundtrip(self):
        w=self.p.reserve()
        with w.buffer as m:struct.pack_into('<QQ',m,0,123,456)
        w.commit(16,8,42)
        r=self.c.borrow()
        with r.buffer as m:self.assertEqual(struct.unpack_from('<QQ',m),(123,456))
        self.assertEqual((r.length,r.message_type,r.message_id),(16,8,42));r.release()
    def test_readonly_exporter(self):
        self.send();r=self.c.borrow();v=r.buffer;obj=v.obj
        self.assertTrue(v.readonly)
        with self.assertRaises(TypeError):v[0]=0
        with self.assertRaises((TypeError,BufferError)):C.c_ubyte.from_buffer(v)
        direct=memoryview(obj);self.assertTrue(direct.readonly)
        v.release();direct.release();r.release()
        with self.assertRaises(BufferError):memoryview(obj)
    def test_slice_survives_root(self):
        self.send();r=self.c.borrow();root=r.buffer;part=root[1:];root.release()
        with self.assertRaises(BufferError):r.release()
        self.assertEqual(bytes(part),b'ello');part.release();r.release()
    def test_cast_survives_root(self):
        w=self.p.reserve();m=w.buffer;cast=m.cast('Q');m.release()
        cast[0]=111
        with self.assertRaises(BufferError):w.commit(8)
        cast.release();w.commit(8)
        with self.c.borrow() as r:
            with r.buffer as m:self.assertEqual(struct.unpack_from('Q',m),(111,))
    def test_ctypes_export_retains_slot(self):
        w=self.p.reserve();m=w.buffer;array=(C.c_ubyte*len(m)).from_buffer(m);m.release()
        array[0]=33
        with self.assertRaises(BufferError):w.commit(1)
        del array;gc.collect();w.commit(1)
        with self.c.borrow() as r:self.assertEqual(r.copy(),b'!')
    def test_multiple_roots(self):
        w=self.p.reserve();a=w.buffer;b=w.buffer;a.release()
        with self.assertRaises(BufferError):w.abort()
        b.release();w.abort()
    def test_busy_detach_and_manager(self):
        w=self.p.reserve();v=w.buffer
        with self.assertRaises(BusyError):self.p.close()
        with self.assertRaises(BusyError):self.shm.close()
        v.release();w.abort()
    def test_context_abort(self):
        with self.assertRaisesRegex(ValueError,'intentional'):
            with self.p.reserve() as w:
                with w.buffer as v:v[0]=77
                raise ValueError('intentional')
        self.assertIsNone(self.c.try_borrow())
    def test_read_context_exception_returns_token(self):
        self.send()
        with self.assertRaisesRegex(ValueError,'read error'):
            with self.c.borrow() as r:
                with r.buffer as v:self.assertEqual(v[0],104)
                raise ValueError('read error')
        self.assertFalse(self.c.busy)
    def test_gc_outer_lease_retained_by_alias(self):
        self.send();r=self.c.borrow();v=r.buffer;part=v[1:];v.release()
        wr=weakref.ref(r);del r;gc.collect();self.assertIsNone(wr())
        self.assertTrue(self.c.busy);self.assertEqual(bytes(part),b'ello')
        part.release();gc.collect();self.assertFalse(self.c.busy)
    def test_gc_uncommitted_write_aborts(self):
        w=self.p.reserve();v=w.buffer;v[0]=255;del w;gc.collect()
        self.assertTrue(self.p.busy);v.release();gc.collect()
        self.assertFalse(self.p.busy);self.assertIsNone(self.c.try_borrow())
    def test_exporter_without_views_retains_ability(self):
        self.send();r=self.c.borrow();v=r.buffer;obj=v.obj;v.release();del r;gc.collect()
        self.assertTrue(self.c.busy)
        with memoryview(obj) as extra:self.assertEqual(bytes(extra),b'hello')
        obj.close();gc.collect();self.assertFalse(self.c.busy)
    def test_explicit_copy(self):
        self.send();r=self.c.borrow();copied=r.copy();r.release();self.assertEqual(copied,b'hello')
    def test_zero_length(self):
        w=self.p.reserve();w.commit(0)
        with self.c.borrow() as r:
            with r.buffer as v:self.assertEqual(len(v),0)
    def test_bounds_before_narrowing(self):
        w=self.p.reserve()
        for invalid in [-1,257,1<<32,True]:
            with self.assertRaises(ValueError):w.commit(invalid)
        w.abort()
    def test_duplicate_transfer(self):
        w=self.p.reserve();w.commit(0)
        with self.assertRaises(BufferError):w.commit(0)
        r=self.c.borrow();r.release()
        with self.assertRaises(BufferError):r.release()
    def test_epoch_advances_on_abort(self):
        # SPSC re-reserves the same unpublished physical slot.
        with EliteShm('spsc',capacity=2) as s:
            p=s.producer();c=s.consumer();w=p.reserve();epoch=w.epoch;w.abort()
            w=p.reserve();self.assertGreater(w.epoch,epoch);w.abort();p.close();c.close()
    def test_role_rejected_before_attach(self):
        with EliteShm('spsc',capacity=2) as s:
            g=s.grant(0)
            with self.assertRaises(ValueError):EliteSpscConsumer(g)
            p=EliteSpscProducer(g);s.ack_cleanup(p.close())
    def test_duplicate_attach_cleanup_does_not_resolve_original(self):
        with EliteShm('spsc',capacity=2) as s:
            g=s.grant(0);p=EliteSpscProducer(g)
            try:EliteSpscProducer(g)
            except EliteError as e:
                receipt=e.resource.close();self.assertEqual(receipt['owns_grant_claim'],0)
            else:self.fail('duplicate attach accepted')
            with self.assertRaises(BusyError):s.close()
            s.ack_cleanup(p.close())
    def test_threads_serialized_one_outstanding(self):
        w=self.p.reserve();errors=[]
        def attempt():
            try:self.p.reserve()
            except BusyError:errors.append('busy')
        t=threading.Thread(target=attempt);t.start();t.join(5)
        self.assertEqual(errors,['busy']);w.abort()
    def test_timeout_is_not_lease_revocation(self):
        w=self.p.reserve();v=w.buffer
        with self.assertRaises(WouldBlock):self.c.borrow(timeout=.002)
        v[0]=42;v.release();w.commit(1)
        with self.c.borrow() as r:self.assertEqual(r.copy(),b'*')
    def test_retirement_does_not_revoke_alias(self):
        w=self.p.reserve();v=w.buffer;self.shm.retire();v[0]=41
        v.release()
        with self.assertRaises(RetiredError):w.commit(1)
        w.abandon()
    def test_received_mapping_survives_endpoint_name_drop(self):
        self.send();r=self.c.borrow();v=r.buffer;ref=weakref.ref(self.c)
        self.c=None;del r;gc.collect();self.assertIsNotNone(ref())
        self.assertEqual(bytes(v),b'hello');v.release();gc.collect()
        self.assertIsNone(ref())
    def test_crc_profile(self):
        with EliteShm('ncq',capacity=4,checksum=True) as s:
            p=s.producer();c=s.consumer();w=p.reserve()
            with w.buffer as v:struct.pack_into('<Q',v,0,44)
            w.commit(8)
            with c.borrow() as r:self.assertEqual(r.copy(),struct.pack('<Q',44))
            p.close();c.close()


class ProcessTests(unittest.TestCase):
    def test_spawned_python_publication(self):
        for mode,cls in [('spsc','EliteSpscProducer'),('ncq','EliteNcqProducer')]:
            with EliteShm(mode,capacity=8) as s:
                c=s.consumer();p=spawn(s,0,cls,'publish');line=read_line(p)
                with c.borrow() as r:self.assertEqual((r.message_id,r.copy()),(99,struct.pack('<Q',123456)))
                self.assertEqual(finish(s,p),0);s.ack_cleanup(line['receipt'])
                c.close()
    def test_kill_leaks_token_but_whole_generation_cleans(self):
        with EliteShm('ncq',capacity=8,producers=2) as s:
            p=s.producer(1);c=s.consumer();victim=spawn(s,0,'EliteNcqProducer','crash_write')
            read_line(victim);os.kill(victim.pid,signal.SIGKILL)
            # No partial dead-owner payload is ready. Healthy producer still works.
            self.assertIsNone(c.try_borrow())
            for i in range(32):
                w=p.reserve();w.commit(0,message_id=i)
                r=c.borrow();self.assertEqual(r.message_id,i);r.release()
            self.assertEqual(finish(s,victim),-signal.SIGKILL)
            p.close();c.close();name=s.name
        # Unlink is checked without constructing an untrusted native attachment.
        import ctypes.util
        libc=C.CDLL(ctypes.util.find_library('c') or None,use_errno=True)
        libc.shm_open.argtypes=[C.c_char_p,C.c_int,C.c_uint];libc.shm_open.restype=C.c_int
        fd=libc.shm_open(name.encode(),os.O_RDWR,0)
        if fd>=0:os.close(fd)
        self.assertEqual(fd,-1)
    def test_pause_resume_distinct_successor(self):
        with EliteShm('ncq',capacity=8) as old:
            victim=spawn(old,0,'EliteNcqProducer','pause_write');read_line(victim)
            os.kill(victim.pid,signal.SIGSTOP)
            self.assertIsNone(old.reap(victim.pid,0))
            with old.successor() as new:
                p=new.producer();c=new.consumer();w=p.reserve()
                with w.buffer as v:v[0]=55
                w.commit(1);r=c.borrow();view=r.buffer
                os.kill(victim.pid,signal.SIGCONT);victim.stdin.write('continue\n');victim.stdin.flush()
                result=read_line(victim);self.assertEqual(finish(old,victim),0);old.ack_cleanup(result['receipt'])
                self.assertEqual(view[0],55);view.release();r.release();p.close();c.close()
    def test_grant_encoding_rejects_missing_fields(self):
        with self.assertRaises(ValueError):EliteNcqProducer({'name':'bogus'})
    def test_inherited_handle_methods_reject(self):
        result=subprocess.run([sys.executable,'-S',__file__,'--fork-guard'],capture_output=True,text=True,timeout=budget(10))
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
    def test_uncertain_python_return_never_blindly_retries(self):
        with EliteShm('ncq',capacity=8) as s:
            c=s.consumer();p=spawn(s,0,'EliteNcqProducer','uncertain_commit')
            self.assertTrue(read_line(p)['uncertain'])
            with c.borrow() as r:self.assertEqual(r.message_id,876)
            self.assertIsNone(c.try_borrow())
            self.assertEqual(finish(s,p),0);c.close()
    def test_python_mpmc_two_producers_two_consumers(self):
        with EliteShm('ncq',capacity=32,producers=2,consumers=2) as s:
            workers=[spawn(s,i,'EliteNcqProducer' if i<2 else 'EliteNcqConsumer',
                           'produce_n' if i<2 else 'consume_n') for i in range(4)]
            records=[read_line(p,20) for p in workers]
            ids=records[2]['ids']+records[3]['ids']
            self.assertEqual(len(ids),1000);self.assertEqual(sorted(ids),list(range(1000)))
            for p,record in zip(workers,records):
                self.assertEqual(finish(s,p),0);s.ack_cleanup(record['receipt'])
    def test_normal_exit_closes_idle_generation(self):
        result=subprocess.run([sys.executable,__file__,'--idle-exit'],capture_output=True,text=True,timeout=budget(10))
        self.assertEqual(result.returncode,0,result.stderr)
        name=result.stdout.strip()
        import ctypes.util
        libc=C.CDLL(ctypes.util.find_library('c') or None,use_errno=True)
        libc.shm_open.argtypes=[C.c_char_p,C.c_int,C.c_uint];libc.shm_open.restype=C.c_int
        fd=libc.shm_open(name.encode(),os.O_RDWR,0)
        if fd>=0:os.close(fd)
        self.assertEqual(fd,-1)
    def test_throughput_driver(self):
        import tempfile
        script=pathlib.Path(__file__).resolve().parents[1]/'bindings/python/bench_python.py'
        with tempfile.TemporaryDirectory() as d:
            out=pathlib.Path(d)/'result.json'
            result=subprocess.run([sys.executable,str(script),'--count','256','--warmup','16',
                                   '--size','64','--out',str(out)],capture_output=True,text=True,timeout=budget(20))
            self.assertEqual(result.returncode,0,result.stderr)
            for condition in json.loads(out.read_text())['conditions']:
                self.assertEqual(condition['messages'],256)
                self.assertEqual(condition['status'],'PASS_WITHIN_SCOPE')
    def test_binding_cleanup_registry_empty(self):
        gc.collect();self.assertEqual(cleanup_failures(),())

def tearDownModule():
    gc.collect()
    if cleanup_failures():raise AssertionError(cleanup_failures())


if __name__=='__main__':
    if '--fork-guard' in sys.argv:
        with EliteShm('spsc',capacity=2) as s:
            p=s.producer();child=os.fork()
            if child==0:
                try:p.reserve()
                except RuntimeError:os._exit(0)
                os._exit(2)
            _,status=os.waitpid(child,0)
            if os.waitstatus_to_exitcode(status)!=0:raise RuntimeError('fork guard failed')
            p.close()
    elif '--idle-exit' in sys.argv:
        shm=EliteShm('spsc',capacity=2);p=shm.producer();c=shm.consumer();print(shm.name,flush=True)
    elif '--child' in sys.argv:child_main()
    else:unittest.main(verbosity=2)
