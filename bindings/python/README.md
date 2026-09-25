# Python binding — release candidate 1.1.0

## Runtime and setup

The transport API uses ctypes from the standard library. The supplied C exporter
implements CPython's actual buffer protocol; it is not a replacement queue.
There is no CFFI/setuptools/pip runtime dependency. Build with `make python` using
the same Python interpreter and native architecture used to run the application.
The exporter supports GIL-enabled CPython >=3.11, main interpreter only. This
release was executed locally with CPython3.13.5. Free-threaded Python,
subinterpreters and PyPy are rejected/not supported, not presumed equivalent.

The source workflow needs ELITE_LIBRARY as the explicit absolute `.so`/`.dylib`
path and PYTHONPATH containing `BUILD/python` and `bindings/python`. `make
install-python` installs a matching library and exporter under the specified
prefix. Import compares every local ctypes structure's size and member offsets
against the loaded C implementation. No shared atomic structure is recreated
in Python. Header/format CRC and grant validation remain native.

## Public classes

| Class | Responsibility |
|---|---|
| `EliteShm(mode, capacity, max_payload, producers, consumers, checksum, parkable, max_backing_bytes)` | Own one managed generation; no name-only attach. `mode` is `spsc` or `ncq`. |
| `EliteSpscProducer(grant)` | One SPSC producer endpoint. |
| `EliteSpscConsumer(grant)` | One SPSC consumer endpoint. |
| `EliteNcqProducer(grant)` | One NCQ producer endpoint; one outstanding token. |
| `EliteNcqConsumer(grant)` | One NCQ work-sharing consumer endpoint. |
| `WriteLease` | Own a writable slot; explicit commit or abort. |
| `ReadLease` | Own a read-only valid payload prefix; explicit release or context-exit release. |

`shm.producer(index=0)` and `shm.consumer(index=0)` create local factory endpoints
of the appropriate named class. Consumer indexes are relative to the consumer
population. `grant(endpoint_index,pid)` instead uses the combined endpoint index
(producers first), registers self or an owned child before exposure, and returns
one fieldwise JSON-safe control record. Endpoint grants are single-use; they are
not refreshed when an endpoint closes. Deliver them only on a trusted control
channel. A grant/name/CRC is not authentication against a malicious same-user peer.

`try_reserve()` / `try_borrow()` return a lease or None. `reserve(timeout=...)` /
`borrow(timeout=...)` have finite Python-level timed polling; default0 is a try
with a WouldBlock exception. They do not silently activate native MPMC parking.
`parkable=True` selects the native SPSC format; these Python convenience methods
still use timed rechecks rather than the native wait API.

## Buffer and transfer discipline

`lease.buffer` returns a new ordinary Python memoryview directly over mapped
payload memory. A write view exposes configured capacity. A read view exposes
only the valid length and is read-only at the exporting object itself.

Two related ownership graphs are used:

`derived buffer -> C exporter -> _Pin -> public endpoint -> public manager`

`C exporter -> _LeaseState -> _EndpointLifetime -> _ObjectLifetime -> authority`

The first keeps normal application wrappers alive while a view is usable. All
container references in the exporter are visible to cyclic GC. The second holds
only native cleanup data and has no strong edge back to a public wrapper, view,
or exporter. Weak finalizers retain this independent state, not their watched
objects. Consequently GC can clear user cycles in either order without clearing
data needed to close the native lease/mapping.

One exporter owns one native view pin; its `exports` count includes both pending
acquisitions and issued Py_buffer roots. It increments before `_assert_live`
can execute Python/drop the GIL and rolls back on either callback or FillInfo
failure. Explicit close and transfer are blocked while that count is nonzero.
Slices/casts retain CPython's managed buffer, not merely the outermost view.

Finalization requests terminal closure. An existing exported alias remains valid
until it releases; no new export is admitted after finalization is requested.
The last release closes the native view pin, then performs any requested abort
or read release and deferred endpoint/object cleanup. `tp_clear` does not call
arbitrary Python cleanup; `tp_dealloc` uses resurrection-aware CPython finalizer
handling before freeing GC-managed storage.

```python
write = producer.reserve()
root = write.buffer
slice_ = root[:16]
root.release()
# write.commit(16) now raises BufferError; slice_ still owns the export.
slice_.release()
write.commit(16)
```

Explicit transfer permanently closes that exporter against future re-exports.
Destroying the outer lease while an alias remains does not recycle the slot.
After the final alias/exporter ends, ordinary finalization aborts an unpublished
write or releases a read. It never auto-commits partial construction. Retaining
`view.obj` itself can keep the exporter available for future views, so that also
retains the native lease until close/deallocation.

`WriteLease.commit(length,message_type=0,message_id=0)` validates unsigned
ranges before narrowing into C. `abort()` cancels only its unpublished write.
`ReadLease.length`, `.message_type`, `.message_id` and `.epoch` are captured under
ownership; accessing metadata properties does not reread a recycled descriptor.
`read.copy()` explicitly creates Python bytes. Context exit aborts/releases an
active lease, but refuses to force-close escaped views.

The private `_elite_buffer.create` takes a trusted native address; bypassing the
public binding or escaping an untracked pointer is unsafe. Read-only payload
exports are a correctness contract, not a security boundary against native code
or a malicious peer mapping the entire shared object. Compliant extensions must
retain their Py_buffer for as long as they use its pointer.

## Threading and interruption

ctypes native calls can release the GIL. Per-endpoint RLocks serialize calls and
shutdown; a separate authority RLock protects management. Different endpoints
can operate concurrently. The wrapper is not itself a lock-free Python runtime.
Application threads must still avoid concurrent conflicting raw buffer writes.

No fork-inherited endpoint is a valid attachment. Use subprocess spawn/exec or
multiprocessing's spawn mode with registration before delivery. Method/factory
PID checks reject inherited use; a previously exported pointer cannot be made
revocable by a Python guard. Unregistered inherited mappings remain forbidden.

Use cooperative cancellation. An asynchronous Python exception delivered after
a C transfer may lose its return value even though publication happened. The
binding then blocks new exports/retries, reports OUTCOME_UNCERTAIN and retains
its obligation. A parent authority may fence that process; there is no automatic
resend. The same rule covers reserve/borrow interruptions before Python adoption:
cleanup state is installed first and an uncertain result poisons that endpoint. Signal handlers must not re-enter endpoints or manipulate their buffers.

## Managed shutdown and crash behavior

Close all aliases, finish/abort the lease, close the endpoint, then acknowledge
its cleanup receipt to the manager. Local factory endpoints acknowledge on
normal close; externally attached children return the receipt to their parent.

For an unresolved owned child, call `shm.reap(pid,timeout)` BEFORE any external
wait/poll consumes its terminal status. A terminal native reap retires affected
generations and resolves holder fencing, not token reconstruction. SIGSTOP is
not terminal. After all child grants have acknowledged normal cleanup, use
ordinary subprocess wait instead; there is no longer an unresolved native
child registration. Process roles, actual OS handles and receipts must stay
within one responsible launcher.

`shm.successor()` retires/quarantines old storage and creates a distinct object
under the SAME authority budget. At most four objects/two quarantines are
admitted. Existing old aliases stay attached to old storage, not the successor.
`lease.abandon()` only ends explicitly retired local responsibility and does not
put an unknown token back in QF. `shm.close()` refuses pending holders and views.

Normal interpreter exit tries to close idle generations only. SIGKILL/os._exit
cannot run Python cleanup; the parent must fence/reconcile. `cleanup_failures()`
reports deliberately retained local obligations; it must be empty in a normal
closed run. Neither garbage collection nor atexit is the production control-plane
recovery guarantee. See the complete process tests for executable scenarios.

## Tests and benchmark

```sh
make CC=clang BUILD=build/release check-python
python3 bindings/python/bench_python.py --count 20000 --size 64 --out py64.json
python3 bindings/python/bench_python.py --count 10000 --warmup 100 --size 1048576 --out py1m.json
```

`bench_python.py` performs Python reserve/export/commit/borrow/release per
message. Separate native workload helpers construct and compare every payload
byte through the memoryviews. Results are **Python-driven native-workload IPC
bandwidth**, not pure-Python byte-loop serialization or a native latency result.
No payload-sized Python bytes object is created on that path. Input and output
control records are small JSON messages over setup pipes; payloads use shm only.
Each size/mode is a separately identified fresh run; one producer/consumer has
exact sequential IDs and full-byte validation. Output files are created exclusively.

The test wrapper owns its process group and has a300-second watchdog. Diagnostic
sanitizer runs may set `ELITE_TEST_TIMEOUT_SCALE=5` (bounded1..10) for control
waits; this never changes performance gates or production lease lifetimes.
ASan/UBSan and native tests are separate binaries. Clean thread diagnostics are
not a certificate of every interprocess history.


## Regression entry points

`make check-python` retains all 36 baseline tests (version expectations updated).
`make check-python-hardening` adds deterministic export races, callback/FillInfo
unwind, cyclic-GC retention and reclamation, resurrection/error probes, and
manifest negative controls. `tests/test_gc_cycle.py` exercises both queue profiles,
both lease roles, and endpoint/lease/manager cycles without manually breaking an
unreachable cycle. Original A09 race scripts are retained under `repros/`; the
historical `a09_gc_cycle.py` expects the old leak and is used only as a pre-fix
negative control, not as the post-fix pass oracle.
