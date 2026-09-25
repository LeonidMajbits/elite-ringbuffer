#!/usr/bin/env python3
"""Run the bounded chaos smoke population in a new temporary directory."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile
sys.dont_write_bytecode=True

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build',default='build/native');a=p.parse_args()
    build=Path(a.build).resolve(strict=True);root=Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix='chaos-check-',dir=build) as temp:
        out=Path(temp)/'run'
        for cmd in ([sys.executable,str(root/'tools/run_chaos.py'),'--build',str(build),'--out',str(out),'--trials','1','--smoke','--messages','10000','--storm-messages','16000'],
                    [sys.executable,str(root/'tools/verify_chaos_results.py'),str(out),'--source-root',str(root),'--build',str(build)]):
            subprocess.run(cmd,check=True)
    return 0
if __name__=='__main__':raise SystemExit(main())
