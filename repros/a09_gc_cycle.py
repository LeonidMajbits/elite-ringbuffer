"""Application-level reference cycle retaining an exporter-owned lease."""
import gc,json,weakref
from elite_ringbuffer import EliteShm,cleanup_failures
shm=EliteShm(capacity=4,max_payload=64);p=shm.producer();c=shm.consumer();w=p.reserve();v=w.buffer
# Application caches its payload view on the endpoint. No private state touched.
p.cached_view=v
rp,rw=weakref.ref(p),weakref.ref(w)
print(json.dumps({'exporter_gc_tracked':gc.is_tracked(v.obj)}))
del v,w,p
for _ in range(3):gc.collect()
print(json.dumps({'outer_lease_collected':rw() is None,'endpoint_collected':rp() is None,'endpoint_still_busy':rp().busy,'cleanup_failures':cleanup_failures()}))
# Prove retention was not an actually external live root, and finish cleanly.
p=rp();p.cached_view.release();del p.cached_view;gc.collect()
p.close();c.close();shm.close()
print('explicit cycle break cleaned all resources')
