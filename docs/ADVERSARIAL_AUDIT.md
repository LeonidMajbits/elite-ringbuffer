# 09 — Adversarial Stress Test and Claims Audit
## ELITEIPC / elite-ringbuffer 1.0.0 / SPSC and NCQ-SC64

**Commission:** Gemini Operator Lab, paired with Leon — post-release falsification before publication.  
**Audit date:** 2026-09-23, America/Toronto; resumed verification on 2026-09-24 UTC.  
**Revision:** 2 — completed after the interrupted response; section 17 adds recovered and freshly reproduced findings. The audited production archive is unchanged.  
**Disposition:** **HOLD the current release's production-safe Python claim. A high-severity buffer-export defect and a separate, explicitly constrained finalizer-resurrection heap-use-after-free are reproduced, together with cyclic-GC retention, an asynchronous acquisition-ownership gap, a final-archive integrity failure, and a GCC sanitizer-build failure.** The native queue algorithms survived the finite tests run here; that is not an all-executions certificate.  
**Actual audited input:** `Standalone_Repository/elite-ringbuffer-1.0.0.zip`, 3,959,129 bytes, 486 files.  
**Input literal SHA-256:** `9d8a9843ecbde3cd5dcc780a744f398c2638a92a0a656b3aaa86ddc72cec2907`.  
**Input Drive ID:** `1l9KanXCTHFsMqK5SEeGBD4_UJ-dWp4QR`.  
**Output destination:** Deployment Research / Elite_Systems_LockFree_RingBuffer.  
**Integrity:** The companion `.md.sha256` authenticates the complete literal document. This document does not contain a self-referential literal-file digest.  
**Scope convention:** **REPRODUCED** = executed against the identified input; **SOURCE-DERIVED** = code inspection or reasoning; **LAB-REPORTED** = the commissioning statement; **HISTORICAL** = retained predecessor evidence; **NOT RUN** = no new execution. Source locations below are relative to the audited archive, not a similarly named earlier release. The companion evidence bundle contains exact numbered excerpts, commands, logs, input identities, reproducers, and an isolated candidate patch. Section 17 distinguishes the original audit evidence from the continuation replay; the candidate patch is not a production fix and does not resolve the added findings.

---

## 1. Executive verdict

The strongest part of this system is still its native ownership architecture: the SPSC forward and reverse release/acquire chains, and NCQ's publication-before-tail mechanism. The weakest part of the current release is the claim that the Python exporter makes lifetime safe for every supported view operation. It does not.

I reproduced an execution in which `memoryview(exporter)` begins while a lease is live, the lease is then legally transferred by another Python thread, and the exporter finishes by returning a nonempty buffer whose address has already been cleared to NULL. Ordinary `struct.pack_into` or `struct.unpack_from` on that view crashes the interpreter. No fabricated native pointer, private factory call, out-of-bounds user index, free-threaded interpreter, signal-handler reentry, or source modification was required. A trace hook only makes an existing two-thread interleaving deterministic.

The shipped 36-test Python suite passes on the vulnerable build. It also passes with Clang AddressSanitizer/UndefinedBehaviorSanitizer instrumentation. The new adversarial schedule, not another repetition of the existing tests, exposes the defect.

A second experiment creates an ordinary application reference cycle involving a cached memoryview. The exporter does not participate in cyclic GC. After the outer write lease becomes unreachable, the endpoint remains alive and busy with no reported cleanup failure. It can be cleaned after explicitly breaking the cycle, but normal cyclic collection does not do so.

There are also release-engineering failures. The downloaded ZIP agrees with its external SHA-256 receipt, but its own `MANIFEST.sha256` does not agree with its final `.gitignore`, and its new `LICENSE` is absent from the manifest. GCC's strict O1 sanitizer build fails on a sign-conversion warning in `src/elite_shm.c`. Neither defect is proof of a broken queue, but both contradict a blanket statement that this exact final archive is already fully validated.

**Publication recommendation:** do not present this exact 1.0.0 archive as a memory-safe, production-qualified Python IPC distribution. Preserve it as an auditable release candidate, fix the exporter and GC lifetime defects, regenerate the final artifacts, and issue a new identified candidate. A native-only research/showcase release can be scoped separately, but it must not inherit the Python safety or universal performance claims.

### Findings register

| ID | Severity / type | Finding | Evidence status |
|---|---|---|---|
| A09-01 | **High — release blocker** | Pending `bf_getbuffer` is not counted before a Python callback; concurrent transfer closes the exporter and a NULL-backed view is subsequently returned | Reproduced in SPSC/NCQ, read/write, GCC/Clang, including four sanitizer cases |
| A09-02 | **Medium — resource/lifetime defect** | Exporter owns Python containers but is invisible to cyclic GC; a normal application cycle strands the lease and endpoint | Reproduced; explicit cycle break restores cleanup |
| A09-03 | **Medium — release integrity** | Final ZIP fails its own manifest; `.gitignore` differs and `LICENSE` is unlisted | Reproduced directly from downloaded archive |
| A09-04 | **Medium for qualification; low runtime severity** | Required strict GCC O1 ASan/UBSan build fails on a sign-conversion warning | Reproduced; GCC O3 release and Clang sanitizer builds succeed |
| A09-05 | **Medium — benchmark provenance gap** | Cache-control JSON omits the start/end ticks and timebase needed to independently reconstruct its RMW rate | Source-derived; not evidence that the reported rate is false |
| A09-06 | **Claim boundary** | Apple headline results do not establish universal latency, Python-bytecode bandwidth, absence of all false sharing, or full preregistered release qualification | Source-derived and evidence-scoped |
| A09-07 | **High impact; constrained hostile/buggy-hook trigger** | Exporter destructor exposes its dying self to `sys.unraisablehook`, then frees it despite a retained reference; reading `.closed` is a heap use-after-free | Recovered sanitizer evidence and fresh reproduction; see §17.2 |
| A09-08 | **Medium — availability and exception safety** | An asynchronous exception after native acquisition but before Python pin adoption loses the public lease while native ownership remains BUSY | Recovered evidence and fresh deterministic reproduction; see §17.3 |

---

## 2. Input identity, execution scope, and what “exhaustive” does not mean

### 2.1 The standalone archive is the subject

I located and downloaded the standalone candidate from the specified Drive project. Its ZIP CRC check passes. Its SHA-256 matches the neighboring ZIP receipt. I did not substitute the earlier chat archive, which had a different hash, file population, Python implementation surface, and build history.

The current native SPSC, NCQ, common core, format, internal header and wait sources match the corresponding mounted earlier source files. Other files differ, including the Python wrapper/exporter surface, headers, build files and `elite_shm.c`. The audit follows the **current standalone bytes**, even when an earlier report describes a correction that is absent from them.

The archive has 484 declared complete-manifest entries and 46 source-manifest entries. There are 486 actual regular files, including the complete manifest itself and the unlisted license. All 46 source-manifest entries match their files. Of the 484 complete-manifest entries, 483 match and one does not.

All 49 original files in the source/build/API/test/tool/packaging inspection inventory still match their archive bytes after testing. Build outputs were created in separate build directories. The isolated exporter patch was compiled outside the source tree and was not applied to the release. See `evidence/ARCHIVE_SOURCE_IDENTITIES.json` and `evidence/SOURCE_EXCERPTS.md`.

### 2.2 Newly executed environment

The new audit ran on Linux/x86-64, kernel 6.18.44 with glibc 2.41, reported AMD EPYC 9V74, CPython 3.13.5, GCC 14.2.0 and Clang 17.0.0. Five logical CPU IDs were allowed; the CPU quota was four CPU-time equivalents. Exact observations and compiler versions are in `evidence/environment.json`.

This is **not** the earlier Xeon measurement host and **not** the user's Apple machine. Apple CPython 3.14 execution, P/E placement, cross-socket NUMA runs, PMC collection, power interruption and the full repeated 100M qualification were **not newly run** here.

### 2.3 What was actually inspected and exercised

Review covered all native implementation translation units, internal ownership structures, the Python exporter, ctypes declarations, Python lease/manager/endpoint code, native latency/throughput/cache benchmark logic, Python benchmark logic, build files, release verification and the relevant tests. Historical planning papers were used as contracts, not treated as evidence that an executable model had run.

New execution included strict GCC and Clang builds; the existing core, IPC, adversarial, limits, chaos, prefix-fuzz and benchmark-tool suites; the 36-test Python suite; a Clang ASan/UBSan version of that Python suite; three additional 1M-message NCQ runs with 16P/16C, 16P/1C and 1P/16C; a GC-cycle witness; and 20 controlled exporter-race child executions. An isolated patch was then tested against the four mode/role race combinations and the existing Python suite.

No finite test campaign proves absence of every race, memory fault or infinite-execution liveness defect. This audit is comprehensive across the requested interfaces and concrete enough to reject the release, but it is not an exhaustive state-space proof. No formal-checker coverage or fresh TSan result is invented.

---

## 3. A09-01 — A pending buffer export can outlive its lease validation

### 3.1 Exact defect location

`bindings/python/elite_buffer.c:34–51` validates the exporter and calls its Python owner before incrementing `self->exports`:

- Lines 40–42 reject an already-closed exporter.
- Lines 43–45 check the export-count ceiling.
- Line 46 invokes `_assert_live` through `PyObject_CallMethod`.
- Line 49 publishes `self->address` and `self->length` into `Py_buffer`.
- Line 50 increments the export count, **after** the callback.

`close_buffer`, lines 59–73, only rejects an export count that is already nonzero. It marks the exporter closed, drops the native pin, clears the Python owner, and sets `self->address=NULL`. It does not zero the length.

The Python `_assert_live` method, `bindings/python/elite_ringbuffer/__init__.py:380–383`, is a liveness check, not one indivisible transaction with `PyBuffer_FillInfo`. A Python callback is an interleaving/reentrancy boundary. A GIL-enabled build does not make multiple bytecodes and subsequent C work indivisible.

### 3.2 Minimal causal history

Use either a write lease or a read lease. Obtain an initial view, retain its exporter through `view.obj`, and release the initial view. The exporter remains a valid object and still holds the native pin; its export count is zero. The package already tests this retained-exporter behavior.

| Step | Thread A — acquiring a new buffer | Thread B — transferring the lease |
|---|---|---|
| 1 | Calls `memoryview(exporter)` | — |
| 2 | Sees open exporter and calls `_assert_live`; check succeeds | — |
| 3 | Pauses at the return from that Python check | — |
| 4 | — | Calls ordinary `WriteLease.commit` or `ReadLease.release` |
| 5 | — | Export count is still zero, so close succeeds; native view pin ends; address becomes NULL; ownership transfers |
| 6 | Resumes and calls `PyBuffer_FillInfo` with the now-NULL address and old positive length | — |
| 7 | Returns a new view even though exporter is closed and lease inactive | — |
| 8 | Ordinary `struct.pack_into` / `struct.unpack_from` dereferences the advertised buffer | Process receives SIGSEGV |

This is not a test that writes through a knowingly retained old raw pointer after commit. It is a test of the exporter's own promise: **a new buffer request racing closure must either acquire a protected live span or fail, never return invalid storage**. The source fails that promise.

The deterministic reproducer uses `sys.settrace` only to pause at an existing `_assert_live` return, plus two events to order the competing operations. It does not rewrite `_assert_live`, change private state, construct an address with ctypes, or invoke the exporter's private factory. Thus it is a controlled schedule of the unmodified implementation, not an injected implementation bug. The natural occurrence rate is not measured.

### 3.3 Observed evidence

`repros/export_race.py` covers both SPSC and NCQ, and both writable/read-only leases. All 16 uninstrumented executions — two compilers, two modes, two roles, two repetitions — terminated with SIGSEGV. Four corresponding Clang ASan/UBSan executions terminated with sanitizer-reported NULL-page reads or writes. Every child was reaped; only that child's explicitly logged test backing name was unlinked afterward.

A representative pre-crash observation was:

`transfer_blocked=false, closed=true, active=false, exports=1`.

The writable returned view had length 64; the read-only returned view had length 8. These were nonempty views returned after their lease had ended. The sanitizer logs identify address zero.

**Proved impact:** an invalid use-after-lease export and process crash through ordinary Python buffer consumers. **Not proved:** a heap use-after-free, a double-free, arbitrary code execution, leakage of arbitrary memory, or corruption of a successor ring. Those stronger labels are not attached to this witness.

The ordinary 36-test suite passed on this same vulnerable source under the release build and again under Clang ASan/UBSan. That does not contradict the defect: those tests do not force the pending-export/close interval. ASan reports a problem when the bad path executes; it is not an exhaustive scheduler. Python's buffer contract requires correctly populated buffer fields and paired lifetime management; `PyBuffer_FillInfo` is not an ownership validator. [W01]

### 3.4 Candidate mitigation and its actual status

Reserve an export-in-progress count **before** any callback into Python. Closure must treat pending and established exports as busy. On failed validation or failed buffer creation, roll that count back. Keep the native lease pin alive through the complete acquisition transaction.

The accompanying `repros/pending_export_candidate.patch` makes that limited change. Compiled separately against the unchanged native library, it blocked all four mode/role race schedules with `BufferError`, preserved a live lease and valid view, and still passed the 36 existing Python tests. Logs are `candidate_*` and `candidate_python_suite.log`.

This is a **validated mitigation candidate**, not a released fix or a full safety proof. It has not been executed on Apple/CPython 3.14, does not repair cyclic GC, and needs error-path, reentrant-callback and repeated-export stress. Moving the count before the callback is more robust than a lone late closed-state check because the pending request itself now prevents transfer.

---

## 4. A09-02 — The exporter is invisible to cyclic garbage collection

### 4.1 Source and reproduced cycle

`EliteBuffer` holds a strong `PyObject *owner` reference. The owner is a Python `_Pin`, which references the endpoint. However, `BufferType` at `elite_buffer.c:108–118` has only `Py_TPFLAGS_DEFAULT`; it has no `tp_traverse` or `tp_clear`. Allocation at line 128 uses `PyObject_New`. The implementation's internal weak endpoint-to-pin link avoids its initially intended cycle, but it cannot prevent ordinary application objects from forming another cycle.

The reproducer stores a view on its endpoint as an application attribute:

`endpoint → cached_view → managed buffer/exporter → _Pin → endpoint`.

Then it deletes all normal external strong references to the endpoint, view and outer write lease, and performs three cyclic collections. A weakref is used only to observe whether the endpoint survived; it is not a strong root.

Observed result:

- `gc.is_tracked(exporter)` is false.
- The outer lease is collected.
- The endpoint is **not** collected and remains busy.
- `cleanup_failures()` remains empty.
- Explicitly releasing the cached view and breaking the cycle allows ordinary close and object destruction.

The evidence is in `repros/gc_cycle.py` and `evidence/gc_cycle.log`.

### 4.2 Severity and remedy

This is a resource-retention and availability failure, not evidence of a dangling pointer. One unreachable cycle can retain a native lease, endpoint and mapping for the rest of a process lifetime. Repetition can exhaust service capacity or prevent managed shutdown. A “zero leaks” claim derived from the existing tests does not cover it.

CPython requires container extension types to participate in traversal, and mutable container types to support clearing, when their references can form cycles. [W02] A complete fix requires GC-aware allocation/tracking/traversal/clearing and a proof that clearing cannot release a lease while a usable exported alias still exists. Merely adding the GC flag, or blindly clearing the owner, is not enough.

Add tests for endpoint-cached views, user objects that retain both endpoint and exporter, nested memoryviews, cyclic exceptions/tracebacks, cleanup failure, finalizer reentry and potential resurrection. The pending-export patch is deliberately **not** presented as solving any of these.

### 4.3 What does work in ordinary use

Acyclic abandonment of an uncommitted write aborts it; normal read cleanup returns its token. Slices, casts, independent roots and downstream ctypes buffers keep the exporter alive, and the existing targeted tests passed. Context-manager cleanup correctly refuses transfer with outstanding exports. The defects are not a reason to say the entire lifetime mechanism never works; they identify specific missing interleaving and reachability cases that the current contract claims to cover.

---

## 5. Release artifact and toolchain failures

### 5.1 A09-03 — Hashing the ZIP did not validate its final contents

The outer ZIP hash is correct. The internal complete manifest is not.

| `.gitignore` identity | SHA-256 |
|---|---|
| Expected by `MANIFEST.sha256` | `1fc70a7bb05528371b3b0ff8530406d37384ac0c932c51bce2c2673321605195` |
| Actual file in downloaded ZIP | `2e330e18c0d9f40c4e5de2fa11d3f82bc9bf0e833e0c6f0f338815ec8d41e536` |

Executing the archive's own `python tools/verify_release.py` returns exit 1 with `FAIL: hash mismatch: .gitignore`. `LICENSE` is also present but absent from the purported complete manifest. The verifier checks declared entries but does not establish set equality with all packaged files, so an unlisted addition is not detected by that script.

This does **not** show malicious tampering or changed queue source. All 46 source-manifest records match. It does mean the statement that the final standalone artifact matched 484 files bit-for-bit is not true of the retrieved ZIP. A check made before final packaging edits is not a check of the final candidate.

Required correction: finalize license, README, ignore rules and all source files; generate a manifest from an explicit final package file set; package; extract into a new directory; verify both set membership and every digest; build/test that extraction; then publish the new outer digest and exact commit. Build outputs should be outside the package set, not silently allowed as arbitrary extra release content.

The first audit invocation mistakenly passed `--strict`, an option from a different earlier verifier; it failed argument parsing and is preserved as an auditor command mistake. The corrected current-verifier invocation produced the product manifest failure above. These are not conflated.

### 5.2 A09-04 — GCC sanitizer warning is a real reproducibility failure

The release builds cleanly at O3 under GCC 14.2.0 and Clang 17.0.0. The unmodified strict GCC O1 ASan/UBSan configuration fails in `src/elite_shm.c:66`:

`conversion to 'unsigned int' from 'int' may change the sign of the result [-Werror=sign-conversion]`.

The expression shifts an integer-promoted byte and applies `& 1u` before the outer cast. Moving the unsigned conversion to the byte operand before the shift is the appropriate direction; suppressing `-Wsign-conversion` globally is not.

No runtime memory fault is inferred from this warning. Its importance is that a declared qualification lane cannot even build. Clang's sanitizer build succeeds and was used for the reproduced memory fault and clean baseline suite. Compiler and optimization configurations are different evidence cells, not interchangeable passes.

---

## 6. Hardware and throughput claim falsification

### 6.1 Evidence provenance before interpretation

The standalone archive contains `evidence/turn08/APPLE_LAB_REPORTED.json`. It explicitly labels the Apple results LAB_REPORTED and lacks a supplied SoC identity, CPU count, SDK/compiler/binary hashes and raw Apple latency traces. The archived Python benchmark JSON examined here is Linux evidence, not the newly reported 22.8 GB/s Apple run. The current commissioning message supplies the new Python/CPython 3.14 outcomes, but not the corresponding raw run package.

Therefore this audit can inspect the harness, check arithmetic and boundaries, and run new Linux falsification tests. It cannot independently replay the new Apple observations or assign exact causes to their tails. The Apple figures may be correct within their experiment; the available evidence does not justify broader conclusions. This is a provenance boundary, not an allegation that the lab fabricated results.

### 6.2 250 ns median RTT: what the number actually buys

The current latency driver is a two-process, two-ring, one-outstanding-request closed loop. The origin timestamps before reserving a request and after validating and releasing its reply. It includes both directions and responder work. NCQ is 1P/1C per direction, not a 16-contender RPC service. The clock is Mach absolute time on Darwin or the raw monotonic OS clock on Linux; ordering and instrumentation costs remain in the result. [S07; H07 §§2–3]

The defensible claim is: **a reported median of 250 ns for that specified instrumented SPSC round-trip experiment**. It is not any of the following:

- a 250 ns upper bound;
- a certified 125 ns one-way median;
- a sub-50 ns or sub-15 ns one-way result;
- an open-loop service tail under burst arrivals;
- guaranteed performance on E-cores, another cluster, another socket or under preemption.

For an individual exchange, an abstract decomposition is

`RTT = outbound work + responder processing/queue work + inbound work + relevant instrumentation/scheduling effects`.

The components need not be symmetric. Quantiles of sums do not generally equal sums of quantiles. Dividing p99 RTT by two does not recover p99 of either direction. Even the mean would require assumptions about response work and symmetry before interpreting a halved value.

A zero-copy protocol still performs stores, ownership communication, loads, checks and scheduling. “Zero latency” is false. A zero timer delta would establish inadequate observation resolution for that sample, not instantaneous communication. The equal 250 ns p50/p90 values could reflect a narrow or quantized distribution; actual raw ticks and timebase must decide it. No unprovided Apple counter frequency is assumed.

The original one-way goals were p50 <15 ns and p99 <50 ns, with separate metrology and five-run rules. Neither a closed-loop RTT result nor inverse throughput fulfills them. The prior report explicitly leaves absolute timing uncertainty unqualified. [P05 §§4–6; H07 §3.2]

### 6.3 Preemption and open-loop bursts are the immediate latency cliffs

A producer paused while holding a private lease retains capacity; it does not publish its partial payload. A responder paused after reading a request prevents that particular reply. A process descheduled for D contributes D to its pending exchange regardless of the queue's instruction count. In the unrestricted scheduling model there is no finite worst-case wall-time bound.

The one-outstanding-request experiment intentionally suppresses backlog. A bursty service must retain original intended offer times, pending requests and missed deadlines. Otherwise a generator that itself stalls can report only the fast successful calls afterward. Do not use the closed-loop median to choose a service-level objective for a 32-worker, quota-throttled deployment.

NUMA and cluster claims require placement evidence. On Linux, collect CPU masks, memory-placement evidence and first-touch policy; distinguish same-node from cross-node communication. On Darwin, `thread_policy_set` affinity tags are cache-placement hints, not an API for binding to a numbered P-core. A successful tag readback is not proof of P/P execution. [W05]

### 6.4 22.8 GB/s Python goodput: credible category, overstated universality

The current Python benchmark invokes Python and ctypes for each lease operation, but constructs and validates payload contents using `_elite_buffer.fill_pattern` and `verify_pattern`, native C loops at `elite_buffer.c:134–173`. Every payload byte is actually written or compared; I found no counts-only substitution in this path. The pattern is a repeated 64-bit scalar derived from the message ID, not a general Python serialization workload.

For payloads over 4096 bytes, `bench_python.py:69–70` chooses **capacity 8**. Thus the 1 MiB experiment repeatedly cycles an approximately 8 MiB payload pool, not a continuously fresh multi-gigabyte working set. Whether it fits a particular cache hierarchy depends on the actual machine; the code alone does not establish that it is all-cache or all-DRAM. It is nonetheless a deliberately reusable, potentially cache-friendly workload.

The numerator counts application bytes delivered once. At 21,734 messages/s and 1,048,576 bytes/message, the reported decimal rate is about 22.79 GB/s. Those lifecycles include at least one logical payload construction and one full logical verification, but that does not identify physical DRAM or fabric traffic. Cache hits, write allocation, eviction and other traffic are not supplied by the application rate.

The reciprocal of 21,734 messages/s is roughly 46 microseconds per average completion interval. It is not a one-way latency measurement or a latency percentile. Conversely, a large-message bandwidth result says little about 64-byte Python event throughput, where per-call object creation, locking, ctypes and GC costs dominate.

Falsification conditions to run include independently varying payload size and capacity, pools larger than observed last-level cache capacity, non-repeating data generated before the timed span under a separately labeled copy workload, ordinary application parsing, CRC64, multiple endpoint pairs, reader-held views, concurrent allocation/GC pressure and mixed QoS. Exact source/binary hashes, count, warmup, capacity, start/end records and returned-token evidence belong with each number. The current benchmark fixes large-payload capacity internally; extending that dimension is a harness change that must receive its own identity.

**Accepted wording:** “Lab-reported 22.79 GB/s delivered 1 MiB payload goodput through Python/ctypes with native in-place pattern construction and full-byte checking, under the recorded 1P/1C configuration.”  
**Rejected wording:** “Python universally transmits at 22.8 GB/s,” “22.8 GB/s pure-Python serialization,” or “measured 22.8 GB/s DRAM/network bandwidth.”

### 6.5 4.04 billion RMW/s is an aggregate independent-counter control

`benchmarks/bench_cache.c` increments one independent relaxed atomic counter per worker. It intentionally compares eight-byte packing with 128-byte separation. It verifies final counts, and computes the rate as total increments divided by the interval from the common future start to the latest worker end.

The reported 4,039,638,957 operations/s is aggregate worker throughput. Its reciprocal, approximately 0.2475 ns, is **not** the latency of a contended atomic operation on one core. Independent lines permit parallel progress; the actual NCQ queue has shared arbitration points.

The ratio against 35.5M operations/s is about 113.8. That supports a large sensitivity to the selected packed/isolated arrangement in the reported trial. It does not prove that every field, Python object, allocator structure, control-plane cache line or workload is free of false sharing. It also does not eliminate true sharing of QF/QR heads, tails, entries or payload handoffs.

The conditional byte-layout argument remains valid: when admitted coherence granularities divide 128 and designated objects occupy disjoint complete 128-byte cells, those designated objects do not overlap a coherence line. Other cache effects — set conflicts, capacity pressure, adjacent-line prefetch effects, migration and scheduling — are not removed by that theorem. [P04 §6.5]

**A09-05:** the cache JSON at `bench_cache.c:42–45` saves the computed rate, iterations, stride and placement but omits `t0`, final `end`, per-worker start/end ticks and the timebase used in its denominator. A verifier cannot reconstruct the rate solely from that JSON. Export these measurements, exact counter values and binary identities before turning the ratio into a publishable reproducibility claim.

### 6.6 Cross-platform ratios are not controlled hardware comparisons

The retained Linux 100M data came from a Xeon container with four CPU-time equivalents and explicit oversubscription. The Apple report describes bare metal. The 38.5× and 34.6× comparisons use approximately **p99.99**, not maximum latency. The retained Linux maximum values are approximately 213.5 ms and 145.0 ms, not 277 or 410 microseconds. [H07 §2]

Matched-quantile arithmetic is legitimate descriptive reporting, but “Apple hardware is 38.5× faster” is unsupported without comparable CPUs, process populations, placement, background load, power mode, timer uncertainty, repetitions and workload. The Linux tail cause was not established, so it cannot be relabeled as a measured interconnect penalty.

### 6.7 Concrete workload matrix still needed

| Dimension | Required contrasting conditions | Distinguishing failure or cost |
|---|---|---|
| Locality | Recorded same-domain, cross-domain, mixed/unknown placements | Coherence/locality cost versus invented P-core pinning |
| Population | 1/1, 2/2, 4/4, 8/8, 16/1, 1/16, 16/16 | Resource contention and per-caller unfairness |
| Pool size | Capacity × payload below/above observed cache working sets | Reuse-friendly bandwidth versus larger memory pressure |
| Arrival process | Closed loop, fixed-rate open loop, preregistered bursts | Queueing delay and coordinated omission |
| Retention | All tokens free, some long-lived views, all tokens retained | Capacity backpressure, not a hidden FIFO lock |
| Runtime | Native, Python small objects, native bulk helpers, GC pressure | FFI/workload costs rather than borrowed C rates |
| OS faults | Descheduling, quota pressure, memory pressure in isolated test hosts | Tail failures and allocation faults |
| Integrity | NONE and CRC64, deliberate owned-byte corruption | Cost of integrity checking and fail-closed behavior |

No additional 100M campaign or hardware-counter result is claimed in this audit. The new 1M asymmetric runs are correctness stress, not substitutes for this matrix.

---

## 7. Native concurrency and memory-model audit

### 7.1 SPSC publication and reuse

The implementation uses private reservation and a cached covering observation of the peer cursor. `elite_spsc.c:25–39` writes metadata and checksum before release-publishing P. The consumer acquires a covering P before `el_read_metadata`; `elite_spsc.c:57–65` release-publishes C only after the borrowed reads end. Reuse is admitted through an acquired reclamation observation.

The two essential relationships remain:

`W(payload_k) →sb release(P covering k) →sw acquire(P covering k) →sb R(payload_k)`

and

`R(payload_k,last) →sb release(C covering k) →sw acquire(C covering k) →sb W(payload_k+N)`.

Both are needed. A publication-only proof would permit overwriting a slow consumer. In the inspected implementation, no payload or mutable descriptor cleanup follows publication/reclamation. The post-LP operations affect local state and the separate optional wait notification.

The ordinary payload fields need not be atomic when exclusive ownership and both relationships hold. Adding full fences around each payload field would not fix a lease-lifetime violation and would distort the cost model. A checksum detects some content errors; it does not legalize an unauthorized concurrent read.

The minimal polling SPSC algorithm has no reservation CAS loop. Nevertheless, the full call has handle/lifecycle validation, and any wall-clock claim also includes payload work, OS scheduling and runtime overhead. The Python layer is neither wait-free nor lock-free merely because the underlying cursor path is simple.

### 7.2 NCQ-SC64 retains its exact stronger reference orders

`elite_mpmc_ncq.c:8–67` uses sequentially consistent head, tail and entry loads, with strong CAS and SC success/failure orders. Entry installation is the enqueue LP; the subsequent tail increment is helpable. Head advancement is the successful dequeue LP; only its winner returns the captured block index.

I specifically checked the dangerous refinements:

- failed head CAS restarts with a fresh head, entry and desired value;
- failed publication CAS restarts classification rather than using the updated expected word to replace a winner;
- tail does not reserve a future unfinished payload;
- the legal state H=T+1 after consuming a just-installed entry is not rejected merely because the tail helper has not run;
- publication callers and returners do not touch a transferred descriptor after the LP;
- descriptor phase is validation metadata, not an independent queue-membership oracle;
- unique-token admission is required before entering an internal enqueue, so internal queues are not silently used as arbitrary enqueue-when-full containers.

No counterexample to those native operations was found in this review or the new finite tests. It would be incorrect to weaken them to acquire/release simply because some instructions already look like `LDAR`/`STLR`. Source order and a compatible complete compiler mapping matter. A mnemonic grep cannot prove linearizability. GCC's atomic documentation supports the expected-value and strong/weak distinctions; it does not certify this IPC composition. [W06]

The supporting formal documents contain explicit assumptions, not an executed whole-system model-checking certificate. In particular, the planned finite model, weak-memory mutants and full fault-cut campaign are not established by the green integration suites alone. [P05 §§8–10]

### 7.3 ABA is excluded by no live wrap, not solved after wrap

The implementation does **not** support arbitrary 64-bit sequence wrap. It rejects operations at conservative ceilings:

`J = 2^64 − N − 1` for queue tickets, and `G = 2^62 − 2` for packed descriptor epochs.

Enqueue checks the eventual ticket increment before the irreversible entry CAS. Dequeue checks before head advancement. SPSC checks before authorizing eventual publication. Aborted reservations still consume a descriptor epoch. The local lease serial also has an exhaustion guard. The near-limit fixtures passed in the executed suite.

A stale expected value cannot reappear through ordinary allowed counter progress inside one admitted generation. Physical ring reuse is expected; represented ticket identity repetition is not. If someone removes the ceiling, resets a live cursor or reuses a session/grant while stale references exist, the argument is gone. A reduced-width history can then repeat the expected head and let a stale CAS validate a different logical operation.

Consequently **“ABA prevented under 64-bit wrapping” is the wrong product claim**. The valid claim is “ABA from ticket identity repetition is excluded during the enforced nonwrapping session lifetime, subject to the ownership and atomic-ABI assumptions.” Counter size makes exhaustion remote in practice; it is not itself the proof.

### 7.4 Individual starvation remains possible

The NCQ retry loops have no per-caller bound. Schedule a victim to read a current head, then let a rival advance that head before each victim CAS. Rival operations continue while the victim repeatedly loses. A finite participant count does not bound failures to K−1, and strong CAS does not provide fairness.

Call this **individual starvation**, not a proof of global livelock. Global lock-freedom concerns continued system-wide completion under the declared primitive progress model. OS preemption is an additional source of unbounded elapsed time. LL/SC implementations also require care about primitive forward-progress assumptions; the kernel's atomic discussion is a useful implementation warning, not a replacement userspace API. [W07]

The Python timeout loop checks its deadline between native calls. It cannot preempt an NCQ call that is still retrying internally. A service requiring a hard per-call timeout needs a different cancellable/bounded-work interface and a protocol for any already-owned token; killing a thread and stealing its lease is not that interface.

Padding does not remove the four shared queue cursor arbitration points. Under saturation, failed CAS attempts and helpers may consume increasing coherence traffic. The new 16P/16C, 16P/1C and 1P/16C 1M tests completed with exact membership and token return, but do not establish a percentile bound for each worker or a throughput scaling law.

### 7.5 C endpoint thread-safety is a precondition, not a hidden lock

`el_enter` uses a plain `in_call` field, and native handle bookkeeping is ordinary process-local memory. It detects some reentrancy but is **not** a synchronization mechanism for simultaneous native calls from different threads. C clients must serialize each handle or use separate endpoints. Violating that condition can race before a friendly BUSY return.

The Python RLock intentionally supplies local method serialization while ctypes releases the GIL around C calls. The exported-view transaction in A09-01 is the missing boundary; blanket “the GIL protects everything” reasoning is wrong. [W03]

### 7.6 Parkable SPSC is a different cost/progress profile

The wait adapter performs the admitted acquire-release exchange chain, including same-value notification updates. It arms, checks the actual predicate, compare-waits, disarms, and rechecks. POLL_ONLY returns before that work. MPMC waiting is rejected rather than improvised from inactive cells.

A producer can die after publication but before wait-state notification or the kernel wake. Finite requested waits permit later rechecking when scheduled; they do not produce a hard wake deadline. A future optimization that skips a same-value RMW needs a replacement proof, not only a faster benchmark.

---

## 8. Process death, allocation and shared-header trust

### 8.1 What crash resistance actually means here

| Event | Correct current interpretation | Unsupported stronger statement |
|---|---|---|
| SIGKILL before QR publication | Partial block is not readable; other published work can proceed; owned token may be orphaned | Every slot automatically recovered |
| Death after QR LP before caller return | Message may already be consumed; tail can be helped | Missing receipt means safe resend |
| Consumer dies after an external effect | Token/effect outcome may be uncertain | Queue guarantees exactly-once effects |
| SIGSEGV after a prior wild write | Shared control/payload corruption is possible | Signal name implies a clean stop-only failure |
| SIGSTOP and later resume | Old pointer remains a possible access capability | Timeout or epoch update revokes it |
| Authority process dies | No automatic ledger takeover or liveness channel supplied | Shared READY means manager alive |
| Host power loss | Volatile transport state is lost/unqualified | MAP_SHARED supplies a durable journal |

The system avoids an abandoned **ticket lock** because it contains no such lock. It does not thereby avoid an abandoned payload token or an unresolved management obligation. A claimant can die between QF removal and recording a descriptor phase; scanning EMPTY/COMMITTED cannot reconstruct ownership without ambiguity. The existing chaos tests correctly exercise selected stop-failure cases, not every cut point or prior corruption.

### 8.2 Safe reclamation depends on the full holder ledger

The code creates fresh names exclusively, constructs atomics before exposing grants, combines admission state/count in one atomic, uses one-shot participant records, and requires cleanup receipts or terminal owned-child evidence before final sealing. `elite_detach` refuses live calls/leases/views and performs no shared access after its count decrement. Those are meaningful protections.

But closing a descriptor, unlinking a name and ending another process's mappings are different actions. Unlinking does not revoke live mappings; a recreated name refers to another object. [W04] Similarly, an epoch or memory fence does not revoke a raw pointer. Safe generation replacement requires different backing and no redirection of still-live old virtual addresses into the new allocation.

The four-object/two-quarantine policy bounds one authority's managed cohort. Repeated creation of independent managers or an authority restart that forgets the old ledger is not a globally enforced system memory quota. The application must not reset its bookkeeping to claim recovery while old holders remain unresolved. There is no automatic authority-handoff protocol in this release.

Normal queue entry reads only local state and shared lifecycle; it does not continuously verify that the managing process is alive. A manager-death response therefore needs an external application lifecycle channel. Surviving connections can still see READY until somebody with authority requests retirement. Do not claim an automatic manager-death fence based on the presence of an authority ID.

### 8.3 Shared memory is not an adversarial security boundary

All trusted participants receive writable mappings of the control region. A faulty or hostile same-user participant can corrupt a cursor, alter a descriptor, or truncate a backing it can open. The CRC checks the immutable 512-byte prefix at controlled validation points; it does not authenticate a writer or continuously protect mutable head/tail words.

A corrupted queue entry cycle can leave a retry loop unable to make progress. Outside the conforming-participant fault model, the implementation does not guarantee it will turn every corrupted state into INTEGRITY rather than spin. Do not call every such experiment a counterexample to the admitted lock-free theorem; do treat it as an operational reason for process isolation, watchdogs and conservative failover.

A hostile Python program with unrestricted ctypes can also pass arbitrary native pointers. That is an existing process-memory privilege, not a useful new library finding. A09-01 is materially different because it reaches invalid memory through the intended exporter and standard buffer consumers without manufacturing a pointer.

### 8.4 Allocation exhaustion and power loss

The creator sizes/maps the object and then touches all its bytes during construction. Neither a successful virtual mapping nor the four-object count means the host can satisfy every subsequent physical backing fault. On tmpfs-backed systems, backing pressure and externally shortened files must be treated as possible process-failure sources. The mmap interface documents SIGBUS for access beyond backing extent. [W08]

This audit did not deliberately fill the shared host's tmpfs or interrupt power. Those are **unrun isolated-host tests**, not new reproduced defects. Production setup needs byte budgets, available backing admission, page-touch completion before grant exposure, memory-pressure observability and restart handling. Prefaulting narrows avoidable first-use faults; it is not a hard lifetime residency guarantee.

Power-loss persistence needs a separate durable log, replay/deduplication policy and application transaction semantics. A CPU release store, CRC, or shared mapping does not supply them.

---

## 9. Python behavior under abandonment, GC, threads and abnormal exit

### 9.1 Abandoned uncommitted WriteLease

For the tested ordinary acyclic path, the pin finalizer aborts rather than commits an unpublished write. A surviving exported alias retains the pin and delays that cleanup. A duplicate transfer is rejected against local ownership. These are sound intended semantics and are exercised by existing tests.

Three limits matter. First, A09-02 creates an unreachable cycle that never reaches the finalizer. Second, SIGKILL and abrupt process termination do not execute reliable Python cleanup; the manager must deal with an orphaned token or whole generation. Third, asynchronous exceptions around a native ownership transfer can make the return outcome uncertain. The transfer wrapper attempts to retain an uncertain resource and prevent blind retry. This statement does not cover every exception window: the newly reproduced acquisition-to-adoption gap in §17.3 lacks a corresponding public pin. Both cases can require process fencing, and neither supports transparent recovery.

The failure-retention list is not a substitute for an application recovery policy or a bounded lifetime for every Python reference graph. `cleanup_failures()==[]` means no failure was recorded by that mechanism; it does not prove no unreachable native-owning object remains.

### 9.2 Slicing and casting versus creating a new export

Existing slices and casts normally share a managed buffer and retain the original export. The tests confirm that releasing the root does not permit premature transfer while a slice survives. A newly requested `memoryview(exporter)` is a **separate acquisition path**, however; the pending count defect occurs before that path is recorded.

This distinction explains why many convincing lifetime tests passed while A09-01 still exists. Testing only already-created aliases misses creation-versus-close. A correct fix must cover both.

### 9.3 Multiple threads and the GIL

Ordinary producer/consumer methods acquire an endpoint RLock. They do not grant more than one outstanding native token. A second producer call on the same endpoint should encounter BUSY, not share the first writer's payload. Different handles may operate concurrently; sharing one writable view among Python/C consumers still requires application synchronization.

`ctypes.CDLL` releases the GIL around foreign calls. [W03] The wrapper's locks therefore do useful work; a GIL-enabled interpreter alone is not sufficient. Conversely the native pattern helpers retain the GIL while constructing/checking bytes, so a same-process multi-thread Python benchmark is not equivalent to the two-process goodput experiment.

The supported runtime explicitly excludes free-threaded CPython and subinterpreters. That is a limitation, not a bug in an admitted runtime. The confirmed defect occurs in ordinary GIL-enabled CPython 3.13.5. The same source paths warrant immediate Apple/3.14 testing, but no Apple crash is claimed from this Linux execution.

### 9.4 Additional review boundaries

The ctypes layer uses process-local structs, explicit signatures and runtime size/offset checks rather than foreign atomic overlays. That reduces ABI-mismatch risk. Loading an incompatible or attacker-selected library via environment/path remains a deployment-trust problem; use a pinned resolved library and exporter identity.

Finalizer error paths and asynchronous exceptions remain especially sensitive. They should be exercised with forced failures around native view retain/end, lease acquisition, transfer return, close and authority acknowledgement. The inspected fallback finalizer retirement path is another place to review local locking consistently. It is recorded as a follow-up review obligation, **not** an additional demonstrated corruption defect.

---

## 10. Executed evidence ledger

| Experiment | Fresh result | Limits |
|---|---|---|
| Downloaded standalone outer SHA/ZIP CRC | Match / no CRC error | Does not repair internal manifest |
| Current release verifier | **FAIL**, `.gitignore` hash mismatch | Correct current CLI; earlier `--strict` mistake kept separately |
| Source-manifest audit | 46/46 named hashes match | LICENSE not in complete manifest |
| GCC strict native build and baseline suite | PASS | Linux/GCC14.2, not Apple |
| Clang strict build and core/IPC check | PASS | Linux/Clang17 |
| Existing Python suite, GCC release | 36/36 PASS | Does not include new race or cyclic-GC witness |
| Existing Python suite, Clang ASan/UBSan | 36/36 PASS | Leak detector disabled for this CPython lane |
| New exporter schedules, GCC release | 8/8 child executions SIGSEGV | Forced schedule, not a natural failure-rate estimate |
| New exporter schedules, Clang release | 8/8 child executions SIGSEGV | Same four mode/role histories, twice |
| New exporter schedules, Clang ASan/UBSan | 4/4 NULL-page errors and abort | One each SPSC/NCQ × read/write |
| Application GC-cycle witness | Reproduced retained busy endpoint | Explicit cycle break then cleaned resources |
| Isolated pending-export patch | Four schedules safely blocked; 36/36 existing tests PASS | Not applied/released; GC not fixed; Apple not run |
| NCQ 16P/16C | 1,000,000 messages PASS, exact membership/token return | Oversubscribed Linux; no per-caller latency evidence |
| NCQ 16P/1C | 1,000,000 messages PASS | Same scope |
| NCQ 1P/16C | 1,000,000 messages PASS | Same scope |
| Native inherited regression | Core68, prefix512, IPC100k SPSC/NCQ4/4, adversarial8, limits4, chaos3, fuzz1M, benchmark-tools8 PASS | Finite selected cases, not full planned chaos/model campaign |
| GCC O1 ASan/UBSan compilation | **FAIL**, sign conversion | No GCC sanitizer runtime pass claimed |
| New TSan, Apple execution, NUMA, PMC, power loss, 100M qualification | **NOT RUN** | No surrogate pass assigned |

A separate check of the packaged portable runner repeated four original-build crashes and four safe rejections with the candidate exporter. Those additional checks are recorded under `portable-runner-original/` and `portable-runner-candidate/`; they do not add new natural scheduling coverage.

All new crashing children were harness-owned, resource-limited and reaped. Cleanup used only the exact name printed by that child. No unrelated process, production ring or user source was altered. Logs and result records are in the evidence bundle.

A fresh sanitizer clean run and a fresh sanitizer-detected failure can both be true: they executed different histories. The meaningful statement is the scope and observed outcome, not “all sanitizers green.” [W09]

---

## 11. Production-readiness scorecard

These are engineering judgments for the **exact combined native/Python distribution audited**, not compensation estimates, a security certification, or a score for Leon's ability. A high architecture score does not average away a memory-safety blocker.

| Dimension | Score / 10 | Reason |
|---|---:|---|
| **Code Quality** | **7** | Compact core, explicit outcomes, extensive layout assertions and useful negative tests. Dense formatting, the exporter callback window, omitted GC participation and a warning-sensitive build regression reduce maintainability and confidence. |
| **Memory Safety** | **4** | Native ownership defenses are substantial, but the supported Python buffer surface can return invalid memory and crash. An unreachable cycle retains a native lease, and the constrained hook probe in §17 demonstrates a separate heap use-after-free. This category is blocked until fixed and requalified. |
| **Benchmark Reproducibility** | **6** | Native trace/bitmap replay and source/binary identities are good foundations. Final-package manifest drift, missing cache denominator data, unreviewed Apple raw evidence and incomplete original qualification prevent a higher score. |
| **Architecture Elegance** | **8** | Separate SPSC/NCQ progress contracts, publish-first descriptors, offsets, explicit alias lifetimes and conservative failure semantics are strong. Two-queue arbitration and the managed one-shot lifecycle impose real complexity and costs. |
| **Production Hardiness** | **4** | It has useful fail-closed mechanisms, but no automatic authority takeover, no durable recovery, no hard caller fairness, and unresolved Python release blockers. The supported operational envelope is narrower than “enterprise-ready fabric.” |

**Decision:** NO-GO for unqualified production deployment of this combined release. CONDITIONAL GO for continued native-only evaluation in a trusted single-host cohort, with bounded operations, explicit external lifecycle management and measured deployment-specific service objectives. A public showcase is strongest when it includes the discovered witness and fix history instead of claiming the tests established universal safety.

---

## 12. Integration decision: HFT and distributed inference

### 12.1 High-frequency trading or another deadline-sensitive path

This package is a same-host communication primitive, not an execution-control system, durable order ledger or hard-real-time scheduler. Before placing it on a critical trading path, an architect should require:

**Determinism at the application boundary.** Define offered load, burst size, payload schema, retained-lease budget, maximum acceptable queue age and behavior when capacity is unavailable. A nanosecond median without a tail/miss-rate contract is not a risk-control argument. A bounded SPSC topology is often easier to provision than a heavily contested NCQ, but changing to lanes changes ordering and routing and must include the merger cost.

**Failure semantics for external effects.** Record a stable application identity and decide where durable intent, acknowledgement and deduplication live. A consumer can perform an effect and die before returning its token. Do not let a missing transport return trigger an unexamined repeat of a non-idempotent external action. This requirement belongs above the queue.

**Host admission and observability.** Use recorded CPU and memory placement, power/thermal policy, no unexpected quota oversubscription, setup/residency admission, watchdogs and per-endpoint progress metrics outside contested data cells. Keep original offer timestamps and all stalled requests in the service denominator. Qualify compiler/SDK/runtime changes as new binaries.

**Containment.** Keep arbitrary Python object management and finalizers out of the most stringent native deadline path. Explicitly close leases. A process that may corrupt shared control cannot be treated as an isolated customer merely because its payload view is read-only. Stop rather than force-reclaim an unfenced writer.

The current audit does not recommend this combined 1.0.0 Python release for that role. Even after the exporter fix, workload-specific qualification and external transaction controls remain necessary.

### 12.2 Ultra-low-latency inference pipelines

For trusted CPU-to-CPU workers on one host, direct borrowed views can avoid redundant payload copies. Large batches may amortize Python control overhead. But holding a view through slow inference retains a slot; with eight large slots, eight retained borrows can exhaust capacity. Backpressure and memory budgets must include compute lifetime, not only transport function duration.

A distributed inference pipeline crossing machines requires another network transport. An offset in this shared mapping is not a remote address. Likewise, Apple unified memory does not automatically extend a C11 CPU ownership proof to GPU command completion, DMA or device access. An asynchronous device operation must keep the host lease alive until the relevant device completion is established, or copy into independently owned storage under a separately documented path. Those extensions are not implemented by this ring's release store.

After fixes, scope deployment first to a small trusted cohort with explicit endpoints, an owned lifecycle authority, fixed payload contracts and measured sustained load. Reject speculative claims about cross-host zero-copy, GPU-safe lifetime, or exactly-once crash recovery.

---

## 13. Required remediation and requalification order

### P0 — release safety and artifact identity

Fix A09-01's pending-export transaction. Add deterministic tests that interleave exporter acquisition with commit, abort, read release, exporter close and endpoint close. Exercise retained `.obj`, root/slice/cast/downstream buffers, failure/exception callbacks and both queues. Require an acquisition either to hold a valid pin or fail cleanly. No NULL/nonempty buffer, no transfer with pending exports.

Fix A09-02 with a reviewed GC-aware ownership implementation. Add unreachable cycles and finalization/resurrection tests; assert collection or an explicit, observable retained-failure state, not silent forever-busy resources. Do not release native memory just to make GC tests pass.

Regenerate the standalone manifest after all packaging edits, include the license and exact set of release members, and test the extracted final archive. Repair the GCC warning without weakening the strict diagnostic profile. Run the corrected candidate under the supported Linux compilers and native Apple/CPython 3.14 with the new schedules, not only the old tests. Publish a fresh version/commit/hash rather than overwriting the 1.0.0 evidence.

### P1 — qualification gaps that affect deployment claims

Complete the planned linearizability/finite-state and weak-memory tests with explicit bounds and negative mutants. Re-run near-ceiling and stalled-owner histories on target binaries. Exercise more than the three selected crash histories: grant/attach/detach windows, notification gaps, late publication during retirement, authority death and resource-cap exhaustion.

Export cache-control denominator and per-worker evidence. Deposit the actual Apple run package with SoC/OS/SDK/compiler/Python identities, binary and source hashes, timer ratio/calibration, all summary/trace/bitmap artifacts and repetitions. Run the separate one-way/open-loop profiles before claiming their targets. A 100M closed-loop run remains useful evidence even when it is not that qualification.

### P2 — performance work only after the safety boundary is stable

Measure contender-local attempt counts in a separate diagnostic build, larger working sets, payload schemas used by the application and retained-view pressure. Consider fewer contenders, routing or SPSC lanes when semantics permit. An SCQ variant or weakened-order NCQ is a new proof/validation project; replacing CAS with FAA or changing SC to acquire/release is not an approved micro-optimization.

Retain every failed run and each changed binary identity. Do not select only the best topology or omit slow intervals to restore a headline.

---

## 14. Publication-safe claim sheet

| Avoid | Use instead, with the actual attached evidence |
|---|---|
| “Zero latency” | “Zero intermediate payload copy for direct construction and borrowed consumption; timing remains nonzero and workload-specific.” |
| “250 ns IPC latency” | “Lab-reported 250 ns median instrumented SPSC RTT, two processes, two rings, 64-byte request/reply, one outstanding exchange.” |
| “125 ns one-way from RTT/2” | “One-way latency was not established by halving RTT.” |
| “22.8 GB/s Python” without workload | “Lab-reported 1 MiB Python/ctypes goodput with native full-span pattern work and capacity eight; bytes counted once.” |
| “4.04B RMW/s means subnanosecond CAS” | “Aggregate independent-counter RMW throughput across the recorded workers; not contended queue-operation latency.” |
| “128-byte alignment eliminates all false sharing” | “Designated ABI cells are isolated for admitted 64/128-byte granularities; true sharing and other cache effects remain.” |
| “Lock-free means every consumer is bounded” | “Conditional system-wide queue progress; individual starvation and OS delay remain possible.” |
| “64-bit counters solve wraparound ABA” | “Represented identity repetition is excluded by nonwrapping ceilings and session lifetime rules.” |
| “Crash cleanup returns every token” | “Uncertain tokens can remain orphaned; managed whole-generation retirement/fencing prevents unsafe reuse.” |
| “All tests prove memory safety” | “Named finite tests passed; this audit found an additional failing exporter schedule and GC cycle.” |
| “1.0.0 is fully production-qualified” | “Candidate release with explicitly listed safety fixes and deployment qualification gates.” |

No salary number, employer name or version label changes these technical predicates. The credible systems showcase is the implementation, exact scope, reproducible counterexample and disciplined repair.

---

## 15. Evidence index and reproduction

The companion audit bundle is intentionally separate from the production archive. It contains no replacement production binary and does not patch the user's source.

- `evidence/ARCHIVE_SOURCE_IDENTITIES.json`: downloaded ZIP identity, actual file count, original source hashes and unchanged working-copy verification.
- `evidence/manifest_audit.json`, `verify_release.log`: the final-manifest failure.
- `evidence/SOURCE_EXCERPTS.md`: relevant exact code with original file line numbers.
- `repros/export_race.py`: deterministic two-thread export-versus-transfer reproducer, four mode/role combinations.
- `evidence/export_race_results.json` and `export-*.log`: all 20 reproduced failures, exit codes and named-object cleanup.
- `repros/gc_cycle.py`, `evidence/gc_cycle.log`: unreachable-cycle retention and controlled cleanup.
- `repros/pending_export_candidate.patch`, `candidate_*`: isolated mitigation, four safe-block results and baseline suite result.
- `evidence/*baseline*`, `build_and_tests*`, `ncq_*`, `*asan*`: fresh builds, baseline suites, asymmetric runs and sanitizer outcomes.

Build the unmodified standalone project in a fresh local directory using its own Makefile and supported Python headers. The audit's baseline commands use `BUILD=build/audit` and `CC=gcc`; the Clang lane uses `build/clang`. Set `ELITE_LIBRARY` to that exact library and `PYTHONPATH` to its exporter output followed by `bindings/python`.

**Run crash reproducers only in isolated child processes.** They intentionally terminate a vulnerable interpreter. The supplied portable runner, `repros/run_export_races.py`, bounds each owned child, disables core dumps, saves output and reaps it before unlinking only its exactly reported test object. It does not search process names, kill unrelated services or scan/delete a shared-memory namespace. Its commands and target source identity must be retained when results are compared.

The candidate patch addresses one identified transaction window. Treat it as review material until the complete corrected release and native Apple lane are retested. The GC defect remains open in that candidate.

---

## 16. Source ledger and attribution

### Audited primary artifact

**S00.** `elite-ringbuffer-1.0.0.zip`, input hash at the top of this document. Retrieved from the user's connected Drive standalone-release folder. Source paths and line numbers in this report refer to that exact archive. It is the authority for code findings; earlier conversation wording is not substituted for it.

**S01.** `bindings/python/elite_buffer.c`, SHA-256 `ca4d6ffa5f05a3beb839129a11993a5a1c1f33d62420abd840dc4d24ad7f565e`. Export acquisition, closure, GC participation and benchmark native workload.

**S02.** `bindings/python/elite_ringbuffer/__init__.py`, SHA-256 `1017c3ee7adcb08abbdd688142259bf7a6e7e00eb1bc7b6f4dc3546dd5462692`. Endpoint locking, lease/pin lifetime, finalization and timeout behavior.

**S03.** `bindings/python/elite_ringbuffer/_native.py`, SHA-256 `42bdd5ee9accd1e87b2abd76ec7cdcc71f1ddb4321530375403b7dad389c60ce`. Private ctypes declarations and ABI admission.

**S04.** `src/elite_spsc.c`, SHA-256 `fac8a92d5a71c8475769586b2744ed39a99b57cb29e9704efcacbec59c84e774`. Private reservation, covering acquire and two release transfers.

**S05.** `src/elite_mpmc_ncq.c`, SHA-256 `540715143bf5f9b90477f212e52dfa60bebeb55e0bc88b983f8a71affc11cec9`. Publish-first NCQ-SC64, SC strong-CAS metadata and bound checks.

**S06.** `src/elite_shm.c`, SHA-256 `e009639a9c80381afc4e3460c33eb26b79d236bb5a34e67e76d01d694fb09569`. Initialization, managed holder state, permission correction, detach/destruction and the GCC warning location.

**S07.** `benchmarks/bench_latency.c`, `bench_throughput.c`, `bench_cache.c`, `bench_common.c`, and `bindings/python/bench_python.py`. Exact hashes are in the source inventory. Support the measured-interval, pattern, pool-size and missing-cache-denominator observations.

### Governing and historical project sources

**P04.** Turn 4 ABI, literal hash `6aaa3d6706c29096cfab56ea1fe569afa561c4cba7caea26ebd3c09cc9812386`. Shared-format, order, lifetime, counter and failure contracts; not a current test result.

**P05.** Turn 5 test plan, literal hash `bc7ae2702cecb8d46820cf5365046fe40dda36c59dde9c80fda98ff7833ecdf8`. Distinct qualification metrics, repetitions, formal models and chaos coverage; requirements are not automatically executed evidence.

**H07.** Turn 7 hardware report, literal hash `48515cca5e5a34b3ab8c2790267a1d634f664d581a57cd0828f49828fe8c4604`. Historical Linux 100M results and their limits, provided in the project context and release history. Not newly rerun here.

**U09.** Current commissioning statement. Apple qualification, 22.66/22.79 GB/s Python goodput and standalone-release claims remain lab-reported unless independently bound to supplied artifacts. The actual downloaded archive resolves the packaging question; no unprovided Apple raw trace is invented.

### External primary documentation verified during this audit

**W01 — CPython buffer protocol.** https://docs.python.org/3/c-api/buffer.html  
Used for buffer field/lifetime obligations and the limits of FillInfo, not as evidence that this implementation is safe.

**W02 — CPython cyclic-GC support.** https://docs.python.org/3/c-api/gcsupport.html  
Used for container traversal/clearing obligations. The concrete retained cycle is independently reproduced.

**W03 — Python ctypes.** https://docs.python.org/3/library/ctypes.html  
Documents CDLL GIL release. The wrapper's lock coverage is established from its source, not inferred from the documentation.

**W04 — Linux shm_open/shm_unlink.** https://man7.org/linux/man-pages/man3/shm_open.3.html  
Used for name/descriptor/mapping lifetime distinctions and the Linux tmpfs namespace. No Darwin or universal lifecycle behavior is guessed from Linux alone.

**W05 — Apple XNU affinity contract.** https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/osfmk/mach/thread_policy.h  
Used to distinguish affinity tags from P-core-number binding. A live source header is not a trace of the lab's scheduler placement.

**W06 — GCC atomic builtins.** https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html  
Supports compare/exchange expected-value, strong/weak and order distinctions. Kernel or compiler documentation does not replace the admitted C11 IPC/refinement proof.

**W07 — Linux kernel atomic forward progress.** https://docs.kernel.org/core-api/wrappers/atomic_t.html  
Used only as a primitive-implementation warning about progress/LL-SC, not as the userspace ring's ordering API.

**W08 — Linux mmap.** https://man7.org/linux/man-pages/man2/mmap.2.html  
Mapping and SIGBUS/backing-extent limits. No host-wide pressure/destructive test was run.

**W09 — Clang sanitizer documentation.** https://clang.llvm.org/docs/AddressSanitizer.html and https://clang.llvm.org/docs/ThreadSanitizer.html  
Instrumentation has executed-path and address-space scope. New observed results are recorded in logs; no clean log is upgraded to exhaustive concurrency proof.

---

## Original-stage audit decision (retained)

**The native design is worth preserving. The current Python release is not safe enough to publish as production-qualified.** Repair the pending-export transaction, account for cyclic Python reachability, correct the final manifests and sanitizer build, then requalify the exact new candidate. Keep the impressive Apple numbers, but attach the experiment and evidence boundaries that make them defensible.

The decisive result is not another 100 million successes. It is a small, reproducible schedule that breaks a claimed lifetime invariant while the existing suite stays green.


---

## 17. Continuation verification and additional findings

### 17.1 Recovery of the interrupted audit, not a restart of the experiment

The first completed report was already deposited in the requested Drive folder. Its exact bytes were recovered: **68,413 bytes**, SHA-256 **`639ea865cb8cf70a746b7d3916fda6211e0980ff0d8a210c30f3f20e2b6937a4`**, Drive ID `1dNDsvk5FMEx8oCJbwkS0TNCKYff05knP`. The original evidence ZIP was also recovered and rechecked: **103,916 bytes**, SHA-256 **`08a300574934e27c68aaa49090ddbdc9a7f2e799b734db0781b18a9dd7bf2ad0`**. ZIP CRC and all **87** original manifest entries match. That report is preserved unchanged in `history/09_Audit_Revision_1.md`.

Additional reproducers and logs survived the interrupted response but were absent from that deposited revision. A locally recovered later `.sha256` names a different, unavailable draft; it is retained as `history/unresolved_late_draft.sha256`, not used as a digest for this document. This revision integrates the verified findings below and receives a newly computed literal checksum. No historical byte identity is silently reused.

The standalone source archive was freshly extracted and independently rechecked. It remains **3,959,129 bytes**, SHA-256 **`9d8a9843ecbde3cd5dcc780a744f398c2638a92a0a656b3aaa86ddc72cec2907`**. Every one of its **486 regular input files** is unchanged in the new test working copy. All **46 source-manifest entries** still match; `.gitignore` still differs from the complete manifest and `LICENSE` is still unlisted. These are source/artifact findings, not evidence of malicious alteration.

The continuation used Linux/x86-64, CPython **3.13.5**, Clang **17.0.0** and GCC **14.2.0**, with allowed CPU IDs 0–4 and a four-CPU-time quota. Full records are under `evidence/resumed/`. Native Apple or Python 3.14 execution is **not** newly claimed. Tests exercise the user's own candidate in isolated child processes; no production service or original source is modified.

### 17.2 A09-07 — Finalizer error reporting can resurrect a freed exporter

**Classification:** a distinct heap-use-after-free, high impact but with an explicit, non-default trigger. It is not the NULL-backed buffer defect in A09-01 and is not alleged to occur on every normal cleanup.

**Source:** `bindings/python/elite_buffer.c:85–96`. `buffer_dealloc` calls `close_buffer`. When cleanup raises, it calls `PyErr_WriteUnraisable((PyObject *)self)` and subsequently clears the owner and unconditionally calls `tp_free(self)`. That error-reporting call invokes Python's configurable unraisable hook with the object being destroyed. CPython documents both the hook call and the fact that retaining `unraisable.object` can resurrect an object under finalization. Its documentation explicitly advises hook authors not to do so. [W10–W12]

**Reproduced history:**

1. Obtain a legitimate write lease and its ordinary memoryview; release the view so the exporter has no current exports.
2. Drop the outer lease so its exporter reaches destruction.
3. A deterministic trace hook raises `KeyboardInterrupt` on entry to the real `_drop_view_pin` callback. This is an injected exception at a Python callback boundary; no address or production code is changed.
4. A deliberately retaining custom `sys.unraisablehook` stores its `object` argument when it is this `LeaseBuffer`.
5. The destructor continues into `tp_free` even though the hook has retained a Python reference.
6. Reading the saved object's ordinary `.closed` property accesses the freed object.

The fresh sanitizer replay reports:

`AddressSanitizer: heap-use-after-free ... READ of size 4 ... buffer_closed ... elite_buffer.c:82:46`.

The corresponding allocation is a 64-byte Python exporter object; the reported field is 56 bytes into that freed region. The child terminates with sanitizer abort (`returncode=-6`). Evidence: `repro/unraisable_resurrection.py`, `evidence/resumed/probes/resurrection_asan.stdout`, and `resurrection_asan.stderr`. The earlier saved sanitizer result is preserved separately under `late_recovered/evidence/final_packaged_probe_replay/`.

**Scope is essential.** The custom hook violates Python's documented recommendation against retaining `unraisable.object`; this is a hostile/buggy-hook hardening result, not a claim that the default hook produces this outcome. It nevertheless answers the commission's question about whether Python-level behavior, without pointer fabrication, can reach a native use-after-free. No double-free, remote entry point, arbitrary code execution, data exfiltration, or successor-ring corruption is established. Natural failure frequency is not measured.

**Required remedy:** do not expose a dying, unconditionally freed `self` through an arbitrary Python callback. Define a finalization/resurrection-safe state machine and use appropriate CPython finalization/lifetime rules, or report an immutable safe context instead of the dying exporter. A complete repair must cover exceptions, owner decrefs, resurrection, cyclic GC and existing exports together. A superficial reference-count check after one callback is not a proof that all destructor paths are safe. The pending-export candidate in §3.4 addresses a different window and must not be described as repairing A09-07.

### 17.3 A09-08 — Native acquisition can succeed before Python owns a recoverable lease

**Source:** `bindings/python/elite_ringbuffer/__init__.py:328–341`. `_obtain` allocates process-local lease/span structs, calls native reserve/borrow at line 334, then constructs `_Pin`, records its weak reference on the endpoint, and wraps it in `WriteLease`/`ReadLease` at lines 337–340. Those steps are not one exception-atomic transaction.

The test raises `KeyboardInterrupt` through `sys.settrace` at the first actual line boundary after the native result is assigned, before `_Pin` adoption. The native producer has acquired a token, but the public call never returns a lease and no Python pin has been installed. The trace hook is an explicit fault injection; it does not pretend to measure the natural timing of SIGINT.

The fresh observed sequence is:

| Observation | Result |
|---|---|
| Injected exception | Caught `KeyboardInterrupt` after native reserve |
| Public `producer.busy` | `false` |
| Another `try_reserve()` | `BUSY`, outcome `RETAINED` |
| `producer.close()` | `BUSY`, outcome `RETAINED` |

Evidence: `repro/acquire_exception.py` and `evidence/resumed/probes/acquire_exception.stdout`. The recovered earlier run agrees. The release's native ownership checks prevent a second claimant from stealing the token: that is a successful safety boundary. The wrapper nevertheless loses its usable public cleanup handle and misreports its busy state. This is an **availability/exception-safety bug**, not a demonstrated memory corruption.

The `try/except` in `_Pin.finish` does not protect this earlier acquisition path. It would be incorrect to cite that transfer-error handler as coverage for every allocation, tracing, signal, or exception point surrounding acquisition.

**Required remedy:** install an acquisition obligation before entering the foreign call and keep it reachable until its result is safely adopted or explicitly classified uncertain. Exceptional exit must leave either a usable cleanup resource or a poisoned/uncertain endpoint state with accurate diagnostics. Native-to-Python adoption may need a tighter native wrapper rather than several independently interruptible Python bytecodes. Do not infer a safe abort when the native result was never received. Add fault injection before/after the call, pin creation, weak-reference installation, outer lease allocation, view retain/end, transfer-result adoption and detach acknowledgement. Genuine allocation failure and real signal delivery remain distinct further tests; this witness alone does not claim they were all executed.

### 17.4 Fresh continuation replay ledger

The original results in §10 retain their original provenance. The following were actually rerun during continuation against a fresh, unchanged extraction:

| Check | New result | Scope |
|---|---|---|
| Original audit ZIP/manifest readback | CRC clean; 87/87 entries match | Artifact recovery, not runtime correctness |
| Standalone source/member comparison | 486 original regular files unchanged; 46/46 source hashes match | Builds add separate generated directories only |
| Shipped release verifier | Exit 1, `.gitignore` mismatch | LICENSE remains unlisted |
| Clang strict native build/regression | Core 68, prefix 512, SPSC/NCQ 100k, adversarial 8, limits 4, chaos 3, prefix fuzz 1M passed | Linux only |
| Uninstrumented Python baseline | 36/36 passed in 18.780 seconds | Existing suite, not added fault schedules |
| Clang ASan/UBSan native/exporter build | Success | Enables isolated probes; does not by itself certify runtime |
| Export/close race, without dereference | Returns length-64 view with closed exporter and inactive write lease | Logical contract failure even when child exits 0 |
| Same race, `bytes(view)` | Child SIGSEGV (`-11`) | No fabricated address |
| Same race with ASan/UBSan | NULL-page read and abort (`-6`) | Distinct from heap-use-after-free |
| GC-cycle replay | Endpoint survives `gc.collect`, remains busy, no cleanup diagnostic; explicit break frees token | Resource retention witness |
| Acquisition exception replay | `busy=false` but native reserve/detach both BUSY | Exception-safety witness |
| Retaining-unraisable-hook replay | ASan heap-use-after-free, `buffer_closed`, abort (`-6`) | Explicit hostile/buggy-hook condition |
| GCC O1 ASan/UBSan build | Fails strict sign-conversion warning at `elite_shm.c:66` | No corresponding GCC runtime pass |
| Additional full ASan Python-suite attempt | Outer 25-second watchdog expired, exit 124; no final suite result | **INCOMPLETE**, not a pass or demonstrated product failure |

No leftover replay/test child from that bounded run remained when checked. Faulting probes are separate owned children with core dumps disabled. Their exact self-reported names are unlinked only after they are reaped; cleanup never scans a namespace or targets an unrelated process. `repro/replay_saved_probes.py` records native/exporter hashes, commands, return codes, timeouts and cleanup outcomes. The fresh full-suite timeout is retained rather than replaced by an unqualified statement that every newly attempted sanitizer run passed. The earlier 36/36 instrumented pass in §10 remains earlier evidence, not this incomplete run's result.

The newly reproduced failures require no change to the production source. They establish reachable behaviors of the identified release, not a claim that our deliberately scheduled test is a normal workload or that all Apple/runtime variants were executed.

### 17.5 Scores and release decision after continuation

The scorecard in §11 is retained: **Code Quality 7/10; Memory Safety 4/10; Benchmark Reproducibility 6/10; Architecture Elegance 8/10; Production Hardiness 4/10.** These are judgment categories, not probabilities or an average that can waive a blocking defect. The two additional witnesses strengthen the existing no-go for the combined production-safe Python claim; they do not invalidate the native algorithms by association.

The priority order is now:

1. Make buffer acquisition versus closure atomic at the exporter lifetime level; count pending exports before callbacks.
2. Repair GC participation and finalization/resurrection without dropping active lease ownership.
3. Make native acquisition/return adoption exception-safe, or explicitly poison and retain uncertain endpoints.
4. Fix the strict GCC lane and regenerate complete manifests after all release edits.
5. Run the new negative histories on native Apple/CPython 3.14 and supported Linux runtimes; then run the remaining formal, fault-matrix, and workload-specific qualification.

The cache and goodput claims keep their experiment-specific scope. A 250-ns RTT median, a 22.79-GB/s native-assisted Python payload workload, and an aggregate independent-counter result can all be real while an exceptional buffer path is unsafe. The proper engineering response is a patched, newly identified candidate with preserved evidence, not declaring the research illegitimate and not declaring a race harmless because the benchmark was fast.

### 17.6 Additional primary sources

**W10 — CPython `sys.unraisablehook`.** https://docs.python.org/3/library/sys.html#sys.unraisablehook  
Confirms that the hook receives the error-context object and warns explicitly that retaining it can resurrect an object being finalized. Used to constrain the A09-07 trigger, not to conceal that it is non-default behavior.

**W11 — CPython C API exception handling.** https://docs.python.org/3.13/c-api/exceptions.html#c.PyErr_WriteUnraisable  
Documents the hook invocation by `PyErr_WriteUnraisable` and the contextual object argument. The unconditional free and the UAF result are established from this project's source and sanitizer evidence, not from the manual.

**W12 — CPython object lifecycle.** https://docs.python.org/3/c-api/lifecycle.html  
Documents finalization, possible resurrection and deallocation responsibilities. It motivates a full lifecycle repair; no untested patch is certified by citation.

**W13 — CPython container GC support.** https://docs.python.org/3.13/c-api/gcsupport.html  
The type holding an owner reference must expose relevant traversal/clearing behavior. Collection must remain compatible with borrowed-buffer lifetime.

**W14 — CPython ctypes.** https://docs.python.org/3.13/library/ctypes.html  
Foreign calls may release the GIL; local method locks and buffer acquisition/finalization require actual coverage rather than an assumed all-encompassing GIL transaction. The project's `_native.py` is reviewed for signatures/loading; the demonstrated higher-level adoption gap is in `__init__.py`.

These live primary pages were read during continuation. Their documentation version is not used as evidence that the experiment ran on that interpreter version; the execution logs identify CPython 3.13.5.

## Final completed verdict

**Keep the native design and the reproducible benchmark work. Hold the combined 1.0.0 production-safe Python release.** There is a reproduced invalid-buffer race, a separately reproduced constrained heap-use-after-free, an unreachable-cycle retention defect and a native-acquisition adoption gap, plus concrete packaging/toolchain issues. The corrective work is localized but safety-critical. A showcase publication should carry the exact scope and failure/fix history; a deployment must wait for a corrected identified candidate and its relevant qualification.
