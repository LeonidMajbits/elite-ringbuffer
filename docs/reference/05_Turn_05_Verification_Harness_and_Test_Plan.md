# Turn 05 — Verification Harness and Test Plan
## ELITEIPC / LE128-V1 / SPSC and NCQ-SC64

**Project:** Leonid Majbits / Gemini Operator Lab, paired with Leon  
**Date:** 2026-09-23  
**Stage:** 5 of 8 — final planning gate before separately authorized implementation  
**Status:** Normative verification specification. No queue implementation, executable model, harness, compiler probe, sanitizer run, fault injection, or hardware benchmark has been produced in this turn.  
**Authoritative predecessor:** `04_Turn_04_C11_ABI_and_Assembly_Specification.md`, 93,610 bytes, literal SHA-256 `6aaa3d6706c29096cfab56ea1fe569afa561c4cba7caea26ebd3c09cc9812386`. Mounted bytes verified in this turn.  
**Governing earlier records:** [P3], 81,776 bytes, SHA-256 `7ad5fbd43134e7d3419c5a1f76d030e25d051510dbc6f20ce173b3d0eccff605`; [P2], 91,463 bytes, SHA-256 `9bb8b971b65f7da80bd13053e8027aaa6ede5ae9ea19da8262e24282e16a6977`. Both mounted byte identities verified.  
**Canonical-SHA256:** `6856610f526f358d8c25755c79819cf57beb445782e0dbd0401ad4eb6f2ee738`  
**Hash convention:** Replace only the 64 hexadecimal characters in the preceding field with 64 ASCII zeros before computing the embedded hash. The adjacent `.md.sha256` authenticates the literal finalized file. See §17.  
**Destination:** Deployment Research / Elite_Systems_LockFree_RingBuffer  
**Evidence convention:** [P4] is the frozen ABI; [P3] supplies the governing kill policies; [P2] supplies the abstract queue and ownership proofs. [R01]–[R16] are externally verified primary sources listed in §18. Numerical budgets, test populations, trace formats, and statistical decision rules introduced here are proposed project policy, not prior measurements. Source-derived facts and new deductions are distinguished.

---

## 0. Final planning verdict and scope reconciliation

**Approve implementation of the frozen reference for verification, not promotion to production or certification of the requested latency.** The implementation must make the following three claims independently falsifiable: correctness of the ownership protocol; useful performance on a nominated native deployment; and safe managed recovery with explicitly uncertain delivery outcomes.

The new targets are **p50 < 15 ns** and **p99 < 50 ns** for the defined native, uncontended, one-message handoff. They are additional, stricter experiments, not deductions from alignment or instruction names. The governing **p99.9 ≤ 1,000 ns** sub-saturation service policy remains in force. The 16P/16C saturation test is not substituted for either latency experiment. [P3, §9; P4, §15]

| Request or acceptance premise | Required interpretation |
|---|---|
| 100M messages, zero loss | Exactly 100,000,000 distinct measured messages, each delivered once with all required bytes checked, in each nominated failure-free run; not only matching counts/XOR |
| 16 producers and 16 consumers on 16 cores | 32 endpoint workers, a worker/core ratio of 2 before controller/profiler overhead; not 16 endpoints and not 32 simultaneously executing cores |
| Strict LDAR/STLR | Preserve the frozen source orders; eligible non-SC acquires may lower to LDAPR. NCQ SC metadata retains its stronger complete mapping. No new mnemonic-only restriction |
| `/dev/shm` format | LE128-V1 is a POSIX-shm format; `/dev/shm` is the Linux adapter's common namespace, not Darwin's pathname |
| Consumer advances after epoch timeout | Timeout can trigger suspicion, finite wait recheck, or managed retirement. It does not authorize slot theft, fabricated reclamation, or skipping an unpublished payload |
| Log corrupted slot after producer death | Record DEAD/OVERDUE/OUTCOME_UNCERTAIN as supported. Call a record corrupt only after an authorized integrity observation or an explicit corruption experiment |
| Continuous monotonic stream after recovery | Native dequeue order is per-generation publication order, not consumer completion order. A management audit can have monotonic event numbers with GAP/UNKNOWN records; there is no gap-free cross-generation delivery theorem |
| TSan clean means IPC correct | A sanitizer lane has a declared process/address scope. Clean same-process instrumentation is not an interprocess or aliasing proof |
| 128-byte alignment proves speed | Alignment fixes byte geometry under admitted hardware assumptions. Actual timing, compiler lowering, and scheduling remain empirical gates |

The requested timeout-steal recovery scenario is included as a **mandatory negative test**. A system that performs it fails this plan, even if it continues printing increasing message IDs. That is the direct consequence of [P4, §§9.6, 11, 13], not a new restriction imposed after benchmarking.

### 0.1 What is frozen versus newly specified

No LE128-V1 byte, shared member, memory order, wait mode, status code, or ownership LP is changed. The new material is the verification environment around that ABI: message workloads, private evidence storage, instrumentation lanes, timer qualification, test bounds, exact compiler profiles, and acceptance rules. A timing label never authorizes removing public handle checks, lifecycle acquire loads, epoch validation, or required payload reads.

[P4]'s 10,000,000-ns maximum requested wait slice governs PARKABLE_SPSC. [P3]'s earlier proposed 1-ms slice is not silently substituted. The four-object/two-quarantine budget is unchanged. No automatic authority takeover or MPMC parking is added.

---

## 1. Verification architecture: independent lanes, not one overloaded benchmark

The future harness has an application-owned coordinator, endpoint workers, a private evidence recorder, and offline oracles. These are verification roles, not a new production daemon. The coordinator grants already constructed objects, controls test starts/faults, accounts for every child/endpoint, and enforces termination/resource budgets. It does not serialize healthy data-plane calls or supply the payload through a pipe/socket.

| Lane | Subject | Instrumentation | Permitted conclusion |
|---|---|---|---|
| V0 — artifact/ABI admission | Exact sizes, offsets, types, constants, CRC, range arithmetic, supported atomic tuple | Compile assertions and stable parser fixtures after Turn 6 | Named binary realizes or rejects the specified format |
| V1 — finite model | SPSC RA protocol, NCQ SC queues plus pool, lifetime and wait models | Explicit states/reads-from/LPs; no real clock | No counterexample within enumerated bounds, or a concrete witness |
| V2 — controlled concurrency | Named stale-observation, help, and lifecycle interleavings | Test-only cut points and local histories | Particular adversarial histories have required outcomes |
| V3 — natural native IPC | Separately mapped processes, real compiler/OS/cores | No scheduling hooks or extra synchronization in the transfer path | Finite-run correctness evidence for the actual binary |
| V4 — sanitizers | Core threads in one address mapping; local lifecycle/parser/FFI code | TSan separately from ASan/UBSan | Findings within the tool's observed scope |
| V5 — validated 100M throughput | Failure-free native full message lifecycles | Exact membership bitmap and full payload checks; no per-message clock | Verified-message goodput, with verification cost included |
| V6 — direct latency | Same public ABI and declared payload; exact start/end events | Ordered timestamps, private sidecars, no fault hooks | Instrumented per-message elapsed distribution with uncertainty |
| V7 — counters | Repeated native workload | Instruments CPU Counters or admitted Linux perf events | Work/coherence/scheduling diagnostics; not per-message timestamps |
| V8 — chaos/recovery | Owned child failure, false suspicion, retirement/cutover | External fault controller and managed lifecycle evidence | Safety, outcome classification, and separately measured restoration |

Every result names its lane. A V2 scheduler barrier can add synchronization that hides a weak-memory defect; therefore it cannot replace V1 or V3. A sanitizer, profiler, or hook-enabled binary cannot establish V6 production-speed claims. Conversely a fast V6 run cannot discharge V0–V4 and V8.

### 1.1 Production and verification dependency boundary

The native core remains C11 plus admitted OS facilities. Compiler sanitizers, an optional formal checker, Instruments/perf, and offline analysis are development tools, not linked production frameworks. An optional CFFI package belongs only to the binding lane. The baseline oracles and process driver can be implemented without a third-party runtime; adopting a checker does not require vendoring it into the core.

### 1.2 Result statuses

Use NOT_RUN, PASS_WITHIN_SCOPE, FAIL_WITNESS, FAIL_SERVICE, INCONCLUSIVE, UNSUPPORTED_TARGET, and HARNESS_INVALID. Do not map a timeout, unsupported counter, malformed trace, missing child, or incomplete exploration to PASS. One valid safety witness overrides any number of successful runs. A failed harness is repaired and rerun with the old record retained.

---

## 2. Machine, topology, binary, and resource admission

Record machine model and CPU identity, physical/logical CPU counts, P/E counts and available topology evidence, OS/kernel build, native architecture, memory size, actual page size, cache information actually exposed, power mode, thermal/pressure observations, compiler executable hash/version, target triple, SDK/sysroot, deployment target, linker, complete flags, library/helper dependencies, executable hash, and whether debugging/profiling/sanitizers were active.

Apple measurements must be native ARM64, not an x86 process running under translation. A 16-core product claim requires evidence of the nominated 16-core host configuration. If the available host has a different core count, execute a separately labeled condition and leave the specified 16-core cell unfulfilled. A CPU option cannot turn one physical model into another.

### 2.1 Worker and process topology

The requested stress condition uses P=16, C=16, K=32, N=1024, B=64. All records are single-owner, one token maximum, with the roles fixed by [P4]. On 16 eligible cores, the nominal worker oversubscription is 32/16=2. The coordinator and OS also need CPU service; they are not omitted from resource reports.

Mandatory execution arrangements are: 32 owned child processes with one endpoint each; two owned processes with 16 producer threads in one and 16 consumer threads in the other; and one-process/32-thread core verification. The first is the process-isolated IPC stress reference. The second exercises multithreaded process death as a different fault domain. The third is not presented as IPC performance and is the primary TSan arrangement. Launch workers through a managed spawn/exec sequence before distributing grants; do not accidentally inherit an exposed mapping into an unregistered child. In particular, a sanitizer-thread experiment is not implemented by forking an already multithreaded instrumented process and continuing arbitrary child execution.

The TSan core adapter shares **one virtual mapping** among its test threads. This is a verification adapter, not an assertion that independent public attachments already share a mapping lifetime. Native attach/detach tests use [P4]'s actual independently owned mappings. Distinct virtual aliases and distinct processes are exercised in V3 with the protocol oracles, not collapsed into the TSan claim.

### 2.2 Placement evidence

Classify placement as ENFORCED, OBSERVED, REQUESTED_ONLY, or UNKNOWN. Linux affinity must be read back for every worker and checked against allowed cpusets. macOS QoS or affinity-set requests are not labeled hard P-core pinning. Same-cluster, cross-cluster, P/E, and E/E claims require sufficient observed/enforced evidence; otherwise label the run mixed/unknown. Preserve migration and scheduling disturbances in primary latency data. [P3, §6.4]

The default measured cohort requests equal application-appropriate QoS for producers and consumers; the mixed-QoS lane separately requests background producers against interactive consumers and the reverse. The coordinator must remain schedulable, but assigning every worker the highest QoS is not an assumed solution. No scheduler deadline is inferred from a class name.

### 2.3 Frozen geometry and memory budgets

LE128-V1 uses A=128 and Q=16384. For N=1024, B=64, K=32, the participant records occupy [16384,24576); the first queue array still starts at 32768. QF, QR, descriptors, and payloads occupy the same subsequent ranges as the K=16 worked example. Total NCQ backing is **557,056 bytes (544 KiB)**. SPSC K=2 uses **294,912 bytes (288 KiB)**. These are independently checked arithmetic, not target allocations. [P4, §6]

The requested 32 workers do not violate K≤N. N=2 with K=32 does and is an admission-rejection fixture, never a valid concurrency counterexample. At most four cohort backing objects and two quarantines remain charged even during failed creation; private test evidence and profiler memory are separately capped, not hidden in that production bound.

Prefault and touch admitted pages before the measured epoch, using only initializer/owner-authorized accesses. Do not scan live mutable descriptors from the coordinator. Do not require HugeTLB or the rejected Darwin 2-MiB request. Record failed residency requests and actual faults; residency preparation is not a guarantee that all later faults or scheduler delays vanish.

### 2.4 Primitive and alias admission probe

Before full-ring stress, the later test suite constructs dedicated AU32/AU64 test objects with the admitted initializer and maps the same backing at independently chosen, recorded virtual addresses. These are verification fixtures, not extra fields in LE128-V1. One writer alternates two complementary bit patterns for 10,000,000 updates while an observer checks every acquired value belongs to the initialized/authorized set. Repeat for each required width and admitted placement class. A third participant checks contended strong CAS semantics in a separately specified scalar test. No deliberately misaligned atomic is executed through a purportedly valid path.

The fixture establishes finite alias/primitive evidence and can catch some torn or incorrectly mapped observations. It does not prove that all other bit patterns or histories are safe. Combine it with actual width/alignment assertions, compiler lowering, helper inspection, and the address-free platform contract. A lock-free query alone is insufficient. Deliberately incorrect per-process-lock or mixed-width implementations, if used as detector controls, remain isolated negative fixtures, never a production fallback.

---

## 3. Exact 100,000,000-message zero-loss throughput protocol

### 3.1 Mandatory profiles and repetitions

Execute V5 independently for SPSC 1P/1C and NCQ 16P/16C, in POLL_ONLY, checksum NONE, N=1024, valid length B=64, direct producer construction and in-place consumer reads. Each profile requires **five fresh managed generations**, with exactly **100,000,000 measured messages per generation**. Thus qualification is five separate 100M trials per profile, not 100M divided among five restarts. CRC64, 8-byte, 256-byte, and 4096-byte workloads are separately labeled extensions, never mixed into the primary rate.

The measured artifact is verified-message **goodput**, not only the number of successful enqueue returns. Allocation, construction, grant distribution, page preparation, and warmup are outside the measured epoch. Payload writes, public reserve/commit/borrow/release, required checks, local duplicate checks, and evidence bitmap updates are inside its work budget.

### 3.2 Unique IDs and exact producer quotas

Let M=100,000,000 and let P divide M for the mandatory configurations. Producer p, 0≤p<P, owns IDs:

\[
m=p(M/P)+s,\qquad 0\le s<M/P.
\]

For P=16, each producer publishes exactly **6,250,000** measured IDs. For SPSC, its producer owns all M. A retry of an unavailable reserve retains the same next ID; it does not create another offer or increment the successful publication count. The producer advances its local sequence only after a known PUBLISHED outcome. An uncertain result invalidates the failure-free trial rather than triggering a blind resend.

The tuple (run/session identity, m) is the test identity. `message_id=m` is legal even for zero. Assigned numeric IDs do **not** impose global publication order between producers. A higher-numbered producer may legitimately publish first.

### 3.3 The complete 64-byte payload contract

Use eight little-endian U64 test words. Word 0 is m; word 1 is the 64-bit complement of m; word 2 is p; word 3 is s; word 4 is a nonzero run seed; word 5 is a 17-bit rotate-left of m XOR that seed; word 6 is the complement of word 5; word 7 is m XOR a fixed per-run 64-bit pattern. Specify the pattern and seed in the run manifest before execution. All bitwise operations are on mathematical 64-bit bit strings, not signed arithmetic.

The producer writes all eight words directly into its valid lease; it never starts from an intermediate message buffer. It supplies the fixed test message type, length 64, and ID m at commit. The consumer checks all eight words against the independently known run schema while it owns the borrow; it also checks the API-reported length/type/ID and the admitted descriptor-generation contract. It must not simply compare a checksum that could hide a logical duplicate or skip actual bytes.

Input seeds and received values remain opaque to whole-program constant propagation. The later disassembly audit must confirm actual mapped loads, comparisons, and a non-eliminable error path. `-DNDEBUG` must not delete the oracle or public validation. Ordinary payload memory does not become volatile synchronization.

### 3.4 Exact membership oracle: no probabilistic zero-loss certificate

Each consumer owns a **private M-bit bitmap**, initially zero. On a valid received ID, it rejects an out-of-range value and rejects a bit already set in its own bitmap, then sets that bit. A duplicate delivered to different consumers is detected offline by pairwise-disjoint merge. No shared per-message bitmap or global success counter is introduced into the ring's hot controls.

Each bitmap uses M/8 = **12,500,000 bytes**. Sixteen bitmaps consume **200,000,000 bytes**, approximately 190.735 MiB, excluding alignment and other local records. This is verification overhead; it is not the ring's 544-KiB footprint. All bitmaps are prepared before the epoch and saved after it. Their random/cache access cost remains part of validated goodput.

After all endpoints have stopped data access, merge wordwise: require no overlap between the current union and the next bitmap, then form their union. Require the final union to contain every bit in [0,M), each consumer's checked receive count to equal its bitmap population, and the total to be M. Also require every producer's successful publication quota and every consumer's successful release accounting to reconcile.

**Finite-run proof.** Each accepted delivery names one element of [0,M). Local duplicate checks and pairwise-disjointness ensure no element occurs twice. A full union ensures each expected element occurs at least once. Therefore the observed accepted-delivery multiset equals [0,M), exactly once. This proves the trial's membership property, not all future executions and not survival of crashes.

Counts alone, a sum of IDs, an XOR, or a hash alone are insufficient substitutes: duplicate/loss combinations can preserve them. Checksums authenticate saved evidence against later byte damage; they do not replace the exact membership oracle. Consumer receipt and completed token return are separately checked.

### 3.5 Setup, warmup, timing, and shutdown sequence

1. Admit the exact binary/machine tuple. The authority allocates a fresh object, registers every potential holder, fully constructs it, and issues one-shot grants. Endpoints attach and validate the immutable prefix and CRC.
2. Prepare private bitmaps/evidence. Run **1,000,000 warmup messages in this same generation**, using a disjoint warmup identity range and seed. Drain and return every warmup token. Stop at a cooperative between-call barrier without detaching. Do not reset any shared cursor, epoch, entry, admission count, or grant record. Clear only private measured-run accounting.
3. Record the actual quiescent initial cursors and participant states through the authorized management snapshot. The measured counter deltas, not assumed zero counters, determine later checks.
4. Publish one future start time t0 in the admitted common monotonic domain, with **100 ms preparation lead**. This is a scheduling setup choice, not a deadline promise. Each worker records actual first entry; none starts before t0. Start skew counts in cohort goodput rather than being subtracted.
5. Producers execute their exact quotas. No per-message wall-clock read, syscall, allocation, printing, file I/O, or controller handshake is in V5. Native public lifecycle checks and validation remain. At most one outstanding token per endpoint is enforced.
6. Each producer reports completion once, after its last known publication call. After all producer reports, the coordinator issues DRAIN: no future publication is allowed by these producers. Consumers keep consuming/releasing and finish after observing no ready data with no local borrow/call outstanding. A consumer ending after that condition does not imply that a different consumer has finished its already-claimed record; wait for all endpoint acknowledgments.
7. Each consumer takes one ordered endpoint timestamp after receiving DRAIN, finishing its final release, observing the ready queue empty, and ending all local data calls. This is its **drain-quiescent timestamp**, not a retroactively known last-message read timestamp. No per-message clock is required. The timestamp precedes its final completion notification and any wait for other consumers; the preceding drain-control delay and final empty observation are honestly included.
8. Reconcile all bitmaps, publication/release counts, immutable CRC and zero reserved ranges under a stable snapshot, and quiescent queue membership. For NCQ require QR empty, all N unique blocks in QF's live interval, no borrowed/transfer-owned token, and every descriptor EMPTY. Historic array words outside live intervals are not extra tokens. For SPSC require P=C and no live lease. Then detach by the frozen protocol.

The primary rate is:

\[
G_{100M}=\frac{M}{\max_c(t_{c,\mathrm{drain\ quiescent}})-t_0}.
\]

This is **validated drained-cohort goodput**: it includes in-run validation and local drain/control-tail work, but not the final coordinator notification/barrier or offline bitmap merge/export. Also report each producer's end-of-quota timestamp, start skew, elapsed CPU time, faults, and completion imbalance. If exact last-message read/release endpoints are desired, obtain them in a separately clock-instrumented run; a consumer cannot know retrospectively which message was its last without additional measurement or a terminal marker. A lighter diagnostic can omit the bitmap only under a different label and cannot borrow this run's zero-loss proof.

### 3.6 Failure and timeout accounting

Use a **600-second whole-trial watchdog** after t0. Expiry preserves incomplete work and reports INCOMPLETE/FAIL_SERVICE or HARNESS_INVALID according to the trace; it does not assert a finite loss count without evidence. The coordinator stops admission, resolves/fences its owned workers, and preserves evidence safely. Do not divide M by a partial runtime and call it completed throughput.

Any process crash, integrity mismatch, uncertain publication, missing artifact, bitmap conflict, failed release, or unexplained counter reconciliation invalidates the failure-free zero-loss result. Chaos belongs to V8 and has a different denominator and allowed outcomes. Passing five finite runs does not prove a zero long-run failure probability.

---

## 4. Latency protocol: preserve the operation being timed

### 4.1 Named intervals

For each offered message m record, as applicable:

| Time | Definition |
|---|---|
| t_offer(m) | Preregistered intended application offer time; never moved forward after a stall |
| t_entry(m) | Ordered timestamp immediately before the first public write_reserve attempt for that offer |
| t_read(m) | Ordered timestamp after the winning consumer finishes all required payload/metadata reads and checks, before read_release |
| t_return(m) | Completion of the consumer's release call, separately instrumented when requested |

Primary native latency is t_read−t_entry; offered-service latency is t_read−t_offer. A retry chain remains one request. Consumer-return and request/response latency are separately named. The primary interval includes direct 64-byte construction and every required public-ABI operation, including lifecycle observation; timing only a cursor store is not this experiment. [P4, §§8, 15.2]

Timestamps are saved in **process-private sidecars**, joined offline by (session,m). Start time is retained in local operation state and may be written to its sidecar after publication; the publisher must not write it into transferred slot storage. Consumer evidence uses values captured under ownership and never rereads the descriptor after release. Clock and trace writes can perturb later messages, and that effect is recorded rather than claimed absent. Evidence is preallocated, never grown on the timed path. A concrete 10M-offer plan reserves an indexed eight-byte entry-time array across producers (80,000,000 bytes total) and, per consumer, up to 10M 24-byte local records containing ID, end tick, and status/flags (240,000,000 bytes). At 16 consumers this is 3,920,000,000 bytes of worst-case raw timestamp storage before auxiliary indexes. A 4-GiB timestamp-storage cap admits those primary arrays, with auxiliary/bitmap/runtime budgets recorded separately. Each consumer could receive every message, so allocating only one-sixteenth of that capacity without an overflow policy is invalid. Insufficient RAM or log capacity blocks the declared run; no timed file-I/O spill, silent trace dropping, or post-hoc sampling is substituted.

### 4.2 L0 — uncontended, one message outstanding

Run SPSC 1P/1C and NCQ 1P/1C separately on warmed native mappings, POLL_ONLY, checksum NONE, N=1024, B=64. Keep exactly one message awaiting completion. A test-only completion acknowledgment after consumer release controls the next offer; it is outside that message's t_entry→t_read interval but its resource and cache effects remain part of the experiment. It is not a production payload path or an alleged free round trip.

Five independent restarts each collect at least **10,000,000 directly timed offers**. New goals for each nominated uncontended profile are:

\[
q_{0.50}(T_{native})<15\ \mathrm{ns},\qquad q_{0.99}(T_{native})<50\ \mathrm{ns}.
\]

Do not silently scope these thresholds to eight bytes, same-thread loopback, a batched average, an internal function stripped of ABI checks, or one selected fastest core pair. Nominated topology labels are fixed before qualification. Failure on an intended supported topology is reported, not removed after inspecting its tail. Full 16P/16C oversubscription is not described as uncontended.

### 4.3 L1 — open-loop service distribution

Retain [P3]'s common load at 0.70 times the lower independently calibrated sustainable rate of semantically comparable candidates on the same topology/resources. A separate absolute application rate, when supplied, is an additional requirement; without one the report cannot claim a commercial capacity target is met.

Freeze the rate from pilot data before qualification. The offered schedule is a deterministic evenly spaced cohort schedule with producer offsets spread across one producer period. A predeclared burst variation uses the same total offered count and mean rate but groups 32 consecutive offers at each burst start. Both retain original scheduled times, including when the generator cannot run on schedule.

Each producer holds at most one native token and a local queue of still-unattempted scheduled offer IDs. The arrival schedule can be generated from ID/rate arithmetic rather than allocating an unbounded backlog. Its finite offer set and whole-run budget are known. If the generator falls behind, do not relabel the postponed offer as a new on-time arrival. All missing or deadline-censored offers remain failures of the offered-service budget.

Five independent restarts, at least 10,000,000 offers per condition, measure both intervals. The retained policy is **p99.9 ≤ 1,000 ns** and the corresponding >1,000-ns/missing-completion fraction ≤0.001. The strict new 15/50-ns goals are also reported wherever requested, but their failure is not concealed by passing the older one-microsecond budget.

### 4.4 Calibration without reducing the requirement to a trivial load

Pilot candidate rates use five 5-second steps per trial rate, fixed payload work, no growing backlog, and a final complete drain. Search rates on a predeclared doubling grid then refine the final feasible bracket to 5% relative width. Use the lowest sustainable estimate across three independent pilots for common-load selection. A timer/profiler change or a different checking workload requires recalibration. Pilot observations are excluded from qualification quantiles.

Report the sustainable rate itself and the fixed-rate failures. A slow queue does not earn a capacity pass by choosing 70% of its own diminished speed. Compare only services with compatible ordering, routing, copy, CPU, and borrower-lifetime semantics; include any SPSC multiplexer and downstream hop.

### 4.5 Additional latency conditions

Report 8-, 64-, 256-, and 4096-byte payloads; N=32,1024,65536 where K≤N; low and high occupancy; 1P/1C, 2P/2C, 4P/4C, 8P/8C, 16P/1C, and 16P/16C. Larger payloads and CRC64 are not required to satisfy an unstated universal 15-ns budget. SPSC parkable-but-awake overhead, actually sleeping wake-up, and Python/native-boundary overhead are separate series. There is no MPMC parked series until that protocol is admitted.

---

## 5. Clock metrology and counter collection

### 5.1 Three different instruments

A monotonic elapsed-time source measures duration. A core PMC records events attributable to execution and may be gated, reset, multiplexed, or migrated. A cycle count is not automatically a wall-time clock. A throughput rate cannot yield latency percentiles by inversion.

Apple's archived API guidance requires conversion of mach absolute ticks through the supplied timebase. Apple's current CPU Counters guidance describes sampled workload profiling; XNU distinguishes core and uncore counters and counting versus sampling. These sources support separating the roles, not an assumed per-message PMC timestamp facility. [R01–R03]

### 5.2 Apple Silicon timestamp adapter

Use mach_absolute_time and a once-queried mach_timebase_info ratio for the primary supported OS-clock path. Store raw 64-bit ticks plus the numerator/denominator in the manifest. Convert differences with overflow-checked rational arithmetic, retaining sub-nanosecond fractions or outward rounding for bounds. Do not multiply an arbitrary large absolute tick value in an overflowing U64. Do not assume ticks are nanoseconds or CPU cycles. [R01]

The admission probe after implementation must establish counter ordering relative to the timed loads/stores, effective update granularity, read overhead distribution, monotonicity, relevant cross-core comparability, and sleep/migration behavior. For the conservative ARM64 measurement adapter, specify compiler motion barriers and ISB ordering around the underlying clock-read boundary; validate the actual API implementation/disassembly and add a load-completion ordering barrier when its contract requires it. A slower conservatively ordered diagnostic is preferred to an unqualified fast timestamp. Timing barriers are harness operations, not additions to the queue's release/acquire protocol.

No precise current Apple binary sequence is claimed here. Arm documents both speculative counter reads and self-synchronized counter extensions, and cautions that advertised counter resolution and actual update rate can differ. A direct CNTVCT/CNTVCTSS/PMU-register adapter is a separately admitted option only if the OS permits it, its feature is present, its clock relation is established, and its ordering is audited; it is not inferred from `-mcpu=apple-m4`. [R04]

A hypothetical 24-MHz counter advances about 41.667 ns per tick; a zero-tick difference would not prove zero latency or a <15-ns median. A nominal 1-GHz scale also does not prove actual one-nanosecond updates. The actual host determines which case applies. If clock uncertainty straddles a threshold, report METROLOGY_INCONCLUSIVE rather than manufacturing fractional nanoseconds from a coarse trace.

### 5.3 x86-64 timestamp adapter

On an admitted x86 target, the baseline start boundary is an ordered LFENCE/RDTSC/LFENCE family with compiler barriers; the read-completion endpoint uses RDTSCP/LFENCE with compiler barriers. Record feature support and the IA32_TSC_AUX observation when used, and verify the OS meaning of that tag. These ordering families are described in the perf project's primary timing discussion. [R05]

They do not mean preceding stores have become globally visible. A separate metric requiring that property uses an appropriately stronger MFENCE-containing adapter and receives a different label. Neither adapter is a slot-revocation mechanism. Timer frequency comes from the admitted invariant-TSC/platform calibration, not the CPU's current turbo clock. Do not assume an Intel LFENCE condition applies identically to every AMD target without its corresponding admission evidence.

AUX equality at two observations does not exclude migration away and back in between. A pinning claim requires affinity evidence, and a cross-core subtraction requires compatible counter domains. Cross-socket/virtualized cases may be rejected for direct nanosecond measurement while still running correctness/throughput tests.

### 5.4 Calibration procedure and uncertainty intervals

Before and after each qualified run, take 1,000,000 back-to-back reads on every admitted worker placement class; retain raw deltas, negative/zero counts, modal/minimum nonzero increment, and overhead quantiles. Observed increments are evidence, not proof that no finer or coarser updates exist. Pair them with the clock-source contract. Acquire 10,000 two-way timestamp handshakes per participating domain pair before/after the run to test skew and drift; do not divide round-trip time by two and assume equal path delays.

For comparable-frequency clocks A and B with offset theta=B−A, a handshake with A-send a1, B-receive b2, B-send b3, A-receive a4 constrains:

\[
b_3-a_4\le\theta\le b_2-a_1,
\]

after accounting for timestamp uncertainty. Intersect only bounds valid under the declared drift model. Non-overlap is a failed calibration. A minimum round-trip or a fitted offset is not a universal bound by itself. An independently documented common counter may supply stronger domain evidence; identify that source rather than inferring it from a successful test.

For each elapsed observation d, retain a conservative interval [max(0,d−u),d+u], with u covering effective quantization, residual offset/drift, conversion rounding, and justified boundary-order uncertainty. Unbounded/unestablished ordering uncertainty invalidates direct latency certification. For uncensored observations the lower and upper order statistics bound the corresponding empirical quantile. Missing completions have unbounded upper endpoints and remain deadline misses.

For the new 15/50-ns qualification, predeclare a **5-ns maximum justified interval half-width u** in addition to requiring the final upper quantile bounds to lie below the targets. If this cannot be established, the run cannot certify those targets even if a displayed median is small. The old one-microsecond gate also requires intervals not straddling its threshold; it is not automatically passed by an unresolved clock.

Do not subtract a median clock overhead from every sample. Raw instrumented results are primary. An empty-boundary calibration and a 1-in-64 sampling sensitivity run may quantify perturbation, but neither reconstructs the uninstrumented tail or authorizes dividing a batch duration to obtain p99. The publication explicitly identifies instrumented latency; a claim about an uninstrumented deployment needs corroborating evidence, not an assumed universal additive correction.

### 5.5 Hardware counter collection: separate repeated runs

On Apple Silicon, use the installed Instruments **CPU Counters** facility with its documented preset modes. Record the installed Xcode/Instruments version, exact template/preset/export schema, process/CPU scope, interval, and privilege availability. Run counting/sampling only in V7 repetitions, not inside each sub-50-ns measured message. XNU's CPMU/UPMU distinction means uncore events are not naively attributed to one endpoint. No private kpc syscall or privileged register access becomes a mandatory production dependency. [R02, R03]

Request available retired-instruction, cycle, branch/misprediction, cache/load-store or bottleneck metrics, along with scheduler/context-switch observations. Unsupported event names are marked unavailable; do not invent universal M4 PMU event numbers. Count P/E classes separately where event definitions differ. Report CPU-time-attributed cycles and instructions per completed lifecycle; never subtract a producer's core cycle register from a consumer's different core register.

On Linux, use perf_event_open/perf with identified event groups, enable/running times, and scope. Report multiplexing and scaling; a group that did not run cannot report zero as its count. Repeat small nonmultiplexed groups when scaling uncertainty makes comparisons unreliable. Access restrictions are explicit capability gaps. Instructions, cycles, faults, context switches, and supported cache events are diagnostics rather than replacements for outcome logs. [R06]

Software RMW-attempt counters live in endpoint-private diagnostic state and count failures plus tail help. Instrumented counter runs must report their cost. Preserve [P3]'s combined thrash criterion: mean attempts >24 per completed lifecycle and 16P/16C goodput <90% of 8P/8C in at least three of five matched trials. A rising attempt count alone is not a proof of global livelock.

---

## 6. Statistical preregistration and decision rules

### 6.1 Exact empirical definition

For n offers, use the nearest-rank quantile: q_p is order statistic ceil(pn), with no interpolation between adjacent timer bins. Retain lower/upper timing intervals separately. Equality fails the strict 15-ns and 50-ns goals; equality meets the older ≤1,000-ns criterion. Report the complete offered count, delivered count, deadline-miss count, censored count, p50/p90/p99/p99.9/max, and per-endpoint counts. An infinite upper endpoint is not discarded before sorting.

Qualification uses five independent process-cohort restarts. The inherited rejection rule is a confirmed breach in at least **three of five** runs. Acceptance requires **all five** runs to meet each mandatory empirical objective and its uncertainty conditions. One or two failing/mixed runs block acceptance; they are not averaged away. Safety needs only one witness. The new strict objectives use this same repetition discipline, while remaining separately labeled from the older service gate. [P3, §9.4]

### 6.2 Dependence-aware interval construction

Per condition, freeze **20,000 circular moving-block bootstrap resamples**, separately within each run, for block lengths **1,000; 10,000; and 100,000 consecutive offer indices**. A run has at least 10,000,000 offers, giving at least 100 nonoverlapping longest blocks. A resample concatenates blocks until n entries are reached and truncates excess entries. Keep whole per-offer records, including missing outcomes and timing-interval endpoints. Do not IID-resample individual messages.

Bootstrap seeds are deterministically derived from the plan's literal SHA-256, condition ID, run index, block length, and resample index; save the exact generator/version and seeds after implementation. Compute quantiles from upper timing endpoints and deadline-miss fractions from the conservative classification. For the three main quantiles over five runs, use a one-sided percentile upper confidence level **1−0.05/15 = 0.9966666667** for each run/metric, a Bonferroni family allocation within that condition. Take the maximum upper endpoint across the three block-length analyses. The strict fast-path goals require this upper endpoint below their target, not merely a favorable point estimate.

For the legacy deadline-miss metric, report its block-bootstrap upper interval using the same per-comparison level as a corroborating requirement; it is not advertised as an additional unadjusted family-wide confidence guarantee. A fully joint claim including that metric must use alpha=0.05/20 and recompute all four metrics across five runs. The analysis record states which family was claimed.

These are **conditional inference procedures**, not distribution-free correctness proofs. They require an adequate approximately stationary, short-range dependent regime. Long runs of scheduler delay can violate that model. Inspect ten consecutive deciles, block miss-rate autocorrelation, thermal/clock changes, and sensitivity to block length. A zero-miss trace does not acquire a zero upper population failure probability merely because every bootstrap replicate is also zero. In that degenerate case report the finite-run result and refuse a population-tail certification without a separately justified stochastic model or additional evidence.

If decile changes or correlation beyond the longest block undermine the model, mark inferential acceptance INCONCLUSIVE. Retain empirical service failures; do not delete slow deciles. An amended longer-run/dependence plan must be committed before collecting new qualification data; old observations stay archived. This avoids claiming that an arbitrarily chosen bootstrap repairs nonstationarity.

### 6.3 Offered work and coordinated omission

The t_offer schedule is fixed before work. A producer that cannot attempt an offer until much later retains the earlier timestamp, even when its next successful native call is fast. Record both native and offered intervals. A deadline miss includes a missing completion by the deadline. Backpressure, retries, failed returns, and pending work are neither reset to a new request nor dropped from the denominator.

Closed-loop L0 answers a deliberately narrower one-outstanding question and is labeled as such. It does not establish open-loop tail behavior during saturation. V5 throughput likewise has no per-message latency distribution to invert.

---

## 7. Formal invariant register and evidence linkage

| ID | Invariant to preserve | Falsifying witness / required evidence |
|---|---|---|
| I01 | Every endpoint/lease is bound to exact session, role, grant and local instance | Old or foreign handle accesses mutable bytes before local rejection |
| I02 | All atomic objects are constructed before exposure and accessed at admitted width/alignment | Unconstructed gate read; hidden lock fallback; accepted misaligned member |
| I03 | SPSC 0≤P−C≤N | Counter trace or authorized state violating capacity |
| I04 | Payload writes happen-before authorized reads | Consumer gets old/mixed payload after valid claim; weak-memory model witness |
| I05 | Last borrow read happens-before next overwrite | New writer appears while previous read alias remains live |
| I06 | Each pool token belongs to exactly one live logical category | Two owners, duplicate QF/QR membership, unaccounted transfer treated as free |
| I07 | NCQ H≤U, 0≤U−H≤N, U−1≤T≤U | Tail ahead of publication; missed allowed H=T+1 state |
| I08 | Publication precedes tail advancement; entry CAS is enqueue LP | Tail skips uninstalled entry or receiver depends on stopped post-LP publisher |
| I09 | Exactly one head claim owns each ticket | Same-value false CAS success; two claims for one ticket |
| I10 | CAS failure discards all dependent observations | Refreshed expected with old desired/index replaces or duplicates another result |
| I11 | Post-LP publisher/releaser never touches transferred mutable block | Late logging, owner reset, checksum update after ownership moved |
| I12 | No live ticket/epoch/session/grant identity repeats | Overflow, in-place reset, recycled grant or stale pointer routed to successor |
| I13 | Phase is not queue membership or owner recovery evidence | COMMITTED scanner enqueues twice; EMPTY scanner steals unreturned token |
| I14 | Wait mutation RMW chain and expected-value enrollment are preserved | Ordinary live notifier/waiter leave satisfiable predicate stranded |
| I15 | Attach LP competes with close in the same gate word | New enrollment after retirement wins; lost concurrent increment |
| I16 | Retirement is not global quiescence or fencing | Reclaimed mapping while helper/alias/pending grant can access it |
| I17 | Successor has distinct physical storage and no live old-VA redirection | Resumed old pointer modifies successor bytes |
| I18 | At most four accounted objects and two quarantines | Uncounted abandoned staging; third quarantine; authority resets its quota |
| I19 | Public status and transfer outcome are distinct | Post-LP notification error reported as unsent and resent blindly |
| I20 | Test evidence preserves exact denominator and scope | Counts-only zero loss, sanitizer-as-IPC-proof, omitted slow/censored offers |

I03–I12 refine the accepted [P2] models through [P4], rather than asserting a newly machine-checked theorem. Quiescent state snapshots can inspect live membership; asynchronously scanning ordinary descriptors cannot. A model tracks ghost ownership at LPs. A native trace records locally captured observations and reconstructs histories offline; it does not add a production shared oracle counter.

### 7.1 Formal meaning of “zero ABA,” “zero tearing,” and “no holes”

No finite hardware stress run proves absence for all executions. Evidence has three layers: the no-repeat/alignment/ownership argument; exhaustive finite exploration with declared bounds; and attempted falsification on real binary/OS tuples. Report **zero observed anomalies in the stated run**, not an absolute theorem inferred from 100M successes.

For ABA, distinguish physical slot reuse from represented identity repetition. Successful stale expected reuse in the same represented ticket domain is forbidden; normal different generations at the same array index are required. For tearing, individually atomic controls and ordinary multi-line payloads have different obligations. A checksum alone cannot legalize a racy read. For publication holes, the key test is stopped producer before or after the actual QR entry LP, not a phase-word rename.

---

## 8. Finite exploration and weak-memory verification plan

### 8.1 Explicit bounds and state components

After authorization, create a dependency-free SC transition explorer for the reference and a separately reviewed weak-memory checker input for the SPSC/descriptor handoffs. No such programs are supplied now. GenMC is a suitable optional development checker for C/C++ memory-model exploration; record its exact revision, chosen model, frontend, and transformation assumptions. Its model capabilities do not automatically include our OS bootstrap, cross-process mapping identity, or timeouts. [R13]

Mandatory finite models are: N=2,K=2 with one producer and consumer; N=4,K=4 with 2P/2C; and N=4,K=3 with 2P/1C and its dual. Enumerate up to eight completed payload lifecycles per endpoint, a pause at every atomic and owner-field transition, aborts, one stopped participant, and every named stale-CAS history. N=8,K=4 extends reuse distance in a bounded nightly lane. K>N is a parser/admission negative case.

The state includes both queue arrays, head/tail, finite ticket/phase encodings, local saved expected/desired/index, ownership and held aliases, gate/count, one-shot participant states, pending grants/holder acknowledgments, and ghost LP events. Do not test only QR and ignore QF. Prune symmetries only after documenting preservation of roles, ownership, and identity. Record visited states, terminal states, explored transition/depth bounds, cutoffs, and a minimal witness. A run that hits a bound before completing the claimed state space is incomplete, not exhaustive.

### 8.2 Counter-ceiling refinement

Use an eight-bit mathematical ticket domain with N=4 and ceiling J=251, and a reduced six-bit generation domain with corresponding two-bit-phase packing and a nonwrapping ceiling. These are model parameters, **not LE128-V1 wire changes**. Prove that rejecting every transition whose checked successor exceeds the ceiling excludes wrap. Include stale expectations kept across multiple physical laps and abort-driven epoch increments.

For native production-width code, exercise pure arithmetic/validation routines at J−1,J,J+1 and G−1,G,G+1, plus coherent model-derived near-limit fixtures in an explicitly verification-only construction path. Such fixtures never claim to be normally initialized public sessions; [P4]'s initializer stays unchanged. Do not overwrite a live ring to force wrap. A separate targeted eight-bit ABA history permits at least 260 successful head advances while one old expectation is retained. This is distinct from the eight-lifecycle-per-endpoint exhaustive suite: that smaller bound cannot traverse a 256-value ticket domain. The reference must retire before the relevant ceiling; a deliberately weakened no-check mutant must expose repeated identity or a resulting ownership violation. Save the exact schedule and distinguish this targeted long trace from exhaustive exploration of all equally long traces.

### 8.3 Weak-memory obligations

Explore SPSC publication and recycling separately, then together. The required forbidden outcomes are a covered publication with earlier payload, and reuse before a retained reader finishes. Preserve ordinary non-atomic payload accesses and the exact release/acquire operations. Do not strengthen all atomics to SC to make the SPSC model pass.

For NCQ, keep all queue metadata SC and status transitions release/acquire; test the full token/descriptor composition. Instrumentation cannot turn COMMITTED into an authoritative ready-queue membership oracle. Separately explore the PARKABLE_SPSC RMW modification-order cases, same-value notifications, disarm/rearm, and close. The OS compare-and-wait is an explicitly specified abstract primitive, not an unmodeled magical wake.

The chosen checker may implement RC11 or another repaired/axiomatic language model. State the relation to the admitted C11 implementation and the particular patterns checked; do not call an RC11 result an unqualified proof of all ISO C11 programs. Hardware refinement still needs actual assembly and native stress.

### 8.4 Negative controls and liveness

Required mutants remove the producer release, remove reclamation ordering, allow payload read before winning head CAS, retain stale desired/index after a failed CAS, overwrite a competing current-cycle entry, advance tail before installation, write a descriptor after LP, replace same-value wait RMW with a skipped load, permit wrap, and recycle an unfenced timeout lease. Each relevant oracle must reject its mutant or the verification method is not accepted for that hazard.

Safety exploration and liveness arguments are distinct. Include finite-state nonterminal strongly connected components where the modeled scheduler supplies active steps but no operation completes; label fairness/primitive assumptions. Finite delays and a fixed depth bound alone cannot establish lock-freedom or disprove an infinite-execution theorem. Preserve [P3]'s system-wide progress versus individual starvation distinction.

---

## 9. Native concurrency stress and fuzzing matrix

The baseline reference uses strong CAS; do not inject arbitrary caller-visible spurious failures and call it the same algorithm. Weak-CAS experiments are named mutants/variants with their own progress assumptions. Natural LL/SC retries underneath a strong operation belong to the primitive audit.

| Dimension | Mandatory values or histories | Oracle |
|---|---|---|
| Participants | 1/1,2/2,4/4,8/8,16/1,16/16 P/C | Valid K, independent roles, per-endpoint completion/accounting |
| Capacity | 2 only K2; 4 only K≤4; 32,1024,65536 where valid | No full internal enqueue without outside token; truthful lack of capacity |
| Payload | 0 valid length,1,8,63,64,65,127,128,129,256,4096 within configured B | Exact length bounds; all valid bytes; no exposed padding |
| Occupancy | Empty, one ready, near full, all tokens held, alternating bursts | Empty-ready versus no-free-storage distinguished |
| Reuse | Millions of physical laps; delayed saved entry across several laps | Loser rereads; correct generation and ticket |
| Pauses | Before/after QF claim, phase, each payload chunk, QR install, tail help, QR claim, QF return | No partial delivery, no post-LP access, helpable tail |
| Mapping | Different process bases; independent aliases; guard pages around entire object | Relative offsets; no alias-based private atomic fallback |
| Topology | Available P/P, P/E,E/E,migration; equal/inverted QoS; oversubscription | Correctness unchanged; topology claim matches evidence |
| Lifecycle | Duplicate grant, JOINING crash, retire/enroll race, BUSY detach, pending-grant closure | Gate/count LP; no orphan subtraction or premature unmap |
| Waiting | SPSC awake/armed/rearm; notify-before-wait; spurious/interrupt/timeout; kill in wake gap | Recheck, lifetime and RMW chain; MPMC wait rejected |
| Checksum | NONE and CRC64, empty/check-vector/message mutation | Mode-specific behavior; integrity triggers retirement |

For each valid native combination in the core participant/capacity sweep, require five independent seeds and at least 1,000,000 completed lifecycles per seed before the large V5 trial. Run at least 100 further deterministic seeds for the small-capacity reuse tests. Record the actual cross-product used; do not imply every optional payload/topology combination ran when only a pairwise subset did. The required 16P/16C,N1024,B64 cell cannot be omitted as a pairwise-coverage optimization.

Fuzz delay choices come from a saved counter-based/deterministic generator with a recorded algorithm revision and seed. Values include no delay, short CPU work, scheduling yield, and controlled pauses with explicit release. The benchmark core remains unchanged; delays belong to the test caller or named hook build. A pathological seed remains in the corpus after reduction.

### 9.1 Linearizability and ordering oracle

In a test-only trace build, record native call/return intervals, successful entry-installation tickets and block IDs, successful head-claim tickets, observed empty events, and local operation outcomes. Capture necessary fields while owned; append local records after LP only from saved local values. No global trace RMW is inserted to invent the queue's total order.

Offline, pair QR publication/claim by logical ticket and require a legal FIFO history consistent with each call interval and program-order constraint. Completed-before-invoked precedence must be preserved. Operations interrupted before reporting their outcome remain pending; a history checker can assign an LP only when permitted by evidence and the model. Hardware wall-time timestamps alone do not establish simultaneous events' exact order.

Two consumers may complete ticket h+1 before h; that is not a FIFO violation. Numeric message IDs assigned by producer quota are not publication tickets. Per-producer completion logs split across consumers must be reconstructed by dequeue LP, not by merge arrival time at the coordinator. An oracle that insists global completion IDs increase would reject legal executions and is itself invalid.

### 9.2 Publication-hole witness tests

PH-1 pauses one producer after it owns a payload block but before QR publication. Healthy producers/consumers must complete at least **10,000 other valid message lifecycles** while that producer remains paused, with spare capacity. This is a concrete progress demonstration, not a nanosecond fairness theorem. Avoid retiring the object merely to fabricate progress in this particular pre-failure correctness test.

PH-2 pauses immediately after QR entry CAS and before tail help; another consumer must be able to claim that installed message, and another producer must be able to help the tail. PH-3 forces the last installed entry to be consumed while T still lags: H=T+1 is admitted. A caller that waits for the stopped publisher or asserts H≤T fails. After controlled resumption, the old publisher performs only permitted metadata/local completion, even if the payload has already been reused.

These controlled tests give deterministic interleaving coverage. They do not replace no-hook native runs because the hook channel can strengthen memory ordering.

### 9.3 Parser/ABI fuzzing without racing construction

Prepare stable byte inputs for the immutable-prefix decoder and canonical arithmetic validator. Cover wrong magic endianness, every version/profile/flag mismatch, every reserved byte, invalid CRC, correct CRC with semantically invalid geometry, overflow of each size expression, off_t/sign boundaries, N non-power-of-two, K>N, role mismatch, bad stride, payload length and mode errors, duplicate identity/grants, and unsupported wait mode.

Require **1,000,000 bounded inputs per qualified parser toolchain**, from a seed corpus containing all [P4] ABI-01…ABI-24 cases plus pairwise field mutations. Valid-seed mutation must sometimes recompute CRC so the parser reaches deep semantic checks; invalid-CRC-only fuzzing is insufficient. Cross-check CRC fixtures against fixed independent vectors and an independently obtained reference during verification; no new production library is implied.

A pure byte parser can read untrusted bytes. The full attach routine may access the gate only with an already constructed grant and admitted atomic tuple. Do not build a fuzzer that blindly casts random bytes to live atomics, then count undefined behavior as a product defect. Full-object fixtures construct typed atomics before controlled immutable mutation/exposure, and never mutate headers concurrently with legitimate readers. Guard pages surround the mapping, not inserted inside its frozen region layout.

---

## 10. Chaos and crash fault injection

### 10.1 Isolation and evidence separation

All signals target only explicitly registered, harness-owned child processes. No process-name search, broad kill pattern, or privileged host reset is permitted. The controller validates child incarnation, disallows unregistered descendants, and preserves its own object/holder ledger. Linux process-level handles and Darwin owned-child terminal wait status follow [P4]. Sending SIGKILL is not the same timestamp as observing terminal exit. [R14, R15]

A fault hook belongs to a test-only build and exposes a named cut point to the controller through a separate test channel. The data-plane implementation receives no synthetic successful cleanup on death. “Unobserved death” means the library/recovery actor has not yet been told the outcome, not that the experimenter lacks ground truth. Ground truth is retained privately by the controller and is never used to justify an unsafe data-plane read.

For no-hook randomized killing, record the last externally supported event and an interval of possible cut points. Do not label a random kill as “exactly after CAS” without an exact hook or equivalent evidence. Controlled hook runs and no-hook random runs remain separate evidence classes.

### 10.2 Mandatory cut-point catalogue

| ID | Victim cut | Required outcome |
|---|---|---|
| C01 | Before QF claim | No acquired token attributed to this attempt |
| C02 | After QF head CAS, before RESERVED/owner mirror | Orphan outcome remains uncertain; no guessed token return |
| C03 | After RESERVED, before first payload write | Unpublished data never delivered |
| C04 | Between each of the eight 64-bit payload writes | Partial payload never becomes a ready record |
| C05 | After payload writes, before COMMITTED | No phase-scanner publication |
| C06 | After COMMITTED, before QR insertion | COMMITTED alone is not publication |
| C07 | After QR entry CAS, before tail help | Published record can be consumed; tail is helpable |
| C08 | After tail help, before caller receives result | No missing-receipt resend; outcome may already be delivered |
| C09 | After QR head claim, before CONSUMED | Unknown consumer-held token not stolen |
| C10 | During read borrow / exported view | Old bytes retained until aliases end or full fencing |
| C11 | After external-effect simulation, before return | Effect/delivery ambiguity explicitly retained |
| C12 | After EMPTY, before QF insertion | EMPTY is not already-free membership |
| C13 | After QF insertion, before release returns | No descriptor cleanup by old owner on resumption |
| C14 | SPSC after P release, before notify RMW | Finite wait recheck, not guaranteed immediate wake |
| C15 | After wait RMW, before wake syscall | Same crash-gap handling; state change is not transactional wake |
| C16 | JOINING before/after admission increment | Pending holder retained, no guessed decrement |
| C17 | QUIESCENT before/after count decrement | No premature reclaim and no post-detach shared write |
| C18 | Builder before READY or grant exposure | Unpublished constructed state not guessed from name |
| C19 | Authority before/after successor distribution | No automatic quota reset or competing current generation |
| C20 | Prepared old QR CAS races retirement | Late old LP remains old; RETIRE_REQUESTED is not SEALED |

Each applicable exact cut requires **100 fresh-trial repetitions per target tuple and queue profile**. C04 has seven inter-word cuts and is counted as seven cases, not one. Record unsupported or inapplicable cases explicitly. Supplement with **1,000 no-hook random victim-stop/death trials** per principal native profile, with seed, chosen victim, observed cut interval, and all outcomes. Repetition numbers are coverage policy, not failure-probability estimates.

### 10.3 Delayed notification and false suspicion

For each cut that holds a live writer, pair SIGKILL with a **SIGSTOP/SIGCONT** experiment. Hold notification or progress for 1 ms, 10 ms, and 100 ms in distinct cases; these are test delays, not production lease expirations. Confirm the resumed writer can still access only its old owned allocation, while no actor assigns those bytes to a new owner based solely on elapsed time.

LE128-V1 has no lease-expiration timestamp: its epoch is a nonwrapping identity, not a clock. These timeouts are external observation policies. The safe result of a withheld death report may be continued delivery of unrelated ready entries, a lack-of-data/capacity observation, or retirement/quarantine. The unsafe result is a consumer advancing a cursor or generation merely to skip a lease it has not owned. A timeout can ask the authority to investigate; it cannot write an epoch into somebody else's block.

### 10.4 Controlled corruption versus abandoned partial data

Run a distinct integrity experiment: the authorized producer, paused at a test-only commit hook **after native checksum calculation and before publication**, changes one still-owned payload byte while retaining the old checksum. The hook must be after calculation so the normal commit routine does not simply recompute a valid checksum over the changed payload. An authorized consumer detects the mismatch after its valid QR/SPSC borrow and records INTEGRITY, then retires the generation. The test intentionally violates correct producer serialization; it validates error handling, not recovery from an ordinary stop failure.

Also mutate each immutable header byte in stable pre-attach fixtures and require CRC/semantic rejection as applicable. A checksum collision or a CRC-disabled payload is not an authentication boundary; full expected-byte checking remains the stress oracle. Never have a coordinator scan a concurrently written ordinary payload to decide whether the slot “looks corrupted.”

A mid-write abandoned lease receives a classification such as UNPUBLISHED_PARTIAL/OWNER_OUTCOME_UNKNOWN only when controller evidence supports it. An ordinary library without that evidence records UNCERTAIN. It must not read partial bytes or fabricate a corruption log to make the recovery story complete.

---

## 11. Managed recovery experiment and stream semantics

### 11.1 The permitted automated sequence

The live authority observes a confirmed registered-process failure or an authorized retirement request, closes new enrollment, and requests cooperative stopping. Surviving endpoints finish admitted old-generation calls/borrows where safe, acknowledging all aliases/helpers. Unresolved holder or transfer state is retained; the authority never infers quiescence from the count alone.

It reserves a manifest slot **before** creating one distinct successor, initializes the same ABI under a fresh session identity and grants, verifies the prefix, then attaches the admitted healthy cohort. The old object remains quarantined if any alias or outcome is unresolved. A staged/failed candidate remains charged. There is no same-generation endpoint-record reuse, orphan-token phase scan, in-place cursor reset, or private pointer redirection.

The successor must not be mapped over live old virtual addresses in any surviving process. The strongest false-suspicion test resumes the old producer after successor traffic begins and lets it finish permitted old accesses: successor sentinel/payload evidence must remain unaffected. Old messages are never mixed into the new session merely because their message IDs match.

### 11.2 What “monotonic” may legitimately mean

Within one generation, the NCQ dequeue-LP ticket sequence follows publication order. Processing-completion order may differ. Application message IDs are arbitrary. Across a generation cut, [P4] supplies neither a global FIFO nor exactly-once replay. Preserve these three distinctions in the log.

The **management audit log** may assign increasing event numbers and contain BEGIN_SESSION, DELIVERED_OBSERVED, OLD_OUTCOME_UNKNOWN, GAP_DECLARED, RETIREMENT, FENCE_CONFIRMED, and BEGIN_SUCCESSOR events. This log is external test evidence, not a new shared ABI structure. Its monotonic numbering says the audit record is ordered; it does not turn unknown messages into delivered ones or guarantee durable storage across host failure.

A requirement for continuous gap-free application delivery after arbitrary unobserved death is unsatisfied by the frozen ABI. A separate application replay/deduplication protocol could address another contract; it is not implemented or implicitly specified by this test plan. Healthy SPSC lanes can isolate a failed lane, but they likewise cannot invent its missing messages.

### 11.3 Recovery metrics and repetitions

Record at least t_inject, t_terminal_observed, t_authority_notified, t_retire, t_successor_ready, t_survivors_attached, and t_first_new_delivery. For a pause-only case, there is no terminal-death timestamp and no death-confirmed metric. Keep detector delay, authority scheduling, initialization, reattachment, and first-data restoration separate.

Retain [P3]'s objective **p99 ≤100 ms from confirmed failure notification to a validated successor available to surviving endpoints**, conditional on a live schedulable authority and reserved resources. Here “available” means READY plus successful intended-survivor attachment, not only allocated bytes. Also report injection-to-first-new-delivery so slow detection is visible. No such recovery delay counts as a nanosecond data-plane observation.

Collect 1,000 fresh managed recovery trials per nominated crash class and report all incomplete/failed/censored recoveries. Authority-death tests intentionally require safe stop and no automatic takeover; they do not get a restoration success by starting an unaccounted new manager. Five independent batches of 200 trials preserve restart effects; population-tail conclusions still require an appropriate dependence/uncertainty analysis and are not inferred from a handful of fastest recoveries.

### 11.4 Resource and fencing kills

Hold two generations unfenced in quarantine while one serves and one staged candidate is accounted. Attempt the next retirement/replacement. The authority must stop new admission before creating a third quarantine or a fifth backing record. Count all failed creates and abandoned candidates. Repeat with an authority crash: a replacement process cannot restart a fresh four-object allowance without complete reconciliation/fencing.

A parent exit with a child retaining a mapping must not satisfy global fencing. A stopped thread in a live process is not process death. Unlink/close alone must not satisfy alias revocation. These are required fail-closed outcomes, not successful continuous-recovery samples.

---

## 12. Exact compiler, sanitizer, and optimization profiles

These are **argument specifications**, not commands executed in this turn. Every later invocation records absolute compiler path, version/hash, target, SDK/sysroot, libraries, and complete argument order. A flag unsupported by the pinned compiler fails that profile instead of being silently omitted. The optional toolchain lane may use a compatible revision distinct from the production one; its evidence scope is labeled.

### 12.1 Common C11 warnings and production configuration

For Clang and GCC, require the following common warning/language argument set:

`-std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wformat=2 -Wundef -Werror=implicit-function-declaration -Werror=incompatible-pointer-types -Werror=return-type`

Review and resolve all remaining project warnings; do not force a stale system header warning into a fabricated success. The release reference adds:

`-O3 -g -DNDEBUG -fno-omit-frame-pointer -fno-lto`

Keep mandatory validation/oracles outside removable assertions. Keep strict aliasing semantics; do not use `-fno-strict-aliasing` to conceal an invalid C object overlay. Do not add `-ffast-math`, packed structures, a global atomic-strengthening flag, hidden per-process locks, or native-tuning changes under the same binary identity. Optional `-O2`, LTO, PGO, or more aggressive inlining are independent candidates that rerun the gates. [R08, R09, R10]

Compile native library objects with `-fPIC`. Linux test executables use `-fPIE` and link with `-pie -pthread`; thread-bearing Linux compilations also use `-pthread`. macOS uses its normal dynamic libSystem linkage and PIE executable policy; Linux-only link options are not copied into a Darwin command. Link through the compiler driver. Any extra historical `-lrt` requirement is part of that exact Linux tuple, not an undocumented universal dependency.

### 12.2 Target-specific arguments

| Profile | Additional arguments / admission |
|---|---|
| Apple Clang native ARM64 baseline | `-arch arm64 -mmacosx-version-min=14.4`; selected SDK is pinned with its actual sysroot |
| Apple/upstream Clang M4 candidate | Baseline plus `-mcpu=apple-m4`, only after the compiler accepts it and the execution host/features are admitted |
| Upstream Clang cross/native Darwin | Explicit recorded ARM64 Darwin target and SDK/sysroot; do not assume host compiler defaults or a Linux sysroot are valid |
| Linux x86-64 Clang/GCC baseline | `-march=x86-64 -mtune=generic -pthread`; use the native x86-64 target toolchain |
| Linux AArch64 portability lane | `-march=armv8-a -mtune=generic -pthread`; outlined/native atomic paths are recorded and audited |
| Linux AArch64 LSE candidate | `-march=armv8.1-a+lse -mtune=generic -pthread`, only on admitted LSE hardware |

LLVM records support for the apple-m4 CPU option, and current GCC AArch64 documentation includes the name. That does not mean every installed Apple Clang/GCC version accepts it or supplies a compatible Darwin sanitizer runtime. GCC is mandatory for the Linux cross-check lane; a Darwin GCC lane is optional and must prove its actual SDK/runtime support. Do not claim GCC TSan on Apple Silicon from the presence of an AArch64 tuning name alone. [R11, R12]

Do not simultaneously add an independently guessed `-march=armv9.*` that enables features absent from the actual Apple target. Record feature selection and inspect code; no universal `-march=native` binary is accepted as a reproducible cross-machine release. The frozen layout and C ABI remain the same for all admitted tuples.

### 12.3 Sanitizer build matrix

| Lane | Exact additional compile arguments (replacing release optimization/debug choice) | Link/runtime contract |
|---|---|---|
| TSan | `-O1 -g -fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-lto -fsanitize=thread` | Compiler driver also links `-fsanitize=thread`; PIE, all project core/caller code instrumented |
| ASan + UBSan | `-O1 -g -fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-lto -fsanitize=address,undefined -fno-sanitize-recover=all` | Link same sanitizer selections; no TSan combined |
| Optimized UBSan cross-check | `-O2 -g -fno-omit-frame-pointer -fno-lto -fsanitize=undefined -fno-sanitize-recover=all` | Link UBSan; mandatory scalar/range and alignment fixtures |
| Native debug assertions | `-O0 -g -fno-omit-frame-pointer -fno-lto` without NDEBUG | Not a latency binary |
| Parser mutation driver | Same ASan/UBSan build with the dependency-free bounded corpus driver | The required million-input corpus does not depend on a coverage-guided framework; any optional framework needs separate version/flag admission |

Clang documents TSan instrumentation and PIE limitations; ASan and UBSan cover different error classes, and ASan/TSan cannot be combined in the same executable. GCC's instrumentation options require their own supported runtime. [R07–R10] Do not infer that an mmap-internal use-after-lease is an ASan use-after-free: the memory can remain mapped and allocated. The ownership oracle must catch that logical lifetime violation.

### 12.4 Runtime sanitizer settings and suppression matrix

The requested TSan baseline options are:

`halt_on_error=1:exitcode=66:force_seq_cst_atomics=0:report_atomic_races=1:ignore_interceptors_accesses=0:ignore_noninstrumented_modules=0`

No `suppressions` path is set; no compile-time sanitizer ignorelist is passed. The pinned runtime's help/options report must confirm each requested setting. An unknown or overridden setting invalidates the declared profile. Record the actual values, including Apple-specific defaults that were overridden. Do not force SPSC atomics to SC to hide a missing synchronization edge. [R07]

ASan/UBSan baseline runtime settings are `ASAN_OPTIONS=halt_on_error=1:abort_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. Leak detection is enabled only in a separately declared runtime-supported lane; OS shared-object accounting is always checked independently. A leak-detector clean result does not count POSIX namespace objects, pending grants, and quarantines for the application. [R08, R09]

| Code/report category | Suppression policy | Required treatment |
|---|---|---|
| Queue cursors, entries, phases, payload ownership | None | Fix or reject; no no_sanitize attributes |
| Native handle/lease/attach/detach/wait bookkeeping | None | Fix or reject; do not silence library-wide symbols |
| Test driver and evidence recorder | None | Harness races invalidate affected results |
| Expected-race negative controls | No suppression | Separate executable/test expectation must observe the report or explain tool limitation |
| Known external-runtime issue | No blanket pass | Exact symbol/version/issue and narrow diagnostic-only exception; rerun unmodified baseline and label it blocked |
| Inline assembly or uninstrumented atomic helper | Not suppressible evidence gap | Inspect binary and use admitted compiler-atomic core lane; report instrumentation scope |
| Cross-process memory or distinct virtual aliases | Not “suppressed” | Covered by native/model oracles, not a claim of merged TSan shadow state |

No project-defined synthetic acquire/release annotations may manufacture the synchronization the queue is supposed to provide. TSan's documented need to observe instrumentation/synchronization motivates the one-process/one-mapping lane. This plan makes **no certification claim for cross-process race detection** from ordinary TSan runs; separate runtimes and address aliases require evidence beyond a clean thread report. [R07]

### 12.5 Assembly and linkage admission after build

For each critical [P4] ledger operation, archive source location, compiler IR/order, annotated disassembly, operand width/alignment, success/failure path, helper symbol, and target feature. Check SPSC reserve has no shared reservation CAS, its status word remains dormant, and POLL_ONLY has no hidden wait-word RMW. Check NCQ SC load/CAS semantics and strong failure behavior; inspect full retry control flow, not only one CASAL mnemonic.

Preserve eligible LDAPR for non-SC acquire when the exact compiler mapping permits it. Do not permit LDAPR substitution to weaken the NCQ SC reference. Confirm payload loads/writes occur under the ownership protocol; no post-LP descriptor cleanup; no CASP or two-word substitute for AU64; no full-structure reset after construction; no process-local lock fallback. Archive the uninstrumented optimized binary separately from sanitizer/fault-hook outputs.

---

## 13. Binding, lifecycle, and public-API negative tests

The native verification gate includes every [P4] ABI-01…ABI-24 history. In addition, exercise an invalid lease instance, wrong role, duplicate commit, duplicate release, abort while publication is already in progress, reentrant endpoint use, zero-length valid payload, length>B, null/invalid opaque handle, and supplementary OS errors after a successful LP. Reject from owned local state before touching an unowned descriptor.

For Python/CFFI or C++ wrappers, require view-retention tests: keep two aliases, release one, request native release/detach, and verify BUSY/retention until the last admitted alias ends. Destroy the outer wrapper while a view survives; the mapping and slot lease must both remain valid. Test blocked wait while a closer attempts unmapping. Raw-pointer escape is an explicit unsafe API path, not a safely revocable buffer; the copied safe path is labeled a copy. [P4, §13.4; R16]

Python scheduling, GIL/FFI entry cost, batches of repeated one-token operations, and copied-versus-borrowed receives are measured independently. A Python loop cannot be used to certify <15-ns native latency. A native repeated-operation loop exposed for amortization does not gain permission for multiple concurrent leases or change the ring publication contract. Optional wrapper tests can remain unimplemented at the first core-code stage, but wrapper safety/performance claims remain blocked until they run.

---

## 14. Evidence artifacts and reproducibility contract

Each future run directory contains a UTF-8 manifest, binary/toolchain identities, immutable ABI/grant fixture identifiers, its exact plan hash, raw result records, timer calibration, endpoint summaries, and a final disposition. None of those files is produced as a fake completed run in this turn.

| Artifact class | Minimum content |
|---|---|
| Run manifest | Condition ID, session identity, seed, P/C/K/N/B, checksum/wait mode, process layout, topology evidence, flags, binary hashes, start policy, offers, watchdogs |
| Delivery evidence | Per-consumer M-bit bitmaps plus receive/release/duplicate counts; per-producer quota/publication/outcome counts; immutable file hashes |
| Latency evidence | Raw offer/entry/read timestamps or missing marker; clock ID/timebase; session/message/endpoint identity; lower/upper interval; status/outcome; original order |
| Local history | Operation ID, saved ticket/entry, attempt/result, LP classification, locally captured token/epoch when owned, permitted local event order |
| Chaos evidence | Victim registration, cut point or supported interval, injected action, terminal notification, delayed knowledge, exact permitted outcome, old/new object budget |
| Formal evidence | Tool/model revision, bounded model parameters, state/transition totals, cutoffs, counterexamples, negative-control results, assumptions |
| Sanitizer evidence | Compiler/runtime hashes, actual options, full reports, exit status, instrumentation/suppression scope, matched negative-control results |
| Counter evidence | Instrument preset/events, target CPU/process scope, duration, enabled/running times, availability, sampling/multiplexing caveats |
| Summary | PASS/FAIL/INCONCLUSIVE by gate, denominator, quantiles, uncertainty procedure, all missing cases and unsupported target cells |

Private evidence can use bounded binary storage with a separately documented schema. Every variable-length record is length-delimited and bounds checked; record loss/overflow invalidates affected claims. CSV/JSON summaries cannot silently omit nonfinite or censored outcomes. Trace ordering is local/causal unless actual LP tickets establish a stronger order. The offline merge never invents an unknown LP from the time a log was written.

Write saved evidence only after the measured epoch unless the profile explicitly includes I/O; store checksums for integrity. A 100M bitmap proof is about the current run's provided evidence, not proof that a dishonest process could not forge it. Participants are nonmalicious under the frozen model. Preserve first-failure records and failed seeds as well as successes.

---

## 15. Final kill register and Turn 6 work boundary

| Gate | Preregistered rejection/blocker |
|---|---|
| G01 — format | Any admitted layout/type/atomic-width/construction violation |
| G02 — finite safety | One admitted model/native witness violating I01–I19 |
| G03 — exact zero loss | Missing/duplicate/out-of-range/bad-byte measured message, nonreturned token, incomplete 100M cohort, or invalid evidence |
| G04 — new fast target | Qualified uncontended p50 not <15 ns or p99 not <50 ns; 3/5 confirmed breaches reject profile, mixed blocks acceptance |
| G05 — metrology | Ordered common-domain timing and uncertainty insufficient for claimed threshold; INCONCLUSIVE, not a numerical pass |
| G06 — retained service | p99.9 >1,000 ns or deadline-miss fraction >0.001 at declared common sub-saturation load under inherited 3/5 rule |
| G07 — scaling | Mean RMW attempts >24/lifecycle and 16P/16C goodput <90% of 8P/8C in 3/5 matched trials |
| G08 — comparative capacity | <75% of a semantically admitted SPSC-lane alternative under equal complete resources, reproducibly; no weaker-ordering camouflage |
| G09 — fairness | [P3]'s 100-ms pending request while peers complete ≥100,000 comparable operations and capacity evidence supports the comparison |
| G10 — recovery | Timeout theft, fabricated corruption, inferred exactly-once, unknown-holder reuse, or restoration claim without fenced/isolated backing |
| G11 — managed restoration | Failure to meet declared 100-ms p99 successor-availability objective under its stated prerequisites; detector delay reported separately |
| G12 — object budget | More than four accounted backing objects, more than two quarantines, or unaccounted authority takeover |
| G13 — coverage/tooling | Required native/sanitizer/model case missing, relevant mutant undetected, broad core suppression, wrong clock/toolchain/CPU label |
| G14 — retained unsupported contracts | MPMC parking, persistent power-loss recovery, indefinite gap-free availability with unfenced writers, or unproved raw-pointer revocation claimed supplied |

The stricter new fast-path targets do not erase the original or service-level history. A result of p50=30 ns would meet the older <50-ns median experiment but fail the new <15-ns requirement. A result of p99=70 ns could meet a one-microsecond p99.9 budget but fail the requested 50-ns p99. Report each honestly. A metrology failure is not proof the transport is slow; it is failure to establish the claim.

### 15.1 Implementation sequence authorized only by the subsequent commission

Turn 6 should first implement the frozen byte definitions/assertions, pure validation/CRC arithmetic, managed construction/one-shot lifetime skeleton, SPSC polling, and the exact NCQ-SC64 reference. Preserve public outcomes and private one-token state. Include test hooks only in distinct verification builds. Add actual sanitizer/native admission and the smallest invariant tests before attempting performance optimization.

No green benchmark permits changing release/acquire to volatile, SC metadata to weaker orders, private reservation to a blocking ticket, timeout suspicion to revocation, or fixed fields to native pointers. A failing NCQ service profile can motivate the separately proved SCQ candidate or semantically explicit SPSC lanes; it cannot mutate the reference under the same name.

The first implementation can be correct while failing the ambitious latency target. That outcome is useful evidence, not permission to strip checks or reinterpret average throughput. Conversely a very fast loop that fails one ownership witness is rejected.

---

## 16. Traceable answers to the commissioning directives

**100M run:** §3 fixes exactly 100M measured IDs per run, producer quotas, all 64 payload bytes, private bitmaps, warmup, legitimate quiescence, start/end events, five restarts, and final token reconciliation. It distinguishes data-loop timing from offline verification while retaining the cost of in-run checking.

**<15-ns median / <50-ns p99:** §§4–6 define direct one-way public-ABI latency, clock ordering/conversion, private timestamp joins, interval uncertainty, stricter goals, open-loop preservation, and dependence-aware acceptance. No measured result is supplied.

**16P/16C:** §2 labels 32 endpoint workers and 2× worker oversubscription on the nominated 16-core host; §9 includes that mandatory cell and smaller valid capacities/participants. K32 remains within N1024; no undocumented core pinning is asserted.

**ABA/tearing/holes:** §§7–9 link every claim to proof assumptions, finite bounds, actual binary admission, natural IPC stress, named forced histories, and detector-negative controls. No finite trial is misrepresented as universal absence.

**SIGKILL and timeout recovery:** §§10–11 specify exact and random cut points, delayed observation, SIGSTOP/resume controls, no partial delivery, managed distinct successors, unknown outcomes, fencing, aliases, and the four-object budget. The requested unsafe cursor-advance-by-timeout behavior is explicitly a failing mutant.

**Compilers/TSan/M4:** §12 supplies exact argument sets and runtime settings, empty core suppressions, separate sanitizer lanes, target-specific qualification, and a no-guessing rule for unsupported M4 flags or Darwin GCC runtimes. Toolchain installation is not simulated.

**Deliverable:** this specification and its literal SHA receipt are the normative Turn 5 pair. Any ZIP is a document envelope, not an implemented harness. Transfer verification is recorded separately after actual upload/readback.

---

## 17. Artifact integrity

Use UTF-8 without BOM, LF newlines, and one final LF. Canonicalization replaces only the single header field `Canonical-SHA256`'s 64 lowercase hexadecimal characters by 64 ASCII zero characters. Hash every other byte unchanged. Reject missing or duplicate fields. The embedded digest is SHA256(canonical bytes), not a supposed self-containing literal-file fixed point.

The adjacent receipt contains SHA256(literal finalized bytes), two ASCII spaces, the exact Markdown basename, and one LF. Any ZIP has its own sidecar and manifest, excludes itself and its own hash from self-referential contents, and contains no source, header, executable model, assembly program, or runnable harness. Local document arithmetic and hash checks are not logged as executed queue verification.

---

## 18. Primary-source ledger and verified predecessor identities

Access date for external verification: **2026-09-23**. Live pages establish documented mechanisms, not a pinned installed compiler, SDK, processor, or runtime. No external source implementation is copied into this artifact. The plan's workload/oracle/statistical/lifecycle test choices are original normative proposals grounded in the frozen project records.

**[P4] Turn 04 — C11 ABI and Assembly Specification.**
93,610 bytes; literal SHA-256 `6aaa3d6706c29096cfab56ea1fe569afa561c4cba7caea26ebd3c09cc9812386`.
See repository reference file: `docs/reference/04_Turn_04_C11_ABI_and_Assembly_Specification.md`.
Full supplied text and mounted bytes read/verified. Governs every ABI constant, operation order, lifetime condition, the 10-ms wait slice, and the four-object/two-quarantine budget.

**[P3] Governing Turn 03 — Concurrency Kill Gate.**
81,776 bytes; literal SHA-256 `7ad5fbd43134e7d3419c5a1f76d030e25d051510dbc6f20ce173b3d0eccff605`.
See repository reference file: `docs/reference/03_Turn_03_Concurrency_Kill_Gate_and_Failure_Modes.md`.
Full supplied text and mounted bytes verified. Governs 1,000-ns service cliff, five-run/three-breach policy, common-load comparison, 24-attempt/90% scaling gate, starvation tripwire, and managed-recovery objective. Older differing chat variants are not substituted.

**[P2] Turn 02 — Cache Topologies and Barrier Proofs.**
91,463 bytes; literal SHA-256 `9bb8b971b65f7da80bd13053e8027aaa6ede5ae9ea19da8262e24282e16a6977`.
See repository reference file: `docs/reference/02_Turn_02_Cache_Topologies_and_Barrier_Proofs.md`.
Full supplied text and mounted bytes verified. Supplies SPSC ownership chains, NCQ ghost frontier/LPs/token conservation, single-waiter proof, and separation of physical placement from universal hardware claims.

**[R01] Apple, Technical Q&A QA1398 — Mach Absolute Time Units.**
https://developer.apple.com/library/archive/qa/qa1398/_index.html
Official archived API explanation of CPU-dependent tick units and mach_timebase_info conversion. Used for units, not for a modern M4 clock rate or direct reuse of its illustrative overflow-prone arithmetic.

**[R02] Apple, WWDC25 — Optimize CPU performance with Instruments.**
https://developer.apple.com/videos/play/wwdc2025/308/
Official CPU Counters preset/bottleneck-analysis guidance and sampling explanation. Supports a separate repeated profiling lane; not a per-message nanosecond timer or universal event-ID table.

**[R03] Apple XNU, CPU Counters observability documentation.**
https://github.com/apple-oss-distributions/xnu/blob/main/doc/observability/cpu_counters.md
Official source documentation distinguishing CPMU fixed/configurable counters, uncore counters, counting/sampling, and system policy. A description of kernel facilities is not a promise of public unrestricted user access.

**[R04] Arm, Improving performance of multi-threaded applications on Arm.**
https://developer.arm.com/community/arm-community-blogs/b/architectures-and-processors-blog/posts/multi-threaded-applications-arm
Primary engineering discussion of counter-read speculation/ISB and newer counter facilities; explicitly notes that nominal resolution and update rate can differ. Its example machines and delay-tuning numbers are not adopted as Apple timings. No example implementation is copied.

**[R05] Linux perf project, perf tools support for Intel Processor Trace.**
https://perfwiki.github.io/main/perf-tools-support-for-intel-processor-trace/
Primary perf-author documentation quoting Intel's timestamp ordering requirements and distinguishing RDTSC/RDTSCP, LFENCE, and preceding-store visibility. Used for timing-adapter obligations, not as a binary audit or all-vendor timing guarantee.

**[R06] Linux man-pages, perf_event_open(2).**
https://man7.org/linux/man-pages/man2/perf_event_open.2.html
Primary interface documentation for scope, event groups, enabled/running time, access controls, and multiplexing. Supports counter metadata and explicit unavailable/scaled classifications.

**[R07] LLVM Clang, ThreadSanitizer.**
https://clang.llvm.org/docs/ThreadSanitizer.html
Official compile/link instrumentation, supported platform, ignorelist/instrumentation caveat, PIE, and runtime-option documentation. Drives the explicit unsuppressed thread lane; no interprocess shadow-state certification is inferred.

**[R08] LLVM Clang, AddressSanitizer.**
https://clang.llvm.org/docs/AddressSanitizer.html
Official compiler/linker instrumentation, O1/debug/frame-pointer recommendations, detected classes and runtime limitations. Does not detect ring lease lifetime merely because backing memory remains allocated.

**[R09] LLVM Clang, UndefinedBehaviorSanitizer; Clang 22.1.0 User's Manual.**
https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html
https://releases.llvm.org/22.1.0/tools/clang/docs/UsersManual.html
Official check/recovery configuration and the incompatibility of combining address/thread/memory sanitizers in one program. No assertion that the user's installed compiler is this release.

**[R10] GCC, Instrumentation Options.**
https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html
Official sanitizer/optimization interaction and supported flag semantics. The installed target runtime remains separately admitted; a flag list is not a test run.

**[R11] LLVM project commit discussion, Support -mcpu=apple-m4, PR 95478.**
https://lists.llvm.org/pipermail/cfe-commits/Week-of-Mon-20240610/588591.html
Primary compiler-development record for the CPU name. Establishes that the option has an upstream implementation history, not universal availability in every Apple toolchain or a guarantee that every Apple CPU implements its selected features.

**[R12] GCC, AArch64 Options.**
https://gcc.gnu.org/onlinedocs/gcc/AArch64-Options.html
Current official CPU/architecture/tuning option documentation, including apple-m4 among supported names. Does not establish a working Darwin GCC sanitizer/runtime deployment.

**[R13] MPI-SWS, GenMC project and author repository.**
https://plv.mpi-sws.org/genmc/
https://github.com/MPI-SWS/genmc
Primary project description of memory-model checking and LLVM-level implementation. Exact revision, frontend, model and bounds must be pinned before use. No installed checker or executable result is claimed.

**[R14] Linux man-pages, pidfd_open(2).**
https://man7.org/linux/man-pages/man2/pidfd_open.2.html
Process-bound lifecycle handle and registration conditions; also anchored in [P4]. Supports distinguishing child identity/terminal evidence from stale numerical PID or a sent signal.

**[R15] Apple, waitpid system-call documentation.**
https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/waitpid.2.html
Official archived terminal/stopped status semantics carried in [P4, R12]. Used for the managed-child death-versus-pause requirement, not arbitrary process takeover or a scheduling deadline.

**[R16] CFFI, Using ffi/lib objects.**
https://cffi.readthedocs.io/en/latest/using.html
Binding-author buffer/lifetime documentation carried in [P4, R19]. The native slot lease remains an additional project obligation beyond Python/cdata object lifetime.

### Evidence closure

The frozen predecessors' byte identities and this artifact's document arithmetic/integrity can be checked during production of this specification. No source file implementing the transport, sanitizer executable, model-checker program, timing harness, crash driver, or hardware counter probe has been generated or run here. Every machine-result cell remains NOT_RUN. The next authorized implementation is evaluated against this plan rather than being used to retroactively invent its acceptance criteria.
