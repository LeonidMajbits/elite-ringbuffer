#!/usr/bin/env python3
"""Bounded CI lane runner. Writes commands, failures and logs; never swallows them.

GitHub jobs collect this directory with if: always(). No remote publication,
privilege modification, runtime download, or performance threshold lives here.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import signal
import subprocess
import sys
import time
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]

class Runner:
    def __init__(self, out):
        self.out=out.resolve();self.out.mkdir(parents=True,exist_ok=False);self.records=[]
    def run(self, args, env=None, expected=0, contains=None, timeout=900):
        args=[str(x) for x in args]
        name=f'{len(self.records):02d}-{Path(args[0]).name}.log'
        rec={'argv':args,'expected_exit':expected,'timeout_seconds':timeout,'log':name,'timed_out':False}
        started=time.monotonic_ns()
        with (self.out/name).open('w') as log:
            log.write('$ '+repr(args)+'\n');log.flush()
            try:
                p=subprocess.Popen(args,cwd=ROOT,env={**os.environ,**(env or {})},stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
                try:rec['exit']=p.wait(timeout=timeout)
                except subprocess.TimeoutExpired:
                    rec['timed_out']=True
                    os.killpg(p.pid,signal.SIGKILL);rec['exit']=p.wait()
            except OSError as exc:rec['exit']=None;rec['launch_error']=str(exc)
        rec['duration_ns']=time.monotonic_ns()-started
        raw=(self.out/name).read_bytes();rec['sha256']=hashlib.sha256(raw).hexdigest()
        rec['pass']=not rec['timed_out'] and rec['exit']==expected and (contains is None or contains in raw.decode(errors='replace'))
        self.records.append(rec);self.save('RUNNING')
        print(('PASS ' if rec['pass'] else 'FAIL ')+name,flush=True)
        if not rec['pass']:raise RuntimeError(f'command failed; inspect {self.out/name}')
    def save(self,status):
        (self.out/'RESULT.json').write_text(json.dumps({'status':status,'commands':self.records},indent=2)+'\n')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--lane',choices=['native','asan','tsan','valgrind','freebsd'],required=True)
    p.add_argument('--cc',default='clang');p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();r=Runner(a.out)
    build=ROOT/'build'/('lane-'+a.lane+'-'+Path(a.cc).name)
    environment={'system':platform.system(),'machine':platform.machine(),'release':platform.release(),
                 'python':sys.version,'cc':shutil.which(a.cc),'lane':a.lane,'build':str(build),
                 'runner_os':os.environ.get('RUNNER_OS'),'runner_arch':os.environ.get('RUNNER_ARCH')}
    (r.out/'ENVIRONMENT.json').write_text(json.dumps(environment,indent=2)+'\n')
    try:
        r.run([sys.executable,'tools/verify_release.py'])
        r.run([a.cc,'--version'])
        if a.lane in {'native','freebsd'}:
            if a.lane=='freebsd' and platform.system()!='FreeBSD':raise RuntimeError('FreeBSD guest required')
            if a.lane=='native' and platform.system()=='Darwin' and platform.machine()!='arm64':raise RuntimeError('native Apple ARM64 runner required')
            if a.lane=='native':
                r.run(['make',f'CC={a.cc}',f'CXX={"g++" if a.cc=="gcc" else "clang++"}',f'BUILD={build}',f'PYTHON={sys.executable}',
                       'release-check','check-seeding'],timeout=1800)
            cm=Path(str(build)+'-cmake')
            args=['cmake','-S','.', '-B',cm,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release',f'-DCMAKE_C_COMPILER={a.cc}',
                  '-DELITE_BUILD_TESTS=ON']
            if a.lane=='freebsd':args.append('-DELITE_EXPERIMENTAL_FREEBSD=ON')
            r.run(args);r.run(['cmake','--build',cm,'--parallel','2'])
            r.run(['ctest','--test-dir',cm,'--output-on-failure'])
            r.run([sys.executable,'tools/check_cmake_install.py','--build',cm,'--work',Path(str(build)+'-install')])
            for mode in ['spsc','ncq']:
                trace=r.out/('live-'+mode+'.jsonl')
                r.run([cm/'live_throughput_demo','--mode',mode,'--windows','3','--messages','2000','--plain','--json',trace])
                r.run([sys.executable,'tools/verify_live_demo.py',trace])
        else:
            if platform.system()!='Linux':raise RuntimeError('Linux diagnostic lane required')
            env={}
            opts=['make',f'CC={a.cc}',f'BUILD={build}']
            if a.lane=='asan':
                opt='-O1 -g -fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-lto -fsanitize=address,undefined -fno-sanitize-recover=all'
                opts += ['OPT='+opt,'LDFLAGS=-fsanitize=address,undefined']
                env={'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'}
            elif a.lane=='tsan':
                opts += ['OPT=-O1 -g -fno-omit-frame-pointer -fno-lto -fsanitize=thread','LDFLAGS=-fsanitize=thread']
                env={'TSAN_OPTIONS':'halt_on_error=1:exitcode=66:force_seq_cst_atomics=0:report_atomic_races=1:ignore_interceptors_accesses=0:ignore_noninstrumented_modules=0'}
            if a.lane=='tsan':
                r.run(opts+[build/'test_adversarial',build/'tsan_detector_control'])
                r.run([build/'tsan_detector_control'],env=env,expected=66,contains='ThreadSanitizer: data race')
                r.run([build/'test_adversarial','threads'],env=env)
            else:
                r.run(opts+['all','demo',build/'test_adversarial'])
                if a.lane=='valgrind':
                    r.run(['valgrind','--version'])
                    vg=['valgrind','--error-exitcode=99','--leak-check=full','--show-leak-kinds=all',
                        '--errors-for-leak-kinds=definite,indirect','--track-origins=yes']
                    r.run(vg+[build/'test_core']);r.run(vg+[build/'test_adversarial','threads'])
                else:
                    r.run([build/'test_core'],env=env);r.run([build/'test_adversarial'],env=env)
                    for mode in ['spsc','ncq']:
                        r.run([build/'live_throughput_demo','--mode',mode,'--windows','2','--messages','1000','--plain'],env=env)
        r.save('PASS_WITHIN_SCOPE');return 0
    except (RuntimeError,ValueError,OSError) as e:
        r.save('FAIL_OR_INCOMPLETE');print(e,file=sys.stderr);return 1

if __name__=='__main__':sys.exit(main())
