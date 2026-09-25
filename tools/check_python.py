#!/usr/bin/env python3
"""Own the complete test process group; bound even failed child/control paths."""
import os,pathlib,signal,subprocess,sys
root=pathlib.Path(__file__).resolve().parents[1]
if '--hardening' in sys.argv:
    command=[sys.executable, '-m', 'unittest', 'discover', '-s', str(root/'tests'),
             '-p', 'test_*.py', '-v']
else:
    command=[sys.executable,str(root/'tests/test_python_bindings.py')]+sys.argv[1:]
def termination(signum,frame):
    raise SystemExit(128+signum)
signal.signal(signal.SIGTERM,termination)
p=subprocess.Popen(command,start_new_session=True)
try:
    result=p.wait(timeout=300)
except BaseException:
    try:os.killpg(p.pid,signal.SIGKILL)
    except ProcessLookupError:pass
    p.wait()
    raise
# On failed test cleanup, never leave this owned group's children running.
try:os.killpg(p.pid,signal.SIGKILL)
except ProcessLookupError:pass
sys.exit(result)
