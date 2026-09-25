#!/usr/bin/env python3
"""Install, relocate, then build and run independent C/C++ package consumers."""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build',type=Path,required=True);p.add_argument('--work',type=Path,required=True)
    a=p.parse_args();w=a.work.resolve();w.mkdir(parents=True,exist_ok=False)
    old=w/'original-prefix';new=w/'relocated-prefix'
    def run(args,env=None):subprocess.run([str(x) for x in args],check=True,timeout=120,env=env)
    run(['cmake','--install',a.build,'--prefix',old]);old.rename(new)
    run(['cmake','-S',ROOT/'tests/cmake_consumer','-B',w/'consumer',f'-DCMAKE_PREFIX_PATH={new}'])
    run(['cmake','--build',w/'consumer','--parallel','2'])
    run(['ctest','--test-dir',w/'consumer','--output-on-failure'])
    run([new/'bin/live_throughput_demo','--windows','1','--messages','1000','--plain'])
    if not (new/'share/man/man3/elite_ringbuffer.3').is_file():raise ValueError('missing section 3 man page')
    if not (new/'share/man/man7/eliteipc.7').is_file():raise ValueError('missing section 7 man page')
    pcs=list(new.rglob('elite_ringbuffer.pc'))
    if len(pcs)!=1:raise ValueError('pkg-config inventory')
    if shutil.which('pkg-config'):
        env={**os.environ,'PKG_CONFIG_PATH':str(pcs[0].parent)}
        r=subprocess.check_output(['pkg-config','--variable=prefix','elite_ringbuffer'],env=env,text=True).strip()
        if Path(r).resolve()!=new:raise ValueError('nonrelocatable pkg-config prefix')
    print('PASS: relocated C/C++ imported target, installed demo and man pages')
    return 0
if __name__=='__main__':sys.exit(main())
