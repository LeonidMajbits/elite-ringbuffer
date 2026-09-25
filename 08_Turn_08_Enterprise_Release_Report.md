# Turn 08 — Enterprise Release Report
## ELITEIPC 1.0.0: final C distribution, tracked Python buffers, and deployment bundle

**Project:** Leonid Majbits / Gemini Operator Lab, paired with Leon  
**Date:** 2026-09-23  
**Stage:** 8 of8 — final source-release packaging and bindings  
**Release version:** 1.0.0; shared format LE128-V1, ABI `0x00010000`  
**Disposition:** Delivered final project source with scoped executed validation. Not an unqualified hard-real-time, all-platform, or all-planned-tests production certificate.  
**Canonical-SHA256:** `b06aa9f099ecd19ea45e59b9ca1286764218b0ad02a789a09e5a470d4296fcc8`
**Hash convention:** Replace only this header digest's64 hexadecimal characters with ASCII zeros before hashing. The adjacent sidecar authenticates literal finalized bytes.  
**Verified parent:** Turn7 benchmark ZIP,3,799,276 bytes; SHA256 `816d48047aaba90f806a91345c92679d6e7b0d84377d580c69e5335be9d08e9d`;397 inherited manifest entries checked.  
**Destination:** Deployment Research / Elite_Systems_LockFree_RingBuffer.  
**Evidence convention:** [P4]–[P7] are preserved project documents and prior evidence. [A8] is the user's supplied native Apple summary, separately classified LAB_REPORTED. `evidence/turn08/` contains newly executed local results. Design explanations describe the delivered source. External primary references are listed in§12. No upload success is predicted by this immutable report; a separate deposit receipt follows actual transfer.

---

## 1. Executive release verdict

The final distribution adds the missing language/runtime boundary without
replacing the queue: an opaque C/C++ interface,1.0.0 version symbols, exact local
FFI-layout introspection, and a ctypes Python API backed by a small genuine
CPython buffer exporter. The five requested Python classes are supplied, along
with concrete write/read leases, tests, a two-process throughput driver and
C/C++/Python integration examples.

The six inherited native C implementation units remain byte-identical to the
verified Turn7 source. The new `elite_version.c` supplies process-local
introspection and versioning. Header organization changes expose the existing
local-call interface to C++, while the C11 shared layout,103 assertions, source
orders, counters, phases and queue linearization points remain unchanged.
`CORE_DELTA_TURN_08.json` records the exact before/after identities. The Makefile
also incorporates the lab's `_DARWIN_C_SOURCE` correction and retains Turn7's
credited Darwin creation-permission handling.

The substantive new guarantee is narrower than “a pointer wrapped in Python”:
**a live admitted buffer alias prevents both slot recycling and mapping cleanup**.
Slices, casts, independent memoryview roots, and ctypes consumers were exercised.
Read views are read-only at the exporter, including a new memoryview made from
`view.obj`. A closed exporter cannot later resurrect a transferred lease.

Linux GCC/Clang strict builds and36 Python binding tests passed. A scoped
ASan/UBSan26-test lifetime lane and an isolated2P/2C test passed; full combined
instrumented runs were incomplete or hit a harness timeout and remain recorded
as such. Normal Python/C/C++ examples and a staged installation were exercised.
New Apple execution of this binding/exporter/installation has not occurred here.
The user's native Apple100M summary is retained as valuable lab evidence rather
than invented raw-data readback for the new code.

New1MiB Python-driven native-workload trials delivered multi-GB/s on the local
Linux machine, with Python per-message reserve/export/commit/borrow/release and
full native payload construction/comparison. The same adapter delivered only
roughly40–60 thousand messages/s for smaller payloads in these trials. Both are
reported. This binding amortizes language-boundary overhead on large messages;
it does not preserve native C's tens of millions of small messages/s.

## 2. Distribution and build contract

| Component | Delivered artifact |
|---|---|
| Complete C11 mapped ABI | `include/elite_ringbuffer.h` |
| C/C++ opaque calling interface | `include/elite_api.h` |
| Semantic version macros | `include/elite_version.h` |
| Runtime version/local-layout access | `src/elite_version.c` |
| Frozen native data/lifecycle implementation | `src/elite_core.c`, `elite_format.c`, `elite_spsc.c`, `elite_mpmc_ncq.c`, `elite_shm.c`, `elite_wait.c` |
| Python classes and ctypes declarations | `bindings/python/elite_ringbuffer/` |
| Retaining, read-only-aware exporter | `bindings/python/elite_buffer.c` |
| Native and Python benchmarks | `benchmarks/`, `bindings/python/bench_python.py` |
| Public examples and runbooks | `examples/`, `docs/INTEGRATION.md`, binding README |
| New tests/evidence | `tests/test_python_bindings.py`, `evidence/turn08/` |
| Integrity tooling | `tools/verify_release.py`, source and full manifests |

`ELITE_VERSION_MAJOR`, `ELITE_VERSION_MINOR`, `ELITE_VERSION_PATCH` are1,0,0.
`elite_version_string()` and `elite_version_number()` expose the matching
runtime release. The release number and shared-format ABI are distinct version
concepts even though their initial major/minor values coincide. Future semantic
versions must not silently alter live shared objects or reserved bytes.

The static output is `libelite_ringbuffer.a`. The dynamic output is
`libelite_ringbuffer.so` on Linux or `libelite_ringbuffer.dylib` on Darwin. Normal
native objects use PIC, strict C11, `-O3 -Wall -Wextra -Werror -pedantic`, and the
existing stronger conversion/prototype warning policy. C++ examples are compiled
as C++20 with warnings as errors. The optional CPython exporter also compiles
as C11 with the requested strict warnings. Its headers/runtime are development
or optional binding dependencies, never linked into the normal native library.

The Darwin Makefile specifies its14.4 deployment baseline, platform feature
macro, dylib install name and compatibility/current version. Existing strict
LDAR target-feature selection is retained, not replaced by stronger source
atomics. GCC is a Linux cross-check, not an unperformed Darwin-runtime claim.
Fresh BUILD directories are required when flags/toolchains change because Make
is not a compiler-option fingerprinting system.

`make install install-python` was tested into a prefix containing spaces. The
installed package imported and transferred a payload without ELITE_LIBRARY by
loading its explicitly installed native sibling. Both C++ header entry paths
were compiled successfully. No system-wide install or privileged mutation was
needed. Proprietary SDKs, prebuilt native/exporter libraries, sanitizer runtimes,
credentials and the huge historical raw benchmark archive are not shipped.

## 3. Frozen architecture and memory ownership

SPSC keeps one publication count P and one reclamation count C. The writer's
reserve is private and has no shared reservation CAS. It owns the slot while
constructing, release-publishes P after completion, and does not touch its
mutable bytes afterward. The reader uses a covering acquire and retains C until
its final read alias ends. Cached observations are conservative, not wall-clock
freshness guarantees. The invariant remains0≤P−C≤N.

NCQ-SC64 keeps a fixed payload pool plus free and ready index queues. A producer
obtains one free block, completes its bytes, then atomically installs a complete
cycle/index entry. That entry installation—not a prior claim-first ticket—is
publication. Tail advancement follows and can be helped by another participant.
A reader owns the captured index only after winning its head CAS. Every failed
CAS invalidates its dependent observation set. All queue metadata operations
remain strong sequentially consistent reference operations.

For either internal queue, the ghost publication frontier U satisfies
H≤U,0≤U−H≤N,U−1≤T≤U. H=T+1 can be temporarily valid. The entry is already readable
when its publisher is paused before tail help. Arbitrarily long payload
construction holds a resource outside the ready queue rather than an
indispensable unpublished FIFO position. This does not mean a crashed producer
cannot strand a payload block. [P4; P6]

The shared headers remain2,048 bytes, the immutable prefix512 bytes, slot and
queue-entry cells128 bytes, and participant records256 bytes. The16,384-byte
format quantum is not a huge-page claim. Magic octets spell ELITEIPC; header
CRC32 covers only the immutable prefix with its own field canonicalized to zero.
Optional CRC64 scans only the valid payload prefix. No payload pointer or
language object is stored as a portable mapped value.

### The two memory-order chains are unchanged

For SPSC publication:

W(payload_k) →sb release(P covering k) →sw acquire(P covering k) →sb R(payload_k).

For recycling:

R(payload_k,last alias) →sb release(C covering k) →sw acquire(C covering k) →sb W(payload_k+N).

NCQ obtains the corresponding forward chain through QR publication/acquire/claim,
and the reverse chain through QF return/acquire/claim. Its descriptor phase
validates the current owner; phase is not an independent queue-membership oracle.
The Python exporter extends the last-alias boundary in these existing chains.
It does not invent a new fence, weaken an atomic, or write into reserved ABI bytes.

A memory fence orders appropriate memory effects. It cannot revoke an old raw
pointer, prove process death, or make old bytes safe to reassign. The release
still has no slot-revocation operation. All CPU/OS/atomic ABI assumptions and
finite nonwrapping counters remain part of target admission. [P4,§§8–13]

## 4. Python buffer contract and why the extra exporter exists

ctypes is used for the standard C calling interface, not for an unsafe array
created with `from_address` and discarded at context exit. Pure wrapper scope
cannot account for all derived buffers. A small CPython C exporter provides the
actual getbuffer/releasebuffer interface and an explicit closed state. It hands
a buffer consumer a strong owning exporter reference and rejects incompatible
writable requests. This uses the public buffer protocol contract. [R1]

The normal reference graph is:

`memoryview or derived consumer → exporter → lease pin → endpoint → mapping`.

A local factory endpoint additionally retains its manager. Endpoint-to-current-pin
references are weak, so the ordinary wrapper graph has no owner/exporter cycle.
Each exporter holds one explicit native view retain. Buffer exports keep that
exporter alive; its close method cannot succeed while any export remains. The
last exporter cleanup ends the native view pin, after which native commit,
abort or release is permitted. Merely ending the first memoryview is insufficient.

The five classes requested by the commission are public. `EliteShm` owns a managed
generation. Producer classes expose `try_reserve()`/`reserve(timeout)` returning
WriteLease; consumer classes expose `try_borrow()`/`borrow(timeout)` returning
ReadLease. `lease.buffer` is an ordinary direct memoryview, not a custom object
that only resembles the buffer interface. Writable views expose capacity; read
views expose only the published valid prefix. Metadata is captured under native
ownership and can be inspected without rereading recycled bytes.

Tests retain slices after closing their root, retain casts, create several root
views, retain a ctypes from_buffer consumer, and retain `view.obj` without a
memoryview. In each case an attempted transfer must remain blocked until the
last relevant exporter capability ends. Tests also drop the outer lease or
endpoint variable and confirm the still-live view retains safe storage.
Consumer exports reject mutation even through a freshly created view of the
underlying exporter. Duplicate transfer and post-close re-export are rejected.

Normal finalization aborts an uncommitted write or releases a completed read;
it never auto-commits a partial payload. Explicit context exit may raise
BufferError when a view escaped, rather than lying about successful cleanup.
`ReadLease.copy()` is an explicitly copying convenience. Raw/native pointer
escape, monkeypatching the private factory, or a third-party extension releasing
its Py_buffer before its final pointer use violates the contract. Readonly does
not isolate a malicious peer with access to the entire writable mapping.

### Threading, interpreter and ABI boundaries

ctypes library calls can release the GIL. The wrapper serializes each endpoint
and shutdown with a process-local RLock; management has a separate lock. This
prevents concurrent misuse of one native single-owner handle, not all possible
races in caller-written bytes. It also means Python call-level lock-freedom is
not claimed. Different endpoints/processes can run independently. [R2]

The binding supports main-interpreter, GIL-enabled CPython≥3.11 and was executed
locally with3.13.5. Free-threaded builds, subinterpreters and PyPy are not silently
accepted. The exporter is built for the selected interpreter and architecture.
Import verifies the loaded native release plus every size/member offset of all
eight local-call ctypes structures. Shared C atomics are never overlaid by Python
or C++ atomic types. `elite_binding_info` uses an aligned C metadata object so
ctypes is not asked to fake a128-byte-aligned shared-header output allocation.

Use spawn/exec and managed grants. An inherited method call is rejected by PID
checks, but an already-exported pointer cannot be revoked by such a check;
unregistered fork descendants remain forbidden. Cooperatively stop workers.
If a Python exception arrives after a native transfer may already have linearized
but before its return is assigned, the wrapper marks the outcome unknown,
forbids re-export/retry, and retains the obligation for authority fencing. The
suite injects this return-loss case after a real successful publication and
checks that only one message is delivered.

## 5. New Python throughput: direct views, explicit workload

Each measured size below used two spawned Python processes and one native ring,
POLL_ONLY, checksum NONE, with Python API/buffer operations on every message.
The exporter module's separate workload helper constructs or compares **every
payload byte** directly in the view. This is no intermediate payload-sized Python
bytes allocation, but it is also not pure-Python byte-loop serialization. The
native helper is a data-generation/check workload, not a substitute transport.

Each condition used a fresh generation, a warmup/drain without resetting shared
state, a scheduled start100ms ahead, exact sequential IDs and full-byte validation,
and final release/cleanup receipts.64-byte trials used20,000 messages after1,000
warmups;4KiB used10,000/1,000;1MiB used10,000/100. Capacity was1,024 for small
messages and8 for1MiB. Results include start skew, Python calls, view allocation,
native construction/verification and worker endpoint time. They do not include
process startup or offline JSON output. No percentile is inferred from rate.

| Mode | Payload bytes | Messages | Messages/s | Delivered GB/s |
|---|---:|---:|---:|---:|
| SPSC | 64 | 20,000 | 46,971.36 | 0.003006 |
| NCQ | 64 | 20,000 | 39,491.73 | 0.002527 |
| SPSC | 4,096 | 10,000 | 59,455.23 | 0.243529 |
| NCQ | 4,096 | 10,000 | 56,635.93 | 0.231981 |
| SPSC | 1,048,576 | 10,000 | 6,181.55 | 6.481827 |
| NCQ | 1,048,576 | 10,000 | 5,724.71 | 6.002796 |

The local tuple is Linux/x86-64, CPython3.13.5, Clang17 release library/exporter,
shared container with four CPU-time equivalents. No CPU pinning or whole-machine
exclusive service was claimed for these Python runs. Each is one finite trial,
not a5-repeat population-performance certification. Delivered GB/s counts payload
once; it is not DRAM, write-plus-read, or interconnect bandwidth.

`FINAL_PYTHON_64.json`, `FINAL_PYTHON_4096.json` and
`FINAL_PYTHON_1048576_RETRY.json` retain worker timestamps, counts, receipts,
source/library/exporter hashes and exact metric descriptions. Earlier developmental
helper/rate results remain in evidence and are not substituted for final-source
measurements. A combined outer invocation ended during its1MiB condition without
producing a complete JSON; the isolated retry above is separately recorded.
The brief summary-print command also initially used an incorrect JSON field name;
that reporting error did not alter the successfully written trial artifacts.

## 6. Unified native Linux/Apple comparison

Linux figures below are the preserved Turn7 measured/replayed100M results, not
new Turn8 timing. Apple figures are exactly the user's Turn8 native lab summary
[A8], recorded in `APPLE_LAB_REPORTED.json`. The lab reports all seven100M
conditions passed replay, but did not supply the new raw traces, machine/SDK/
compiler/binary manifest, or the2P/2C and4P/4C rates in the commission. Those two
rates stay unreported. An M4 tuning discussion does not establish the actual chip.

### Instrumented RTT, nanoseconds

| Mode / source | p50 | p90 | p99 | p99.9 | p99.99 | Maximum |
|---|---:|---:|---:|---:|---:|---:|
| SPSC / Linux measured [P7] | 660 | 791 | 864 | 20,924 | 277,748 | 213,535,770 |
| SPSC / Apple lab-reported [A8] | 250 | 250 | 625 | 750 | ~7,210 | ~1,330,000 |
| NCQ / Linux measured [P7] | 805 | 896 | 1,060 | 43,247 | 410,436 | 145,048,128 |
| NCQ / Apple lab-reported [A8] | 375 | 542 | 1,250 | 1,580 | ~11,880 | ~1,840,000 |

Each RTT condition means100M completed request/reply cycles and200M directional
records. Origin times before reservation through final response validation and
release. It is not one-way handoff. Apple tail/max figures above were supplied
in rounded µs/ms and converted for the table; extra displayed digits are not
extra measurement precision. The quoted38.5× and roughly35× comparisons concern
**p99.99**, not the maxima. Rounded Apple values limit ratio precision.

Linux used a Xeon8370C shared container with five allowed logical CPUs and a
four-CPU-time quota; its workers were assigned CPU IDs0–3. Apple was described
as bare metal with user-initiated QoS. This compares complete observed deployments,
not a controlled ISA-only experiment. Scheduler, topology, load, timer granularity
and environment differ. Native Apple NCQ p99 is higher than the Linux value in
these summaries even though its far tail is lower; the distribution cannot be
replaced by one universal “faster” factor.

### Validated native application goodput

| Profile | Linux measured messages/s | Apple lab-reported messages/s | Apple delivered GB/s |
|---|---:|---:|---:|
| SPSC1P/1C | 9,480,193.239 | 32,066,903 | 2.052282 |
| NCQ1P/1C | 7,669,928.662 | 26,599,872 | 1.702392 |
| NCQ2P/2C | 4,659,155.018 | Not supplied | Not supplied |
| NCQ4P/4C | 3,370,411.001 | Not supplied | Not supplied |
| NCQ8P/8C | 3,575,175.822 | 4,105,985 | 0.262783 |

All goodput rows are64-byte application payloads. Native100M membership is
established by the stated full validation/bitmap oracle, not inferred from rate.
The lab's3.11s/3.76s elapsed descriptions are rounded; rates are retained as
supplied rather than recomputed from rounded times. Python's table in§5 is a
different workload/size/population and is not blended into these native rows.

### Cache evidence

[A8] reports35.5M independent relaxed RMW/s for8-byte stride and4,039,638,957
RMW/s for128-byte stride, approximately113.8× in that counter control. This is
strong evidence that placement matters for that workload. It does not measure
every IPC address/ownership transition or prove universal absence of false
sharing. The unchanged layout proof covers designated disjoint cells under its
admitted granularity assumptions; contenders on the same NCQ control still
perform true sharing. No additional Apple PMC trace or hard P-core pinning was
created in Turn8. [P7,§§4–5]

## 7. Executed validation and retained failures

| Lane | Turn8 evidence / scope |
|---|---|
| Native strict GCC14.2 and Clang17 builds | Static/shared libraries, new version/introspection, inherited layout assertions and test programs compile |
| C++20 | Opaque API example and public ringbuffer-header include compile/run; g++ and explicit clang++ checks |
| Native regression | Core68 groups,512 prefix mutations,100k SPSC,100k NCQ4/4, hooks8, limits4, chaos3, parser1M, benchmark tools8 |
| Python API/lifetime/process suite |36 tests pass with GCC and Clang native builds; final Clang source run retained |
| ASan+UBSan scoped lifetime |26 tests pass with BOTH native library and exporter instrumented; leak detector disabled |
| ASan+UBSan isolated process test |One2P/2C exact-membership test passes; not full process-suite qualification |
| Full instrumented suite attempts |Incomplete outer-budget runs retained; one10-test process attempt reached a20-second throughput-child timeout; no full-suite pass claimed |
| Installation |Explicit prefix containing spaces; Python import/transfer without environment-selected library, C++ installed-header linkage passes |
| Python performance |Six final size/mode conditions, exact IDs/full native byte checks and completed cleanup; source/binary hashes retained |
| New Apple binding/exporter |NOT_RUN here; native lab results do not certify this new runtime layer |
| New Python TSan |NOT_RUN; prior native/thread or lab-reported sanitizer results are not relabeled as Python IPC certification |

All36 binding tests use explicit assertions that are active in their test
invocation. Core/result safety does not depend on removable C assertions.
The test wrapper bounds its owned process group; nonblocking, deadline-controlled
child-record reads avoid a select-then-blocking-readline trap. Diagnostic test
budgets may be scaled separately for sanitizers, never for native performance
acceptance or lease expiration.

The earliest binding test iteration treated native NOT_READY incorrectly during
child reaping and attempted new work after a confirmed reap had retired the
generation. These wrapper/test errors were fixed before final validation. The
failed log remains. Later full ASan attempts exceeded the outer execution budget;
a retry completed nine process tests but timed out its throughput subprocess.
A further diagnostic-budget attempt was incomplete. These are not converted
into clean full-suite evidence, and no unobserved cause is asserted. Successful
scoped sanitizer results are identified separately. Leak detection is explicitly
off in that instrumented CPython environment; no “zero leaks in all runtimes”
certificate is inferred.

The completed native and Python normal-closure tests verify returned status,
receipts and no binding cleanup-failure registry entries. Crash tests explicitly
permit stranded tokens before whole-generation reclamation. They exercise
unobserved SIGKILL, terminal fencing, SIGSTOP/SIGCONT with a distinct successor,
normal process exit and asynchronous return-value loss. No timeout reclaim,
phase scanner or guessed count decrement is introduced to make a test pass.

## 8. Crash resilience and cleanup runbook

Normal operation holds a token until every relevant buffer ends. A normal
producer context aborts unused construction; a read context releases after use.
Endpoint detach follows final alias closure and returns a cleanup receipt.
Manager acknowledgement and mapping cleanup are distinct steps. The combined
admission state/count prevents an enrollment from slipping through a closed
READY gate, but pending grants remain separate lifetime obligations.

For unacknowledged children, the responsible authority uses its actual owned
process registration to observe terminal exit. A mere PID lookup or timeout
is not sufficient. The Python `reap` method returns None for nonterminal state
and resolves a terminal holder through the native manager. It also retires
that generation; applications must not mistake reap for permission to continue
indefinitely replacing endpoints in the old ring. If all grants already returned
acknowledged receipts, use ordinary process wait rather than native unresolved
reaping. These distinctions are documented and tested.

A killed writer may have removed a token before recording attribution. The
current schema does not reconstruct every such transfer. Healthy ready messages
can progress, but a zero-leak same-generation guarantee is still false. A new
session can resume service under one complete authority ledger, while old bytes
remain quarantined or all possible holders are fenced. Four total objects and
two quarantines bound that cohort's storage. Authority failure stops managed
recovery until reconciliation; a new manager cannot restart its allowance over
unknown old holders.

Neither atexit nor garbage collection is a crash recovery protocol. Idle normal
Python shutdown gets best-effort cleanup; SIGKILL/os._exit cannot run it. Failed
cleanup retains an inspectable local obligation instead of forcing a token back
into circulation. Unregistered fork descendants are forbidden. Application
replay, deduplication and durable effects require their own protocol.

## 9. Production integration quickstart

From the extracted project root:

```sh
python3 tools/verify_release.py .
make CC=clang CXX=clang++ BUILD=build/release all benchmarks python
make CC=clang CXX=clang++ BUILD=build/release release-check
```

Select the actual native suffix:

```sh
export ELITE_LIBRARY="$PWD/build/release/libelite_ringbuffer.so"  # .dylib on Darwin
export PYTHONPATH="$PWD/build/release/python:$PWD/bindings/python"
python3 examples/python_roundtrip.py
```

A direct Python message needs no intermediate payload bytes:

```python
with producer.reserve() as write:
    with write.buffer as view:
        struct.pack_into("<QQ", view, 0, 42, 99)
    write.commit(16, message_type=1, message_id=42)
with consumer.borrow() as read:
    with read.buffer as view:
        values = struct.unpack_from("<QQ", view)
```

The complete runnable program includes construction, cleanup and imports.
`docs/INTEGRATION.md` includes C and C++ source patterns, launch/acknowledgement
order, errors/outcomes, deployment and rollback. Complete C/C++ examples are
compiled, not only illustrative prose. Stage/install into a versioned prefix
and use fresh generations for upgrades or rollback. Do not replace live mapped
state or redirect old pointers to successor memory.

## 10. Architecture post-mortem

The most useful early decision was rejecting an all-purpose claim-first ring.
SPSC can retain its low-contention, two-cursor mechanism while NCQ spends
additional RMW/coherence work for multi-owner publication order. Neither needs
a ticket lock, but they have different progress and cost contracts.

The second decisive decision was treating leases as part of correctness rather
than post-benchmark wrapper work. A fast native pointer handoff with premature
Python recycling would be a different, broken system. Actual exporter ownership
and fail-closed buffer closure preserve the original reverse happens-before
edge. The cost is visible: Python objects, local locks and multiple FFI calls
per message. Large-payload bandwidth and small-message call rates diverge sharply.

The third lesson is to keep observations separated. Cache padding changes byte
geometry, not scheduler guarantees. Apple native lab results differ strongly
from a Linux shared container, but one percentile ratio does not isolate a CPU
architecture's contribution. RTT and direct one-way intervals differ. A100M
finite-membership pass is strong evidence for that run, not a universal theorem.
The source package retains those distinctions instead of upgrading earlier
plans into unperformed certification.

Finally, managed recovery is explicit coordination. Publish-first removes an
unhelpable FIFO hole; it does not recover missing ownership information. The
same rule survives the Python boundary: retaining a view is retaining a real
native capability. Closing a name, changing an epoch, or executing a fence
cannot revoke a paused writer's future store.

## 11. Release scope and remaining admission gates

The eight-turn deliverable is a complete source distribution with working
native/opaque-language integration, tracked zero-copy Python views, executable
validation and benchmark drivers, evidence and integrity manifests. It is ready
to build and evaluate against a nominated deployment contract.

It does not claim the original direct one-way15ns median/50ns p99 goals have been
certified. The full preregistered5-repeat, uncertainty-qualified population-tail
campaign and all formal/chaos/model cases remain distinct from the completed
scoped runs. New Apple Python/exporter builds and execution are still required
before calling that target qualified. The full combined new sanitizer campaign
also remains unpassed, with successful narrower lanes documented.

No performance result authorizes weaker atomics, untracked buffer revocation,
in-place reset, silent copy substitution or retrospective sample trimming.
A future valid ownership witness still rejects that implementation. Source
release versioning does not nullify any frozen kill gate. Use the release
manifest and exact deployment binary identities as the handoff boundary.

## 12. Provenance and sources

[P4] Frozen Turn4 ABI,93,610 bytes, SHA256
`6aaa3d6706c29096cfab56ea1fe569afa561c4cba7caea26ebd3c09cc9812386`;
[P5] Turn5 verification plan,96,883 bytes, SHA256
`bc7ae2702cecb8d46820cf5365046fe40dda36c59dde9c80fda98ff7833ecdf8`.
Preserved under `docs/reference/`. They govern byte layouts, ownership, errors,
failure limits and qualification scopes; neither is reclassified as an executed
proof checker.

[P6] Turn6 implementation report and inherited evidence; historical scoped native
and cross-lowering results. [P7] Turn7 report,26,099 bytes, SHA256
`48515cca5e5a34b3ab8c2790267a1d634f664d581a57cd0828f49828fe8c4604`,
and verified parent archive. They supply the Linux100M comparison and original
artifact identities. Old statements about missing bindings describe their turns.

[A8] User/Gemini Turn8 commission,2026-09-23. Supplied Apple100M numerical
summary, replay assertion, QoS, feature-macro correction and cache control.
Transcribed into `evidence/turn08/APPLE_LAB_REPORTED.json`; not an independently
fetched raw Apple run. Missing rates/model/build/trace information is not filled.

[R1] CPython public buffer protocol documentation, consulted2026-09-23:
https://docs.python.org/3/c-api/buffer.html
Supports exporting-object references, writable-request rejection and paired buffer
release. The specific graph and tests above are this project's implementation.

[R2] Python ctypes documentation:
https://docs.python.org/3/library/ctypes.html
Documents the standard FFI calling convention and CDLL/GIL behavior. Exact local
struct compatibility is established by the new runtime checks, not assumed
from generic documentation.

The native queue's algorithmic attribution remains Ruslan Nikolaev, DISC2019,
Figure5 NCQ and free/ready indirection. No new public redistribution licence is
selected on the owner's behalf. NOTICE.md preserves private delivery and original
attribution. The final archive includes no third-party queue implementation,
compiler/runtime binary, SDK or unrelated asset.
