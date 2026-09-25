"""Raise at an actual Python bytecode boundary after native reservation.
A Python trace hook makes an asynchronous exception window deterministic;
no production source is changed and no private native function is invoked.
"""
import sys,json,os
from elite_ringbuffer import EliteShm
s=EliteShm(capacity=2,max_payload=64);p=s.producer();c=s.consumer()
print(json.dumps({'name':s.name,'pid':os.getpid()}),flush=True)
fired=False
def trace(frame,event,arg):
    global fired
    if frame.f_code.co_name=='_obtain' and event=='line' and not fired and 'r' in frame.f_locals:
        fired=True;sys.settrace(None);raise KeyboardInterrupt('after native reserve, before Python ownership adoption')
    return trace
sys.settrace(trace)
try:p.reserve()
except KeyboardInterrupt as exc: print(json.dumps({'caught':str(exc)}),flush=True)
finally:sys.settrace(None)
print(json.dumps({'public_busy_property':p.busy}),flush=True)
for label,action in [('reserve_again',p.try_reserve),('close',p.close)]:
    try:action(); print(json.dumps({'operation':label,'unexpected':'succeeded'}),flush=True)
    except BaseException as e: print(json.dumps({'operation':label,'error':str(e),'status':getattr(e,'status',None)}),flush=True)
# Deliberately exit, leaving outer runner to unlink this child's unique name.
os._exit(0)
