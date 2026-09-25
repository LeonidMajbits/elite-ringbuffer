"""Owned subprocess only: deterministic bf_getbuffer vs lease-transfer witness.
Trace hook ONLY pauses at an existing Python return; no source/state patches,
raw-address construction, signal-handler reentry, or unsupported runtimes.
Usage: export_race.py spsc|ncq write|read
A corrected exporter should reject the competing transfer and exit zero.
The audited exporter admits it, returns a NULL-address view, and crashes.
"""
import sys,threading,struct,json
from elite_ringbuffer import EliteShm
mode=sys.argv[1];kind=sys.argv[2]
shm=EliteShm(mode,capacity=4,max_payload=64);p=shm.producer();c=shm.consumer()
print('SHM_NAME='+shm.name,flush=True)
w=p.reserve()
if kind=='read':
    with w.buffer as v:struct.pack_into('<Q',v,0,71)
    w.commit(8);lease=c.borrow()
else:lease=w
v=lease.buffer;ex=v.obj;v.release()
ready=threading.Event();done=threading.Event();result=[];errors=[]
def other():
    fired=False
    def trace(frame,event,arg):
        nonlocal fired
        if not fired and event=='return' and frame.f_code.co_name=='_assert_live':
            fired=True;sys.settrace(None);ready.set()
            if not done.wait(5):raise RuntimeError('schedule timeout')
        return trace
    try:
        sys.settrace(trace);result.append(memoryview(ex))
    except BaseException as exc:errors.append(repr(exc))
    finally:sys.settrace(None)
t=threading.Thread(target=other);t.start()
if not ready.wait(5):raise RuntimeError('live-check checkpoint not reached')
blocked=False
try:
    if kind=='write':lease.commit(8,message_id=99)
    else:lease.release()
except BufferError:blocked=True
finally:done.set()
t.join(5)
if t.is_alive():raise RuntimeError('worker did not join')
print(json.dumps({'mode':mode,'kind':kind,'transfer_blocked':blocked,'closed':ex.closed,'active':lease.active,'exports':ex.exports,'errors':errors}),flush=True)
if blocked:
    for v in result:v.release()
    if kind=='write':lease.abort()
    else:lease.release()
    p.close();c.close();shm.close();sys.exit(0)
if not result:raise RuntimeError('missing new buffer')
v2=result[0]
print(json.dumps({'new_view_length':len(v2),'readonly':v2.readonly}),flush=True)
if kind=='write':struct.pack_into('<Q',v2,0,123)
else:struct.unpack_from('<Q',v2)
raise RuntimeError('invalid view unexpectedly survived access')
