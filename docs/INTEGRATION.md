# Integration runbook — ELITEIPC 1.1.0

## 1. Choose the service contract

Use SPSC for exactly one producer and consumer per ring. Use NCQ-SC64 for
work-sharing among multiple independently registered endpoints when publication
FIFO is required. A consumer can finish processing a later claimed item first;
FIFO does not order external side effects. Neither mode supplies multicast.
Payload capacity is fixed at creation, but each message has a valid length from
zero to that capacity. Relative offsets permit independently chosen mmap bases.

Native polling has no per-message heap allocation, payload syscall or copying
transport layer. PARKABLE_SPSC pays separately for notification and waiting.
MPMC sleeping, automatic in-place crash repair, authority takeover, persistent
power-loss recovery and exactly-once effects are not supplied. Use application
identities/replay policy where delivery ambiguity matters.

## 2. Build, install, and admit the exact target

```sh
make CC=clang CXX=clang++ BUILD=build/release all benchmarks python
make CC=clang CXX=clang++ BUILD=build/release release-check
make CC=clang BUILD=build/release PREFIX="$HOME/.local/elite-1.1.0" install install-python
```

The installed tree includes three headers, static and shared libraries, and an
optional version-specific Python package/exporter. On Linux the dynamic name is
`libelite_ringbuffer.so`; on Darwin it is `libelite_ringbuffer.dylib` with an
`@rpath` install name. Linux uses the documented SONAME. Link an absolute static
archive for the simplest deployment, or configure the application's loader
rpath explicitly. `ELITE_LIBRARY` selects an exact Python native binary; a
coincidental system library is not selected silently.

Use a new BUILD directory on every compiler/flags change. Make dependencies track
source/header timestamps, not command-line compiler flags. The native Makefile
uses strict C11/warnings and keeps release, sanitizer and hook objects separate.
Darwin includes `_DARWIN_C_SOURCE` and creation-time0600 shm permissions plus
owner/mode validation. This fixes setup compatibility without changing LE128-V1.

For admitted M4 hardware only:

```sh
make CC=clang CXX=clang++ BUILD=build/apple-m4 \
  ELITE_ARCH_FLAGS='-arch arm64 -mmacosx-version-min=14.4 -mcpu=apple-m4' all python
```

The default Darwin strict-LDAR profile explicitly disables RCpc instruction
selection; `ELITE_STRICT_LDAR=0` is a separately recorded alternate mapping.
Neither option changes the source-level memory orders or guarantees processor
placement. Record compiler/SDK/OS/CPU identities, feature flags and library hashes.
The source package does not ship a prebuilt universally portable binary.

Admission checks cover little-endian32/64-bit atomic widths, supported alignment,
base-page compatibility, byte-exact header geometry, CRC, one-shot identities and
constructed grants. The128-byte software profile isolates designated cells under
its admitted granularity assumptions; it is not a claim that true-sharing traffic
or scheduler effects disappear.

## 3. C usage and ownership results

`examples/local_roundtrip.c` is a complete, compiled local two-mapping example.
`tests/test_ipc.c` is the spawned process bootstrap example. A producer's inner
operation, after an already valid connection `producer` exists, is:

```c
elite_lease lease;
elite_write_span span;
elite_result result = elite_write_reserve(producer, &lease, &span);
if (result.status == ELITE_OK) {
    unsigned char *payload = span.data;
    payload[0] = 'O';
    payload[1] = 'K';
    /* End all uses of payload before this transfer. */
    result = elite_write_commit(producer, &lease, 2, 1, 42);
    /* Check result.outcome even when result.status is not ELITE_OK. */
}
```

This snippet requires configured capacity at least2; the complete example creates
it with64. A native consumer borrows, validates/uses `span.data` only while owned,
then releases. A raw pointer is not independently revocable. Wrapper-managed
aliases call `elite_view_retain` and `elite_view_end`; plain C aliases remain an
application obligation. Do not reuse an active lease variable as the output of a
second reserve/borrow. No post-publication payload or descriptor cleanup is legal.

### Status and outcome are independent

| Status family | Meaning/action |
|---|---|
| OK | Interpret the returned ownership outcome, then continue. |
| NO_DATA_OBSERVED / NO_CAPACITY_OBSERVED | Availability observation; retry under the application's bounded policy. |
| RETIRED / COUNTER_LIMIT | No new work; finish allowed old obligations or retain/quarantine. |
| BUSY | A native call, lease, exporter or holder still prevents the requested action. |
| BAD_ABI / BAD_LAYOUT / BAD_IDENTITY / NOT_READY | Reject attachment/use; preserve any returned partial cleanup handle. |
| INTEGRITY | Stop admission, record the supported observation, retain affected ownership. |
| OS_ERROR | Keep OS error and transfer outcome; it may occur after publication. |
| OUTCOME_UNCERTAIN / AUTHORITY_REQUIRED | Stop guessing; resolve through the recorded authority/fencing policy. |
| UNSUPPORTED / INVALID_LEASE / INVALID_ARGUMENT / CREATE_CONFLICT | Reject; never silently reset or adopt another endpoint/object. |

The complete numeric registry is in `elite_api.h` and remains identical to Turn4.
PUBLISHED and RETURNED mean ownership has transferred. WRITE_OWNED, READ_OWNED,
RETAINED and UNCERTAIN do not mean it is safe to resend, reuse or delete.
For example, notification may fail after publication; retrying the whole send
based only on OS_ERROR can duplicate an application message.

## 4. C++20 usage without foreign atomics

C++ may include `elite_api.h` or `elite_ringbuffer.h` (which selects the opaque
branch). It never instantiates `std::atomic` over the C mapped representation.
The complete `examples/cpp_roundtrip.cpp` is compiled and run by `make check-cpp`.
It scopes any `std::span` before the corresponding commit/release.

```cpp
#include <elite_api.h>
#include <span>
// With a valid writable span already returned by elite_write_reserve:
{
    std::span<unsigned char> bytes(
        static_cast<unsigned char*>(write_span.data), write_span.capacity);
    bytes[0] = 'O';
    bytes[1] = 'K';
} // Caller promises no span/iterator/raw alias survives this scope.
elite_result result = elite_write_commit(producer, &lease, 2, 1, 42);
```

A C++ span destructor does not automatically revoke copies; the caller must enforce
that discipline. The example's errors are reported and fail closed, rather than
an exception destructor pretending it can repair an uncertain native transfer.
Use explicit cleanup/error handling in production launchers.

## 5. Python direct buffers

The runnable `examples/python_roundtrip.py` covers both named profiles. The five
requested classes are exported. Local factories return those same classes; raw
class construction takes a trusted fieldwise grant, never only a filename.

```python
import struct
from elite_ringbuffer import EliteShm

with EliteShm("ncq", capacity=8, max_payload=64) as shared:
    with shared.producer() as producer, shared.consumer() as consumer:
        with producer.reserve() as write:
            with write.buffer as view:
                struct.pack_into("<QQ", view, 0, 42, 99)
            write.commit(16, 1, 42)
        with consumer.borrow() as read:
            with read.buffer as view:
                values = struct.unpack_from("<QQ", view)
            assert values == (42, 99)
```

`pack_into` constructs bytes in the slot. `unpack_from` creates scalar Python
results, not an intermediate transport payload. Creating `bytes(view)` or using
`read.copy()` is a copy and is labeled accordingly. In a Python application that
uses native parsers/codecs, those extensions can consume the memoryview directly
provided they obey its buffer lifetime. A1MiB native-workload demonstration is
not a promise of similar speed for a64-byte Python application loop.

Each endpoint serializes native operations with a local RLock because ctypes can
release the GIL. This does not turn the Python wrapper into a wait-free runtime.
Derived memoryviews retain the exporter, which retains both the lease and mapping.
The read-only property is enforced by the exporter itself. An escaped view causes
BufferError/BusyError rather than forced commit/release/detach. Destructor and
atexit behavior is best-effort; explicit contexts and receipts are the operational
contract. Use only a main, GIL-enabled supported CPython interpreter.

## 6. Launch and shutdown sequence

1. Create one application-owned authority/cohort; allocate validated fresh backing.
2. Spawn/exec each owned worker before exposing its grant. Register its actual PID
   and incarnation while still owned; no unregistered inherited mapping.
3. Send the one-shot fieldwise grant on a trusted control channel. Child attaches
   and reports readiness. Pipes here carry control, not payload messages.
4. Exchange payloads through the mapped ring. Each endpoint holds at most one token.
5. Stop new offers; finish/drain within the application policy. Close every view,
   commit/abort/release as appropriate, then detach each endpoint.
6. Collect complete cleanup receipts and acknowledge them. Once all normal child
   grants are acknowledged, ordinary subprocess wait can reap the child.
7. If a holder exits without acknowledgment, use the native owned-child reaper
   before another wait/poll consumes its status. Confirmed terminal evidence
   retires affected generations. A stopped process is not a dead process.
8. Seal/delete only after every potential holder, including pre-enrollment grants,
   is accounted. An attachment count of zero is not sufficient on its own.

`tests/test_python_bindings.py` and `bindings/python/bench_python.py` implement
both control patterns. Never use a generic process-name kill or global shm-name
cleanup as a substitute for that per-cohort ledger.

## 7. Failure runbook

**Unobserved producer failure:** partial unpublished data is not delivered. Healthy
ready work may continue while spare capacity exists. The failed token may remain
stranded. Do not log it as corrupt unless an authorized integrity observation
supports that classification.

**Confirmed failure with uncertainty:** retire the old generation. Fence all
registered holders or keep old storage in quarantine. A distinct successor uses
fresh identity/backing and shares the existing authority budget. Do not map it
over old virtual addresses while retained aliases remain. Report old delivery
uncertainty or gaps; no automatic cross-generation replay guarantee exists.

**Asynchronous return loss:** if C may have crossed publication before a Python
signal interrupts receipt processing, prohibit re-export and resend. The binding
retains an UNCERTAIN obligation. Restart/fence the affected process through the
responsible authority. Exception handlers must not guess the LP outcome.

**Authority failure:** stop. This release has no automatic authority takeover and
cannot safely reset a four-object allowance over unknown existing resources.
Reconcile/fence first. At most four backing objects and two quarantines belong
to a replacement cohort, including failed/staged allocations.

**Resource/clock/placement anomaly:** preserve the actual run. Do not delete slow
samples or substitute a target CPU flag for observed placement. Profiling and
sanitizers are diagnostic builds, not the primary timing distribution.

## 8. Rollout and rollback

Verify both manifests before building. Run target compilation, layout checks,
local/native IPC and alias/crash suites. Record exact release/library/exporter
hashes, compiler/SDK/interpreter and role counts. Use a canary cohort with the
same payload/schema and process topology as deployment. Existing native Apple
lab results are valuable prior evidence, not execution of the new Python adapter.

Roll back by stopping/grant-closing the canary, resolving or quarantining all
holders and starting the earlier binary in a NEW generation. Never downgrade an
active object's bytes, reset epochs, or replace an active process's mapped library
under its borrowed views. Preserve failures and cleanup obligations in the audit.

## 9. Measurements and limits

Use `docs/BENCHMARKS.md` for native RTT and validated cohort goodput. RTT is not
one-way latency. `bindings/python/bench_python.py` measures Python per-message
FFI/buffer work plus native direct full-payload construction/comparison, with
exact single-producer identities, warmup, common scheduled start and final
release endpoint. Goodput counts delivered application bytes once, not DRAM
traffic. All timing remains host/load-specific and finite-run evidence.

Run the full source verification with `python3 tools/verify_release.py .`.
No proprietary SDK, binary runtime, credential, native library or multi-GB raw
Turn7 evidence set is included in this source distribution. Existing report/data
identities remain in the retained historical evidence.


## 1.1.0 hardening admission

Rebuild both the native library and the CPython exporter. Python checks a matching
1.1.0 runtime/exporter revision; a stale extension must not bypass the fix. The
wire ABI and queue algorithm remain unchanged. Run `make check-python-hardening`
and `python3 tools/verify_release.py` in addition to native admission tests.

Cyclic collection is not forced reclamation. `_Pin`, public endpoint and manager
objects may participate in arbitrary application cycles. Their weak finalizers
retain separate native cleanup state, never the watched public object. A live
buffer retains its mapping and lease; finalization waits for the final buffer
release. `tp_clear` severs Python references only after exports have ended and
does not execute a second native transfer. See the Turn 10 report for the state
machine, callback failure rules, and regression matrix.

An interrupted reserve/borrow result poisons the local endpoint, reports a busy
or uncertain obligation, and does not infer that no token was acquired. Fence
that process through its authority; do not manually clear private fields.
`cleanup_failures()` is empty for the passing normal lifetime/GC scenarios, but
is deliberately populated when an injected native ownership outcome is unknown.
That is fail-closed accounting, not successful automatic recovery.
