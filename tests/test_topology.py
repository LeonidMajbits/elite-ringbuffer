#!/usr/bin/env python3
"""Rooted synthetic OS trees; never hardware measurements."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
from verify_matrix_results import topology, val, locality, cache_bounds, Invalid
from run_matrix import machine_budget
BINARY=Path(os.environ.get('ELITE_TOPOLOGY_BINARY',ROOT/'build/native/bench_topology'))
class Fixtures(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
        self.put('/sys/devices/system/cpu/online','2,4,8,10\n');self.put('/proc/self/allowed_cpus','2,4,8,10\n')
        self.put('/proc/meminfo','MemTotal: 4194304 kB\n');self.put('/proc/cpuinfo','model name : synthetic topology fixture\nflags : hypervisor x\n')
        for cpu in (2,4,8,10):
            node=3 if cpu<8 else 8;group='2,4' if cpu<8 else '8,10';base=f'/sys/devices/system/cpu/cpu{cpu}'
            for key,v in {'physical_package_id':node,'die_id':0,'core_id':1,'cluster_id':node,'thread_siblings_list':group}.items():self.put(base+'/topology/'+key,str(v))
            for index,(level,size) in enumerate(((1,'32K'),(2,'1024K'),(3,'32M'))):
                for key,v in {'level':level,'size':size,'type':'Data' if level==1 else 'Unified','coherency_line_size':64,'id':node,'shared_cpu_list':group}.items():self.put(base+f'/cache/index{index}/'+key,str(v))
        self.put('/sys/devices/system/node/node3/cpulist','2,4');self.put('/sys/devices/system/node/node8/cpulist','8,10')
        self.put('/proc/self/cgroup','0::/tenant/job\n');self.put('/proc/self/mountinfo','25 20 0:28 /tenant /cg rw - cgroup2 cgroup rw\n')
        for path,q in (('/cg/job','100000 100000'),('/cg','50000 100000')):
            for key,v in {'cpu.max':q,'memory.max':'2147483648','memory.current':'1048576','cpuset.cpus.effective':'2,4,8,10','cpuset.mems.effective':'3,8'}.items():self.put(path+'/'+key,v)
    def tearDown(self):self.tmp.cleanup()
    def put(self,name,text):
        p=self.root/name.lstrip('/');p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text)
    def get(self):return json.loads(subprocess.check_output([str(BINARY),'--fixture',str(self.root)],text=True))
    def test_sparse_numa_and_smt(self):
        t=self.get();cs=topology(t,synthetic=True);self.assertEqual(set(cs),{2,4,8,10});self.assertEqual(cs[8]['numa_node'],8);self.assertEqual(val(t['physical_cpus']),2);self.assertEqual(cs[2]['smt_siblings']['cpus'],[2,4])
    def test_visible_ancestor_quota(self):
        t=self.get();self.assertEqual([g['path'] for g in t['cgroups']],['/cg/job','/cg']);self.assertEqual(val(t['cgroups'][1]['quota_us']),50000);self.assertEqual(t['cgroup_status'],'V2_VISIBLE_ANCESTORS_ONLY')
    def test_unlimited_is_not_zero(self):
        self.put('/cg/job/cpu.max','max 100000');self.put('/cg/job/memory.max','max\n');g=self.get()['cgroups'][0];self.assertIsNone(val(g['quota_us']));self.assertTrue(g['quota_unlimited']);self.assertTrue(g['memory_unlimited'])
    def test_malformed_cpu_list(self):
        self.put('/proc/self/allowed_cpus','2,2');t=self.get();self.assertTrue(t['partial']);self.assertEqual(t['cpus'],[]);self.assertNotEqual(t['allowed_cpus']['error'],0)
    def test_missing_cache_is_partial(self):
        (self.root/'sys/devices/system/cpu/cpu2/cache/index1/size').unlink();t=self.get();self.assertTrue(t['partial']);self.assertIsNone(t['cpus'][0]['caches'][1]['bytes']['value'])
    def test_no_node_guess(self):
        self.put('/sys/devices/system/node/node3/cpulist','');t=self.get();self.assertEqual(t['cpus'][0]['numa_node'],-1)
    def test_conflicting_nodes(self):
        self.put('/sys/devices/system/node/node9/cpulist','2');t=self.get();self.assertTrue(t['partial']);self.assertEqual(t['cpus'][0]['numa_node'],-2)
    def test_limit_no_truncation(self):
        self.put('/sys/devices/system/cpu/online','0-1024');t=self.get();self.assertTrue(t['partial']);self.assertNotEqual(t['online_cpus']['error'],0)
    def test_escape_cgroup_rejected(self):
        self.put('/proc/self/cgroup','0::/tenant/..');self.assertEqual(self.get()['cgroup_status'],'UNRESOLVED_MOUNT')
    def test_oversized_mountinfo(self):
        self.put('/proc/self/mountinfo','x'*20000);self.assertEqual(self.get()['cgroup_status'],'MOUNTINFO_UNAVAILABLE_OR_TOO_LARGE')
    def test_v1_not_unlimited(self):
        self.put('/proc/self/cgroup','1:cpu:/job');self.assertIn('V1_OR_UNAVAILABLE',self.get()['cgroup_status'])
    def test_no_live_claim_from_fixture(self):
        with self.assertRaises(Invalid):topology(self.get())
    def test_memory_unknown(self):
        (self.root/'proc/meminfo').unlink();self.assertIsNone(val(self.get()['physical_memory_bytes']))
    def test_no_per_cpu_pe_fabrication(self):
        t=self.get();self.assertEqual(t['worker_cluster_mapping'],'NOT_ESTABLISHED');self.assertEqual(t['host_cluster_class'],'TOPOLOGY_CLUSTER_UNKNOWN')
    def test_topology_not_residency(self):
        t=self.get();b=cache_bounds(t,[],1024,65536);self.assertTrue(b['pool_capacity_below_all_reported_l2']);self.assertEqual(b['cache_residency'],'NOT_ESTABLISHED')
    def test_cgroup_resource_budget(self):
        t=self.get();self.assertEqual(machine_budget(t,4000000000),(2147483648-1048576)//2)
    def workers(self,ids):
        return [{'placement':{'requested_cpu':i,'verified_cpu_before':i,'verified_cpu_after':i,'affinity_error':0,'affinity_enforced':True}} for i in ids]
    def live_shaped_fixture(self):
        # Synthetic unit input only, not persisted as a measured hardware receipt.
        t=self.get();t['source_kind']='LIVE_OS_VISIBLE';return t
    def test_same_data_l2(self):
        t=self.live_shaped_fixture();self.assertIn('SHARED_L2',locality(t,self.workers([2,4]),'same_l2'))
    def test_not_instruction_only_l2(self):
        t=self.live_shaped_fixture()
        for c in t['cpus']:
            for ca in c['caches']:
                if ca['level']['value']==2:ca['type']='Instruction'
        with self.assertRaises(Invalid):locality(t,self.workers([2,4]),'same_l2')
    def test_known_cross_numa_shape(self):
        t=self.live_shaped_fixture();self.assertIn('CROSS_NUMA_CPU_ONLY',locality(t,self.workers([2,8]),'cross_numa'))
    def test_wrong_same_numa(self):
        with self.assertRaises(Invalid):locality(self.live_shaped_fixture(),self.workers([2,8]),'same_numa')
    def test_ambiguous_node_not_certified(self):
        t=self.live_shaped_fixture();t['nodes'][1]['cpus']['cpus']=[2,8,10]
        with self.assertRaises(Invalid):topology(t)
if __name__=='__main__':unittest.main(verbosity=2)
