"""Retain sys.unraisablehook.object while exporter cleanup is interrupted.
No native pointers/factory mutation. A trace hook simulates a Python exception
inside the exporter cleanup callback; a deliberately retaining custom unraisable hook saves it.
Python documentation warns against retaining unraisable.object; this is an
explicit hostile/buggy-hook hardening probe, not default-hook behavior.
"""
import sys,os,json,gc
from elite_ringbuffer import EliteShm
s=EliteShm(capacity=2,max_payload=64);p=s.producer();c=s.consumer();w=p.reserve()
v=w.buffer;v.release()
print(json.dumps({'pid':os.getpid(),'name':s.name}),flush=True)
saved=[]
def unraisable(args):
    if type(args.object).__name__=='LeaseBuffer':
        saved.append(args.object)
        print(json.dumps({'hook_retained_exporter':True}),flush=True)
sys.unraisablehook=unraisable
fired=False
def trace(frame,event,arg):
    global fired
    if frame.f_code.co_name=='_drop_view_pin' and event=='call' and not fired:
        fired=True;sys.settrace(None);raise KeyboardInterrupt('cleanup callback interruption')
    return trace
sys.settrace(trace)
del w
sys.settrace(None)
print(json.dumps({'saved_objects':len(saved),'trace_fired':fired}),flush=True)
if saved:
    # This must either remain a valid closed exporter or reject safely.
    print(json.dumps({'retained_closed':saved[0].closed}),flush=True)
os._exit(0)
