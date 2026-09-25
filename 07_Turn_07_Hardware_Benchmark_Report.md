# Turn 07 — Hardware Benchmark Implementation and Scoped Results
## ELITEIPC / LE128-V1 / SPSC and NCQ-SC64

**Project:** Leonid Majbits / Gemini Operator Lab, paired with Leon  
**Date:** 2026-09-23  
**Stage:** 7 of 8 — native benchmark implementation and execution  
**Disposition:** Required C benchmark files implemented; seven full100M conditions completed on Linux/x86-64. New Apple-native performance and P-core counter qualification NOT_RUN here.  
**Canonical-SHA256:** `384f8cb7b2b3ff5e657f167f78c63f0df910d508b07aa8611adad6061ab07f99`  
**Hash convention:** Normalize only this field's64 hexadecimal characters to ASCII zeros before hashing. The adjacent sidecar authenticates literal finalized bytes.  
**Turn6 parent ZIP:** 210,732 bytes; SHA256 `e0ee969f4fb26601b8150f2ad9fa7bdfabda6c9c211dd94773951a836b5ed92a`. Verified before extraction.  
**Frozen ABI:** Turn4,93,610 bytes; SHA256 `6aaa3d6706c29096cfab56ea1fe569afa561c4cba7caea26ebd3c09cc9812386`.  
**Frozen verification plan:** Turn5,96,883 bytes; SHA256 `bc7ae2702cecb8d46820cf5365046fe40dda36c59dde9c80fda98ff7833ecdf8`.  
**Destination:** Deployment Research / Elite_Systems_LockFree_RingBuffer.  
**Evidence convention:** [P4] and [P5] are preserved project specifications; [P6] is the inherited implementation report. [U7] is the user's Apple qualification statement, not an independently retrieved raw run. [R1]–[R8] are primary documentation/source references below. `evidence/turn07/` is newly executed local evidence. Earlier evidence is historical. Transfer verification is recorded separately after real upload/readback.

---

## 1. Delivery verdict and source continuity

The requested `benchmarks/bench_latency.c` and `benchmarks/bench_throughput.c`
are working native C executables using the existing public library. Additional
common support, an offline C analyzer, a separate cache-interference control,
and stdlib-only Python orchestration/replay tools complete the package.
There is no substitute queue, per-message pipe payload, live ABI mutation,
unchecked timing-only API, or invented Apple measurement.

Seven of the eight inherited native include/source files are byte-identical
to the verified parent. The only change is the creation-permission handling in
`src/elite_shm.c`. `CORE_DELTA.json` records both byte hashes for every file;
`docs/DARWIN_SHM_PERMISSIONS.patch` preserves the exact delta. All15 source
inputs recorded by the full100M runner match the delivered source bytes,
including Makefile and benchmark files. The new ordinary native benchmarks
contain no fault-hook or sanitizer build instrumentation.

### 1.1 Credited Darwin correction

Gemini's Apple run found the inappropriate `fchmod` call on a POSIX shared
memory descriptor. The examined XNU `fchmod` path obtains a vnode; its descriptor
conversion rejects a non-vnode with EINVAL. This supports the reported failure
mechanism rather than assuming Linux tmpfs behavior for Darwin. [R1]

This bundle reconstructs the lab's correction: create exclusively with0600,
omit `fchmod` on Apple, retain it on Linux, then verify owner and exact0600
permission bits with `fstat` before sizing/mapping/exposure. A restrictive umask
that removes owner access causes failure rather than temporarily changing the
whole process's umask. That extra verification is this turn's implementation,
not a claim to possess the lab's exact modified file. The patch changes setup,
not the shared byte format or queue operations. The Linux branch was exercised
in all newly executed tests; the patched Darwin branch still needs a native
rerun of this exact derivative.

### 1.2 Apple evidence received versus evidence created

[U7] reports core68,512 mutation rejections,100k SPSC,100k NCQ4/4,
adversarial8,limits4,chaos3,fuzz1M,sanitizer and disassembly successes on
Apple hardware. This report records them as **LAB_REPORTED**. It does not
invent raw compiler/SDK/binary hashes, exact CPU model, full sanitizer coverage,
or100M Apple observations from that statement. Native correctness testing is
valuable; the new100M RTT/scaling investigation remains a different campaign.

---

## 2. Actual local100M measurements

Host: **Linux-6.18.44-x86_64-with-glibc2.41**, reported CPU **Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz**,
4KiB pages, five allowed logical CPU IDs0–4, cgroup quota400000/100000
microseconds (four CPU-time equivalents). The full sweep explicitly mapped
workers round-robin to CPUs0,1,2,3 and checked each worker's one-CPU affinity
mask before and after its measured phase. The controller had no dedicated
isolated CPU. This is a shared container, not a quiet16-core Apple system.

Compiler: GCC14.2.0; C11 `-O3 -g -DNDEBUG -fno-omit-frame-pointer -fno-lto`,
strict inherited warning set, baseline x86-64 generic tuning, native PIE
executables and the original static library. Exact flags, command lines,
binary hashes and source hashes are retained in the build logs and run
`provenance.json`. The return code of the complete seven-condition runner
was zero. Each condition is **one fresh100M trial**, not five qualified trials.

### 2.1 Raw instrumented round-trip distributions

Each row contains100,000,000 checked round trips and200,000,000 measured
directional64-byte payload records, after1,000,000 warmup round trips.
There are two separately spawned processes, four endpoints and two independently
backed rings. NCQ is1P/1C in each direction, not contended multi-client RTT.

All table values are **nanoseconds**. On this host the raw-clock scale is1:1.

| Mode | p50 | p90 | p99 | p99.9 | p99.99 | Maximum |
|---|---:|---:|---:|---:|---:|---:|
| SPSC | 660 | 791 | 864 | 20,924 | 277,748 | 213,535,770 |
| NCQ-SC64 | 805 | 896 | 1,060 | 43,247 | 410,436 | 145,048,128 |

Both runs checked each expected request and response ID, all64 payload bytes,
length/type metadata and returned ownership. Both rings reconciled all tokens
and expected cursor deltas after warmup and after measured work. There were no
observed bad, missing, duplicate, or unreturned records in these completed
failure-free trials.

The tails are retained in their entirety. The largest values are not replaced
with a cache-interconnect estimate or trimmed as outliers. Their precise causal
attribution is not established by these aggregate records; context-switch and
fault counters are available as diagnostics. The one-way15/50-ns goals cannot
be accepted or rejected from RTT/2. The one-microsecond open-loop service gate
also has a different interval and workload. This table establishes the named
instrumented RTT observations only. [P5,§§4–6]

### 2.2 Validated drained-cohort goodput

Each row contains exactly100,000,000 measured64-byte messages after1,000,000
same-generation warmup messages. Every consumer's complete private bitmap is
saved, pairwise-disjointness checked, all expected IDs present, and all tokens
returned. Producer quotas and consumer populations reconcile exactly.

| Mode | Producers / consumers | Validated messages/s | Delivered payload GB/s (decimal) |
|---|---:|---:|---:|
| NCQ-SC64 | 1 / 1 | 7,669,928.662 | 0.490875434 |
| NCQ-SC64 | 2 / 2 | 4,659,155.018 | 0.298185921 |
| NCQ-SC64 | 4 / 4 | 3,370,411.001 | 0.215706304 |
| NCQ-SC64 | 8 / 8 | 3,575,175.822 | 0.228811253 |
| SPSC | 1 / 1 | 9,480,193.239 | 0.606732367 |

GB/s denotes application payload delivered once, not DRAM bandwidth, cache-line
traffic or doubled read/write traffic. Throughput includes public API calls,
direct construction, full read validation, private bitmap updates, start skew,
and final drain/control tail. It excludes offline bitmap export/merge.
The code never reports reciprocal throughput as message latency.

The decline with additional contending workers is an observation on this
four-CPU-time budget, not a measured Apple scaling law. No attempt counter or
hardware coherence event was collected for these primary rows, so this report
does not claim the conjunctive24-attempt scaling kill gate was evaluated.
Eight producers plus eight consumers means16 endpoint workers sharing four
requested CPU IDs. Extra concurrency is not extra available processor service.

### 2.3 Raw evidence replay

The full campaign exports two starts/duration trace pairs, pre/post calibration
traces, every consumer bitmap, saved union bitmaps, all result JSON, source and
binary identities, and the complete raw-file SHA256 manifest. The 3,494,545,328-byte raw
set is supplied in a separate 935,222,599-byte ZIP evidence archive; the smaller benchmark
bundle carries summaries, manifests, source and replay tools without duplicating
those gigabytes. See `evidence/turn07/Linux_100M_Summary/` and
`LINUX_100M_ARCHIVE.json`.

Offline verification successfully rehashed the full run manifest, recomputed
all RTT/calibration nearest ranks, checked sequential start/end consistency,
and independently merged every saved membership bitmap with bounded memory.
`replay-100M.log` and `replay-100M-exit.txt` record the successful replay.
An earlier outer tool invocation expired during offline verification; that
incomplete invocation is retained explicitly. It was not a failed native100M
transfer, and the replay retry did not replace or rerun the measured samples.

---

## 3. What exactly the latency executable measures

For request ID i, the origin reads an ordered tick immediately before its first
public write-reserve attempt. It directly constructs the64-byte request and
publishes it. The responder borrows, validates and releases the request, then
directly constructs/publishes the reply. The origin borrows, validates and
releases the reply, then reads its stop tick. Thus:

T_RTT(i) = t_origin_after_response_release(i) - t_origin_before_request_reserve(i).

One request awaits a reply at a time. Payloads travel only through the two
POSIX rings. Setup pipes carry grants, phase commands and final results, never
the data payload. Reply construction is direct; it is not forwarding an old
borrowed pointer after reclaiming it. The origin saves timestamps in private
preallocated arrays only after its leases ended. No post-publication write
enters a transferred descriptor, and no timestamp is hidden in reserved ABI
bytes.

`--count100000000` means100M round trips, not100M total directions or100M split
across trials. The new metric is intentionally separate from [P5]'s one-way
entry-to-final-read interval, which excludes response processing and may end
before release. The report cannot subtract responder work or divide by two to
turn this measured interval into that different one.

### 3.1 Clock path and conversion

Darwin uses `mach_absolute_time` and its actual `mach_timebase_info` ratio.
Linux uses `clock_gettime(CLOCK_MONOTONIC_RAW)` and separately queries reported
resolution. These are OS-clock interfaces; their unit/implementation properties
must not be equated with current core cycles or an assumed free direct PMC
read. [R2,R3]

Compiler motion barriers plus DSB ISHLD/ISB on AArch64, or LFENCE on x86-64,
order the measurement boundary. The core's atomic code is unchanged. The
extra measurement instructions are included in raw timing; there is no global
median-overhead subtraction. Actual Apple boundary lowering/API behavior is
still a target-admission requirement. A Linux clock may be serviced through a
vDSO, but this is not asserted for every deployment and does not eliminate the
cost of reading/order-enforcing the clock. [R3]

Differences are converted with overflow-checked quotient/remainder arithmetic.
Raw integer ticks are the primary values; integer nanosecond summaries round
up. A returned scaling ratio does not establish actual effective resolution.
The program detects backward observations and arithmetic overflow rather than
wrapping, clamping away a failure, or silently dropping a sample.

### 3.2 Calibration and uncertainty

One million ordered empty read pairs are measured before warmup and after the
measured epoch. Both Linux RTT profiles observed25ns calibration medians in
this run. This number includes the empty timing path and is not a proof of
25ns counter resolution. Long calibration outliers remain in their raw files.

The result marks absolute timing uncertainty **NOT_QUALIFIED**. It supplies
observations and replayable data, not the complete5ns uncertainty budget or
moving-block population-tail inference specified for the original15/50ns
one-way goals. A0-tick observation would not mean0ns hardware latency; a
submicrosecond display would not alone certify the required measurement.

No sampling profiler or periodic sampling handler is enabled by the main
benchmark. Ordinary OS interruptions, descheduling, timestamp instructions,
private-log cache traffic, controller polling and faults are still possible.
“Zero interrupt sampling overhead” is implemented only in the legitimate
sense of not adding a sampling profiler, not disabling the operating system.

---

## 4. Placement: actual Linux masks, honest Darwin requests

On Linux an explicit CPU list selects an actual single-CPU affinity mask for
each worker. Both set and get results are checked. A requested invalid or
restricted CPU rejects the run. CPU masks are constraints, not a promise that
the worker runs continuously or that the selected CPUs are a particular cache
cluster. The local raw results preserve every before/after mask. [R4]

On Darwin, the code performs the requested `thread_policy_set` and a
`thread_policy_get` readback when a tag is supplied. The examined XNU header
specifies affinity tags as cache-placement hints, generally task-local. Apple’s
archived API notes also distinguish hints from explicit processor binding.
**A tag is not a P-core selector.** Using the same tag in independently exec'd
processes does not establish a common affinity set. Failed/unsupported requests
are recorded; even successful readback does not earn a P/P label. [R5]

`--qos user-initiated` and `--qos background` request, rather than certify,
the indicated scheduling policy. Without external trace evidence the benchmark
leaves P-core pinning and cluster placement NOT_ESTABLISHED. It does not use a
private undocumented scheduler control, claim a measured interconnect cliff,
or infer a core count from a target compiler flag. This preserves [P5,§2.2].

---

## 5. Cache isolation and profiling

The frozen128-byte headers, cursor cells, queue-entry stride, descriptor
stride and payload-stride divisibility remain intact. The inherited compile-time
assertions still compile. New runtime validation checks mapped-base/region
alignment and canonical format geometry, then reconciles data only under
quiescence. For designated cells and admitted64/128-byte granularities,
disjoint128-byte cells have disjoint line sets. [P4,§6.5]

This is a byte-layout theorem with explicit hardware assumptions, not a PMC
measurement. Contenders accessing the same NCQ head or publication entry still
perform true sharing. Prefetching, cache conflicts and scheduling also remain.
A small generic cache-miss count would not establish universal absence of false
sharing, nor would a large count identify it without address/ownership evidence.

### 5.1 Separate packed/isolated sensitivity control

`bench_cache.c` runs independent relaxed-atomic counters in8-byte-packed and
128-byte-isolated arrangements, with exact counter reconciliation and alternating
order across five trials. It uses separate aligned scratch storage, not a
modified ring ABI. The local4-worker run requested CPUs0,1,2,3 and performed
5,000,000 increments per worker per arrangement. Observed ranges were:

- Packed8-byte counters: 64,377,578 to 67,042,747 RMW/s.
- Isolated128-byte counters: 241,890,354 to 335,637,169 RMW/s.

These are finite thread-only diagnostic observations, not IPC rates or an
Apple result. A prior100k unpinned smoke control even had the opposite apparent
speed order; it is retained, illustrating why a short uncontrolled ratio cannot
be a correctness criterion. No packed/isolated speed ratio is used to certify
the transport. TSan results for this control are not performance samples.

### 5.2 Hardware counters

`tools/profile_counters.py` performs actual availability discovery and records
commands/status. Darwin uses the installed xctrace template list and a chosen
exact template, without guessed M4 PMU event numbers. Linux uses perf stat when
installed/authorized. This environment had no perf executable, so the actual
probe reports **UNAVAILABLE_PERF**. No cache misses, cycles, cache-to-cache hits,
P-core stall fractions or IPC cache-line transfers are fabricated. [R6,R7]

A wrapper trace must be reviewed for worker PID/CPU scope: launch-scoped
recording can capture only the coordinator. Counting the whole command includes
setup, warmup and offline export/sorting, not just the measured transfer epoch.
Event availability, multiplexing, sample interval and that scope must be retained
before interpreting counters. Primary timing and profiling are separate runs.

---

## 6. Executed verification and deliberate failure controls

| Lane | Newly observed result | Scope |
|---|---|---|
| GCC strict build | All new C benchmark programs compiled | Linux/x86-64,GCC14.2.0 |
| Clang strict build | Same benchmark programs compiled | Linux/x86-64,Clang17.0.0 |
| Native core regression | core68,prefix512,IPC100k SPSC+NCQ4/4,adversarial8,limits4,chaos3,fuzz1M | Patched Linux setup; frozen queue logic |
| Benchmark tools | Eight test groups passed | Exact p99.99 rank, zero values, conversion/overflow edges, payload negative control |
| Native small sweep | Seven100k conditions plus cache control passed and raw replay passed | Functional harness verification |
| Full native sweep | Seven100M conditions completed; exact raw replay and manifest verification passed | One repetition each; results in§2 |
| ASan+UBSan | Seven10k conditions plus cache control completed, raw replay passed | Separate instrumented process/cohort runs; no performance use |
| TSan | Independent-counter4-thread packed/isolated control clean | Same-process diagnostic only, not IPC race certification |
| Negative controls | Ten expected rejections observed | Five CLI cases, watchdog, duplicate bitmap, bad RTT, incomplete campaign, Python-O |
| Apple-native new harness | NOT_RUN | Requires actual SDK/native deployment |
| Hardware-counter probe | UNAVAILABLE_PERF | No counter result |

The deliberate duplicate-bitmap test bypasses a directory hash check so exact
membership logic itself must reject a duplicate across consumers. The bad RTT
fixture induces invalid overflow/order, not only a summary mismatch. The
verifier refuses Python-O so its assertions cannot disappear. An incomplete
campaign is rejected even if some earlier conditions have results.

The one-second controller-watchdog negative test uses a deliberately oversized
RTT trial. Its owned children are stopped and no completed-trial JSON is
produced. No generic process-name kill is used. Failed and incomplete outputs
are preserved. The optional standalone cache control is iteration bounded but
currently does not enforce the IPC controller's parsed timeout; unattended
hostile-scheduling use needs an external owned-process watchdog. That optional
limit is explicit, not a false completed100M result.

Early GCC/Clang warning failures were fixed before collecting measured data;
logs are retained. The offline replay outer-timeout record is also retained.
No implementation bug or cause is invented from an incomplete tool invocation.

---

## 7. Reproduction and artifact layout

Start with `docs/BENCHMARKS.md`. A native Mac example is:

```sh
make CC=clang BUILD=build/apple benchmarks check-bench-tools
python3 tools/run_benchmarks.py --build build/apple \
  --count 100000000 --warmup 1000000 --trials 1 --out apple-turn07
python3 tools/verify_results.py apple-turn07 --analyzer build/apple/bench_analyze
```

A full repeated campaign adds `--trials 5`. Native output directories are created exclusively. Record
the actual SDK/compiler and machine identity alongside the supplied provenance.
An M4-specific build is documented separately; unsupported flags fail instead
of silently disappearing. No Linux CPU list should be passed as Darwin pinning.

The full raw Linux evidence archive extracts to `linux-100M/`. Replay it with:

```sh
python3 tools/verify_results.py linux-100M --analyzer build/native/bench_analyze
```

The small source bundle contains the requested C files, all core sources,
Makefile, replay/collection scripts, new functional evidence, inherited reference
specifications, large-run summaries, and manifests. The separately supplied
large evidence archive contains every100M raw trace/bitmap and its run manifest.
The outer evidence archive has its own literal SHA256 sidecar. No compiled
executables, sanitizer runtime, proprietary SDK, or unrelated system assets are shipped.

`SOURCE_MANIFEST.sha256` covers source/build/test/tool inputs. `MANIFEST.sha256`
covers every file actually included in the code bundle except itself. Historical
Turn6 manifests remain explicitly named under `docs/history/`. New measured
binary identities and source matches are recorded under `evidence/turn07/`.
A separate deposit receipt records actual Drive IDs and raw readback checks;
this immutable report does not predict a successful upload.

### 7.1 Raw storage and replay requirements

Each100M RTT profile has800,000,000 bytes of entry ticks,800,000,000 bytes of
RTT deltas and16,000,000 bytes of calibration. The measured origin preallocates
that memory; libc sorting may need an additional800,000,000 bytes after the
measurement. Throughput bitmaps use12,500,000 bytes per consumer plus a saved
union. The default sweep retains about3.5GB uncompressed per repetition. Disk,
RAM and wrapper I/O are audit resources, not hidden inside a544KiB ring claim.

Replay is bounded-memory but can be substantial. Its C analyzer sorts one
100M duration array and streams starts; the Python bitmap merge works in chunks.
No sampling, lossy histogram or counts-only substitute is used when resources
run short. Missing, truncated or inconsistent raw data fails verification.

---

## 8. Remaining gate and interpretation

Turn7 supplies a runnable benchmark extension, a documented Darwin correction,
actual large local finite observations, raw replay, and tools for real hardware
profiling. It does **not** establish native Apple performance, P-core selection,
actual P/E interconnect cost, the original one-way15/50ns goals, an open-loop
one-microsecond service budget, or the full five-repeat inferential qualification.
The new report does not upgrade lab-reported100k correctness receipts into
unperformed100M latency data.

No result justifies changing the ABI, weakening NCQ's SC metadata, introducing
raw-pointer timeout reclamation, or stripping public checks from the operation
being timed. Actual native Apple traces from this exact source are the next
input for hardware tuning and any production-performance verdict. The current
Linux observations are valid within their stated metric, workload, binary and
shared-host scope—not a substitute Apple result.

---

## 9. Sources and evidence scope

[P4] Preserved Turn4 specification, byte identity in the report header. Governs
layout, ownership, lifecycle and counter ceilings.

[P5] Preserved Turn5 plan, byte identity in the report header. Governs distinction
between one-way, RTT, validated goodput, counter diagnostics and qualification.

[P6] Preserved Turn6 report and verified source archive. Historical Linux/core
and queue-only cross-lowering evidence is not newly executed Apple evidence.

[U7] User's Turn7 commissioning message. Reports the Darwin EINVAL discovery
and native Apple correctness/sanitizer/assembly outcomes. No raw corresponding
lab logs or revised source hash accompanied this turn; that boundary is retained.

[R1] Apple XNU fchmod/non-vnode path, examined2026-09-23:
https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/bsd/vfs/vfs_syscalls.c
https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/bsd/kern/kern_descrip.c
Supports the descriptor-type distinction behind the Darwin patch; not every
future kernel build or a native test of the new guarded branch.

[R2] Apple Mach absolute-time guidance:
https://developer.apple.com/library/archive/qa/qa1398/_index.html
Supports timebase conversion; supplies no actual M4 timer frequency or overhead.

[R3] Linux clock and vDSO documentation:
https://man7.org/linux/man-pages/man3/clock_gettime.3.html
https://man7.org/linux/man-pages/man7/vdso.7.html
Supports raw monotonic clock semantics and possible userspace servicing. Actual
local timing/calibration is in the raw evidence, not taken from a manual.

[R4] Linux affinity interface:
https://man7.org/linux/man-pages/man2/sched_setaffinity.2.html
Supports CPU-mask application/readback. Does not supply exclusive CPU ownership.

[R5] Apple affinity contracts:
https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/osfmk/mach/thread_policy.h
https://developer.apple.com/library/archive/releasenotes/Performance/RN-AffinityAPI/
The current header identifies an experimental task-local cache-placement hint;
archived notes provide historical API context. Neither is a P-core-number API.

[R6] Apple CPU Counters guidance:
https://developer.apple.com/videos/play/wwdc2025/308/
Supports a separate profiling lane and proper interpretation of sample/event
scope, not a completed measurement or universal public PMU event-ID table.

[R7] Linux perf-event interface:
https://man7.org/linux/man-pages/man2/perf_event_open.2.html
Supports event scope, access controls, enabled/running time and multiplexing.
No event results were collected in this environment.

[R8] Clang sanitizer documentation:
https://clang.llvm.org/docs/ThreadSanitizer.html
https://clang.llvm.org/docs/AddressSanitizer.html
Used for separate instrumented lanes and limits. Actual newly run test outcomes
are provided in this bundle; a clean thread diagnostic is not an IPC certificate.
