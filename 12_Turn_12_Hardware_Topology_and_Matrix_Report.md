# Turn 12 — Hardware Topology and Workload Matrix
## ELITEIPC 1.0.1 / LE128-V1 / A09-06 remediation

**Project:** Leonid Majbits / Gemini Operator Lab, paired with Leon  
**Stage:** Turn 12 — diagnostic implementation and executed workload contrasts  
**Evidence date:** 24 September 2026 UTC; host timestamps remain in raw records.  
**Disposition:** Topology-aware measurement, exact raw replay, and bounded hardware claims implemented. The specified Linux contrast design completed under two compilers. New Apple execution, cross-NUMA hardware execution, universal performance and full prior release qualification are not certified.  
**Canonical-SHA256:** `9aca035c481acc837af27d8173d7bf24363509a760ffaa3472856b5ef54b0cd1`
**Hash convention:** Normalize only this field's 64 hexadecimal characters to 64 ASCII zeros before hashing. The detached sidecar hashes the literal finalized file.  
**Authoritative parent:** Turn 11 PMC bundle, 6,609,713 bytes; SHA-256 `aaed6424e463b9ec17b9d5d609f39aab36a249cea8f542b89fa47cd65997d448`.  
**Destination:** Deployment Research / Elite_Systems_LockFree_RingBuffer  
**Evidence vocabulary:** IMPLEMENTED is source present; EXECUTED is a retained local observation; SYNTHETIC is an intentionally constructed input; LAB_REPORTED is the user's hardware statement; NOT_RUN is not a pass.

## 0. Decision and measurable outcome

A09-06 was a claim-boundary and workload-coverage finding, not evidence that
payload publication ordering was wrong. This turn supplies a setup-time topology
module, native and Python workload callers, exact per-message/raw-token receipts,
an independent verifier and explicit documentation of unsupported claims. It does
not alter the queue to obtain a more favorable rate.

**Two final compiler campaigns completed 186 trials, 1,458,432 measured messages
and 38,895,095,808 measured application payload bytes.** An additional 83,736 warmup
messages are separate. Each compiler ran 31 runnable contrast cells, three fresh
ring generations per cell. Six other planned cells are explicitly unsupported.
There are 37 declared cells per plan, not an implied full Cartesian experiment.
All completed finite-run membership and final-token checks passed.

The most revealing adverse observation is native NCQ 16P/1C: roughly 8.2–8.5k
messages/s and approximately 16-ms median-of-trial offered p99 on this selected
four-CPU pool. Many less contended cells are much faster. This is a concrete
operating-envelope warning, not a new proof of global livelock or a diagnosed
hardware cause. All such outcomes remain in the table and receipts.

The topology and verifier explicitly refuse to label Mach QoS as P-core pinning,
CPU NUMA placement as page placement, a small byte count as actual cache residency,
or arithmetic replay as a physical-clock uncertainty certificate. The historical
Apple 22.8-GB/s and submicrosecond observations remain labeled by their original
reported workload and evidence; none is generalized to this new matrix [P11,U12].

## 1. Baseline custody and change boundary

The mounted authoritative Turn 11 archive was checked against the published
SHA-256, extracted separately, and admitted by its existing release verifier:
1,336 full-file and 76 source-manifest entries matched before modifications.
Its audit supplies the exact eight-row matrix in sections 6.6–6.7; the six grouped
items in the new directive do not silently delete OS-fault and integrity axes.

**All 16 inherited source files under the selected native include/src and Python
binding surfaces remain byte-identical.** `CORE_BINDING_CONTINUITY.json` records
all before/after hashes. The production SPSC, NCQ, lifecycle, checksum, wait,
exporter, `_Pin` and GC code are unchanged. Library 1.0.1 and the shared wire ABI
`0x00010000` remain unchanged.

The new `libelite_topology.a` is optional and separate. Benchmarks link topology
code; the production ring library does not. A separate diagnostic bridge lets
Python callers use typed test-control atomics and the same ordered OS clock. It
contains no second ring and no native loop that hides Python queue calls. No
third-party package, framework, privilege modification, or external daemon was
introduced.

| Source | Role |
|---|---|
| `include/elite_topology.h`, `src/elite_topology.c` | Read-only native OS-visible topology, strict bounded parsers and JSON |
| `benchmarks/bench_topology.c` | Live discovery or explicitly synthetic rooted fixture CLI |
| `benchmarks/bench_matrix.c` | Spawned native IPC callers, arrivals, retention, full-byte oracle and raw timing |
| `benchmarks/matrix_bridge.c` | Python diagnostic clock/control/placement and quiescent token snapshot |
| `bindings/python/matrix_worker.py` | Public Python leases/views in independent producer and consumer processes |
| `tools/run_matrix.py` | Predeclared resource-aware contrast plan and bounded owned cohorts |
| `tools/verify_matrix_results.py` | Independent topology, timing, membership and token replay |
| `tests/test_topology.c/.py` | Native parser and rooted topology/placement tests |
| `tests/test_matrix_results.py` | Fifty positive/negative record and campaign tests |
| `docs/HARDWARE_BOUNDS.md`, `MATRIX_VERIFICATION.md` | Hardware claim rules and exact operational/trace contract |

Additive changes to the inherited benchmark common code supply configurable
payload/checksum setup and complete quiescent free-token snapshots. Historical
RTT, throughput and cache executables now emit topology in their JSON and a
startup sidecar. Their historical raw data is not retroactively edited. Build and
manifest tools cover the new files; prior reports and failed evidence remain
preserved as history.

## 2. Topology architecture

### 2.1 Discovery is a bounded observation, not physical attestation

`elite-hardware-topology-v1` separates reported values from absence. A scalar has
`value`, `status`, and `error`; an unavailable value is null, never guessed zero.
CPU sets are sorted sparse IDs. Objects include platform, architecture, kernel,
model, eligibility, caches, core/socket/die/cluster IDs, SMT siblings, NUMA maps,
perf levels, visible cgroup ancestors and environment indicators.

Discovery has explicit safety limits: CPU IDs below 1024, sixteen cache entries
per CPU, 256 nodes, sixteen perf levels, 64 visible cgroup ancestors and bounded
text reads. Exceeding a cap marks partial/unavailable state. These are resource
limits, not assertions about a particular processor. The planner must not create
a locality claim from an incomplete or failed required observation.

The before/after topology snapshots expose changes at those times; they are not
an atomic global hotplug snapshot or proof that no resource changed between them.
Neither `uname`, sysctl, sysfs nor a binary hash remotely attests the hardware.

### 2.2 Linux

Execution eligibility comes from `sched_getaffinity`, not from assuming IDs
`0..online_count-1`. The module reads each eligible CPU's package/die/core/cluster
IDs, sibling list and all bounded cache indexes, including cache type, level,
capacity, line size, ID and shared CPU map. It separately enumerates node cpulists.
Reported CPU maps can include offline siblings; they are not rewritten to make a
neater picture. Missing or conflicting node membership cannot certify locality.

The `physical_cpus` value on Linux counts known `(package,die,core)` tuples in
the **eligible set**. It is not the system's advertised full physical-core count.
Unknown required identity fields leave it unavailable. The model string is
explicitly OS-reported; a hypervisor may provide virtual topology.

The cgroup-v2 collector resolves `/proc/self/cgroup` using mountinfo, including a
non-root exposed hierarchy, then walks visible ancestors to the mount root.
Quota/period, memory limit/current use and effective CPU/memory-node sets are
retained at each level. A leaf `max` does not cancel a parent's limit. V1,
missing/escaped/unresolved mounts and safety-cap failures stay explicit rather
than being translated to unlimited resources. Invisible ancestor restrictions
remain unknown [R1,R2].

A container marker or exposed hypervisor flag yields
`TOPOLOGY_VIRTUALIZED_CONTAINER`. A cgroup alone is not proof of virtualization;
absence of every marker is not proof of bare metal. The observed host is the
former case. No sysfs value is turned into a universal EPYC or Xeon hardware fact.

### 2.3 Darwin

The module queries actual sysctl return widths, core and physical-memory counts,
cache information, translation status and optional memory frequency/channels.
Perf levels are enumerated through `hw.nperflevels`. Their reported names map
`Performance` and `Efficiency` to the requested classes; unknown names stay
unknown. The host can be classified MIXED when both are reported. Index zero is
not hardcoded as a P-core identity. `hw.perflevelN.physicalcpu`, logical counts,
L1D/L2/L3 capacities and CPUs-per-L2 are captured when supplied [R3].

These aggregate values do not supply a per-worker CPU-to-P/E-class map. The
Darwin backend therefore does not fabricate one. QoS and Mach affinity requests
retain their return/error/readback evidence but cannot prove pinning [R4]. New
intra-P or P-to-E experiments remain unsupported until an admitted placement
observation/control mechanism exists. Missing memory channel/frequency information
is null, not a model-name lookup or hardcoded frequency.

**The new Darwin branch is implemented but not compiled against an Apple SDK or
executed on Apple hardware here.** The user's reported Turn 11 passes are credited
as earlier target observations, not transplanted to this new source.

### 2.4 Memory placement and cache capacity

The native initializer touches the mapped object before exposing it. The matrix
neither binds pages nor measures their physical node residency. Enforced CPU
masks on separate NUMA nodes therefore support CPU placement, not a statement
about which node serves each payload page. CPU affinity and NUMA memory policy
are different interfaces and scopes [R5,R6].

The verifier deduplicates reported selected last-level data/unified cache domains
and compares logical payload-pool and backing extents with reported capacities.
On Darwin it does not rename aggregate L2 as an unobserved system-level cache.
The receipt explicitly retains `cache_residency: NOT_ESTABLISHED` and
`dram_saturation: NOT_ESTABLISHED`. A 64-MiB payload may exceed an exposed cache,
but misses, ownership transfers, cache conflicts, prefetching, active subset,
memory placement and descheduling still govern its cost [R7].

## 3. Concrete matrix, not unspecified exhaustive coverage

The audit's eight axes are retained. The plan is committed before launching its
first cell and later bound by SHA-256 in `MATRIX_COMPLETE.json`.

| Audit dimension | Runnable contrasts in this delivery | Explicit remaining conditions |
|---|---|---|
| Locality | Requested CPU pool; reported shared data/unified L2 pair; automatic known cross-NUMA selection when available | P/E pinning unavailable; local host has one NUMA node |
| Population | Native 1/1, 2/2, 4/4, 8/8, 16/1, 1/16, 16/16; Python asymmetric 16/1 and 1/16 | Not an individual fairness or bounded completion proof |
| Pool size | 4-KiB small pool, 4-MiB L2 candidate, 8-MiB helper pool, 128-MiB LLC candidate | Actual residency/DRAM saturation not measured |
| Arrival | Closed per-producer loop, saturation, fixed100k/s intended offers, burst32 at the same nominal mean rate | No independently calibrated 70%-load qualification |
| Retention | Immediate release; actual held native/Python views; GC during live view | Special all-tokens-held transition fixture remains separate/unimplemented here |
| Runtime | Native C, Python helper, Python per-byte loop, Python GC pressure | Not a free-threaded interpreter or hidden native bulk ring benchmark |
| OS faults | Actual quotas and resource counters; explicit yield requests | No deliberate whole-host memory-pressure attack on shared resources |
| Integrity | NONE and CRC64 public-API cost | Existing owned-corruption adversarial lane is not assigned a successful throughput sample |

The default executed plan has 31 RUN and six SKIP_UNSUPPORTED cells. Unsupported
entries are `cross-numa`, `intra_perf`, `perf_to_efficiency`, `all-tokens-retained`,
`host-memory-pressure`, and `owned-corruption`. The first depends on host exposure;
the P/E limitation is a backend capability boundary. Resource-limited cells would
instead use SKIP_RESOURCE. Neither category counts as a passed trial.

SPSC only admits 1P/1C. The NCQ reference admits K=P+C, at most one token per
endpoint, with K<=N. In particular 16P/16C means 32 endpoints and N=32 in the small
matrix, not sixteen active participants. The planner does not hide oversubscription
by skipping requested high-contention cases.

The default run uses three repetitions. Each is a fresh ring generation. Native
repetitions of one cell share their coordinator process; Python repetitions use
fresh coordinators. This distinction is recorded, not called five independent
OS restarts. `--small` changes the 64-MiB conditions to sixteen measured messages
per trial; all other exact quotas remain in the plan. Those small tails are
empirical order statistics, not strong population-tail estimates.

## 4. Workload and timing implementation

### 4.1 Real IPC and direct payload work

Native workers are independently spawned processes with separately attached
MAP_SHARED mappings and one-shot grants. Setup pipes carry configuration/results,
not payloads. Python workers use the released `EliteShm` and producer/consumer
classes. The small native bridge handles ordered timestamps, typed test-control
atomics, placement and quiescent inspection; it has no native queue batch loop.

Every measured message is constructed directly in its leased payload and fully
checked before read release. Payload lengths are multiples of eight. The named
pattern repeats `message_id xor 0xa5a5a5a5a5a5a5a5` in little-endian words; the
Python compiled helper uses the same pattern. The Python-bytecode lane touches
every byte in Python instead. Length, type and identity checks remain active
under release builds. These are deliberately different runtime-cost profiles.

### 4.2 Arrival and retention

For producer p and sequence s, the global intended order is s*P+p. Fixed-rate
offers use t0+ceil(order*period_ns/timebase). Burst offers use the burst index and
original burst-period schedule. Late execution or repeated NO_CAPACITY never
moves an intended offer forward. Thus queue and generator delays remain visible.

Closed-loop mode waits for the preceding message's release acknowledgment before
issuing the next offer from that producer. Acknowledgments use independent
test-control cells after release; their interference is a stated workload cost,
not a free production-queue feature. Saturation uses actual offer timestamps and
has a different interpretation from a preregistered external arrival stream.

Retention holds an actual native lease or Python buffer for the requested interval.
The read-complete tick precedes the hold; the return tick follows release. The
verifier checks selected holds from those raw values. Yield requests are explicit
caller work; issuing a yield does not certify that the OS switched away.

### 4.3 Per-message raw evidence and exact equations

Producers export little-endian u64 triples `(id, offer_tick, entry_tick)`.
Consumers export `(id, read_tick, return_tick)` and complete membership bitmaps.
Evidence arrays are allocated before measurement. Logging and timing costs remain
part of the instrumented workload; post-run file export and analysis are separate.

For each message m:

- Native entry latency: `read[m] - entry[m]`.
- Offered-service latency: `read[m] - offer[m]`.
- Return-complete latency: `return[m] - entry[m]`.

These are not RTT/2. The primary host clock is CLOCK_MONOTONIC_RAW on Linux or
mach_absolute_time with its actual conversion on Darwin. Recorded absolute clock
uncertainty remains NOT_QUALIFIED. No nominal timer frequency, equal timestamp,
or printed decimal is promoted to a physical-time guarantee.

Let n/d be nanoseconds per tick, t0 the scheduled common start and E the maximum
consumer drain-quiescent end. For M measured payloads of B valid bytes:

`D = E - t0; elapsed_ns = D*n/d; messages/s = M*1e9*d/(D*n); GB/s = M*B*d/(D*n)`.

Decimal GB counts application bytes once. It is not DRAM traffic. Both elapsed
and derived floating values are checked against exact Python Fraction equations
within one binary64 ULP. Raw ticks/counts have no tolerance. Quantiles use exact
nearest ranks, with outward-ceiling conversion to integer nanoseconds.

Every Turn 12 message has timestamps. Therefore these rates cannot be substituted
for Turn 7's throughput lane without per-message timestamps. The final interval
also includes the control/drain tail, not a retroactively guessed last-read time.

## 5. Independent verifier and falsification surface

The native and Python callers write raw data; the standalone verifier joins it
by message identity. Each producer must provide its exact quota. Every consumer's
bitmap population must equal its completed count; bitmaps must be pairwise
disjoint, their union complete, and consumer trace IDs must agree exactly with
those bitmap bits. Checksums or matching totals alone are insufficient.

After all data accesses ended, a native management snapshot records SPSC P/C or
NCQ QF/QR heads/tails, every live free-entry ticket/word/block, and each block's
EMPTY phase/epoch. The verifier independently requires the expected publication
count, correct cycle packing, all unique block IDs, matching phase/epoch and no
remaining ready or held token. Old physical queue words outside live membership
are not counted as extra tokens. A final trace that merely says `all_tokens_returned`
without its actual snapshot cannot pass.

Topology checking validates sparse sets, CPU/cache membership and node consistency.
A `same_l2` claim needs shared **data/unified** L2 maps; an instruction-only cache
cannot certify it. Cross-NUMA producer and consumer node sets must be known and
disjoint. Darwin aggregate geometry cannot satisfy a forged P/E worker claim.
The verifier checks start/end constraint readback, not invented exclusive-core
ownership or unobserved mid-interval migration.

A campaign must contain exactly its declared trial IDs and complete/skip counts,
and its plan hash must match. Missing completion markers, changed plans, extra
unmanifested evidence, truncated traces, changed rates, negative or overrange
ticks, fake residency certificates, duplicate keys, NaN/infinity and booleans used
as numeric settings fail closed. Optimized Python cannot disable these checks.

`--binary` optionally checks a supplied native executable against the measured
SHA-256 and size. `--source-root` checks embedded compile inputs; Python results
separately identify interpreter, exporter, native ring library, bridge and caller
source. These are custody checks, not remote attestation. A dishonest recorder
can fabricate internally consistent observations. A different path/SDK can also
legitimately change a rebuilt binary's debug identity.

A completed run establishes its finite membership/token accounting and completion
under that workload. It does not prove every future schedule deadlock-free,
exclude individual starvation, or guarantee cleanup after an arbitrary killed
owner. The unchanged core's managed-generation and uncertain-outcome rules apply.

## 6. Actual local host and measured results

The new campaigns ran on Linux 6.18.44/x86-64 with a reported AMD EPYC 9V74 80-Core
model string, **five affinity-eligible OS CPUs (0–4)** and one exposed NUMA node.
Visible quota400000/100000 supplied **four CPU-time equivalents**; memory.max was
4GiB. The planner selected CPUs0–3 round-robin, except its topology-selected shared
L2 pair. This is a shared virtualized/container view, not eighty allocated cores
or dedicated bare metal. OS-reported L1D was32KiB, L2 1MiB, and selected shared L3
32MiB; none is a universal AMD specification. Memory frequency and channels were
unavailable. All machine and per-worker observations remain in raw JSON.

GCC 14.2.0 and Clang 17.0.0 used the strict inherited C11/O3 warning/target profile.
The final binaries' exact flags, compiler/source/binary identities are embedded.
Both campaigns have 93 completed records and 729,216 measured messages. There
were no observed missing/duplicate/bad-byte payloads or unreturned tokens in
these admitted completed runs. The shared host was not thermally isolated,
frequency-controlled or guaranteed free of unrelated activity.

### 6.1 Complete contrast table

Rates below are marginal medians of three trial rates. Each p99 column is the
**median of three per-trial empirical offered-service p99 values**, not a pooled
quantile or confidence bound. Raw traces and all minimum/maximum rates are
preserved in MATRIX_RESULTS.csv and RESULTS_SUMMARY.json. Tiny populations in
large-payload/GC cells are not sold as extreme-tail qualification.

| Condition | P/C | Payload | GCC median msg/s | Clang median msg/s | GCC median trial offered p99 (ns) | Clang median trial offered p99 (ns) |
|---|---:|---:|---:|---:|---:|---:|
| spsc-closed-small | 1/1 | 64 B | 2,918,221 | 2,307,081 | 246 | 276 |
| ncq-pop-1-1 | 1/1 | 64 B | 4,676,339 | 4,977,277 | 5,502 | 6,795 |
| ncq-pop-2-2 | 2/2 | 64 B | 6,379,044 | 2,295,845 | 6,715 | 24,492 |
| ncq-pop-4-4 | 4/4 | 64 B | 2,266,810 | 1,833,580 | 6,160 | 17,080 |
| ncq-pop-8-8 | 8/8 | 64 B | 1,262,875 | 1,706,890 | 12,484 | 7,817 |
| ncq-pop-16-1 | 16/1 | 64 B | 8,171 | 8,490 | 16,031,767 | 16,076,138 |
| ncq-pop-1-16 | 1/16 | 64 B | 471,537 | 455,986 | 16,069 | 15,006 |
| ncq-pop-16-16 | 16/16 | 64 B | 1,004,495 | 1,044,150 | 19,464 | 14,611 |
| ncq-closed-small | 1/1 | 64 B | 763,110 | 733,623 | 972 | 906 |
| spsc-saturation-small | 1/1 | 64 B | 3,697,705 | 2,347,036 | 8,252 | 3,520 |
| ncq-fixed-rate | 1/1 | 64 B | 99,933 | 99,686 | 128,474 | 151,312 |
| ncq-bursts | 1/1 | 64 B | 101,454 | 101,199 | 31,454 | 336,102 |
| spsc-retained | 1/1 | 64 B | 9,631 | 9,664 | 361,414 | 303,727 |
| spsc-crc64 | 1/1 | 4,096 B | 32,150 | 42,631 | 182,006 | 308,219 |
| spsc-helper-c-1m | 1/1 | 1,048,576 B | 2,303 | 2,284 | 803,672 | 780,953 |
| spsc-python-1m | 1/1 | 1,048,576 B | 11,658 | 9,477 | 653,112 | 856,280 |
| spsc-python-bytecode | 1/1 | 64 B | 24,233 | 26,975 | 95,392 | 83,019 |
| spsc-python-retained-gc | 1/1 | 64 B | 418 | 411 | 74,271,675 | 79,659,460 |
| spsc-llc-candidate | 1/1 | 67,108,864 B | 267 | 248 | 8,152,023 | 7,645,785 |
| ncq-retained | 1/1 | 64 B | 9,504 | 9,842 | 387,472 | 247,403 |
| ncq-crc64 | 1/1 | 4,096 B | 30,568 | 43,141 | 567,384 | 88,862 |
| ncq-helper-c-1m | 1/1 | 1,048,576 B | 2,326 | 49,634 | 798,242 | 308,444 |
| ncq-python-1m | 1/1 | 1,048,576 B | 2,217 | 7,604 | 797,206 | 1,408,992 |
| ncq-python-bytecode | 1/1 | 64 B | 25,735 | 26,600 | 117,320 | 94,426 |
| ncq-python-retained-gc | 1/1 | 64 B | 373 | 411 | 88,964,869 | 78,968,651 |
| ncq-llc-candidate | 1/1 | 67,108,864 B | 255 | 258 | 7,640,608 | 9,014,953 |
| ncq-l2-candidate | 1/1 | 4,096 B | 1,796,922 | 1,526,505 | 3,565 | 65,027 |
| ncq-yield-requests | 4/4 | 64 B | 54,052 | 85,584 | 1,411,917 | 2,314,394 |
| python-asymmetric-1-16 | 1/16 | 64 B | 10,487 | 10,600 | 125,086 | 77,030 |
| python-asymmetric-16-1 | 16/1 | 64 B | 6,692 | 6,793 | 24,428,153 | 24,482,640 |
| reported-same-l2 | 1/1 | 64 B | 1,234,672 | 928,797 | 837 | 877 |


### 6.2 What the data does and does not explain

Small closed-loop SPSC had median-of-trial native p99 215ns (GCC) and236ns (Clang).
Native NCQ16P/1C had approximately16-ms offered p99 and greatly reduced goodput.
Its seventeen workers share four selected CPUs; the single consumer also shares
its requested CPU with several producers. The receipt supplies that placement
and outcome, but no scheduling or PMC causal attribution was proven. A long
finite completion interval is not itself proof of infinite livelock.

The fixed-rate and burst lanes retain original intended times, so the difference
between native and offered quantiles exposes load-generator/queue delay rather
than resetting a late request's clock. The retained-view lanes show the cost of
holding real capacity; cleanup never steals a token to improve progress.

The 1-MiB native/Python contrasts show substantial run-to-run/condition variability.
For example the Clang NCQ native-helper cell's marginal median is about52.0GB/s,
whereas GCC's corresponding cell is about2.44GB/s. The eight-block pool is8MiB,
below the reported32-MiB shared L3. **This is not a controlled compiler speedup,
DRAM bandwidth certificate, or reason to drop an unfavorable row.** The runs have
short finite populations, different scheduling/cache histories and included
control tails. All raw denominator/counter evidence remains. The Python lanes
include every public FFI call; helper and per-byte Python work are not conflated.

The 128-MiB payload-pool cases exceed the exposed32-MiB LLC capacity, but that
capacity comparison alone does not prove DRAM saturation or actual page locality.
Their sixteen-message traces are useful functionality/boundary experiments, not
a long-duration memory-bandwidth qualification.

## 7. Verification ledger and retained failures

| Lane | Observed result | Scope |
|---|---|---|
| Strict GCC and Clang builds | PASS | New topology/matrix executables, bridge, inherited libraries/benchmarks |
| Native topology parser tests | PASS | Sparse IDs, units, overflow, duplicate/null/bounds rejection |
| Rooted topology and placement tests | 21 tests PASS | Explicit synthetic trees, visible ancestors, missing/malformed data, L2 type and NUMA consistency |
| Matrix verifier tests | 50 tests PASS | Actual native replay fixture, tampered timing, topology, membership, token state, campaign completeness and optimized Python |
| Final GCC matrix | 93/93 records PASS | 729,216 measured messages |
| Final Clang matrix | 93/93 records PASS | 729,216 measured messages |
| Native regressions, each compiler | PASS | Core68, mutation512, IPC100k SPSC and NCQ4/4, adversarial8, limits4, chaos3, parser1M, benchmark tools |
| Existing Python baseline, each compiler | 36/36 PASS | Binding bytes unchanged; not full Turn10 hardening rerun |
| Existing PMC verification | 39 tests and1,004 math cases PASS | Inherited math/event paths, not new hardware PMC measurements |
| New C ASan+UBSan | Topology tests and six matrix histories PASS | Leak detection enabled in these C processes; asymmetry,32workers,burst and retention included |
| Native retention watchdog negative | Expected exit1, zero completed result JSON | Deliberately oversized hold, not a successful low-throughput trial |
| Cache v2 compatibility smoke | Four records replayed | New topology metadata did not invalidate arithmetic replay |
| New native Apple topology/matrix | NOT_RUN | Requires actual SDK and target execution |
| Real cross-NUMA/P/E/DRAM/PMCs | NOT_CERTIFIED | No such physical campaign supplied here |

The first combined execution command outlasted its external120-second window
after sixty completed records. It has no final completion marker and is retained
as `gcc_outer_timeout_01`; it is not mixed into the admitted population. The cause
of overall duration is not diagnosed as a queue defect. The subsequent complete
campaign used the bounded per-condition controller without that short outer
window and completed independently.

An initial Python matrix smoke incorrectly acknowledged native cleanup before
reaping the managed child. The authority had already dropped that process record,
so reaping returned BAD_IDENTITY. The driver now reaps before acknowledgment. Its
failed log and partial artifacts remain. The specifically identified96-KiB test
object was removed only after that owned failed cohort terminated, with the
cleanup action recorded. This was a new test-driver sequencing error, not a
change to the ring's lifecycle contract. Later complete Python cohorts returned
cleanly and reconciled all tokens.

Initial strict-build warning failures and intermediate smoke/source revisions
are retained. A late verifier refinement rejects instruction-only L2 locality
and ambiguous NUMA maps; its earlier tool sources are preserved under
`driver_history`, and both full raw campaigns were replayed with the strengthened
oracle. The relevant live cache maps were Unified and unambiguous in both runs;
this changed no compiled queue/benchmark operation or admitted observation.

## 8. Hardware comparison and operational claim policy

`docs/HARDWARE_BOUNDS.md` replaces unqualified summary language with a table of
exact historical metrics and evidence labels. The Xeon Turn7 data used a
four-CPU-time container and a specific100M instrumented RTT/no-per-message-clock
goodput harness. The Apple statements describe bare metal and reported
measurements. The earlier AMD container helper rates belong to yet another
workload/host. Those datasets are neither merged nor called a controlled
architecture comparison.

The approximately38.5x and34.6x ratios compare reported **p99.99** values, not
maxima. RTT/2 is not direct one-way latency. Cache-counter RMW/s is not message
rate. Payload GB/s is bytes delivered once, not summed load/store or bus traffic.
A Python native-helper result is not a Python-bytecode serialization rate.

The reported22.66/22.79GB/s Apple1-MiB outcomes are retained as LAB_REPORTED, but
this turn does not possess all raw inputs to reconstruct their exact capacity,
placement, compiler/CPython identities and timing denominator. Therefore no
precise new universal operating envelope is fabricated for them. The new matrix
creates those explicit inputs for future measurements rather than filling old
provenance gaps with a plausible narrative.

For Apple, aggregate P/E and cache information is exposed through sysctl; it is
not direct worker binding. For Linux AMD/Intel, exposed package/die/SMT/cache/NUMA
relationships must be read from the actual system, not predicted from the vendor
name. Socket, NUMA, cache-sharing and core domains need not coincide. Virtual
CPUs and hidden host constraints further limit the meaning of a topology table.
These are reasons to retain evidence, not reasons to suppress measured failures.

## 9. Reproduction and deployment

From a new extracted source directory:

```sh
python3 tools/verify_release.py --strict
make CC=clang BUILD=build/matrix benchmarks libraries python check-topology check-matrix
python3 tools/run_matrix.py --build build/matrix --out matrix-results \
  --trials 3 --count 20000 --small
python3 tools/verify_matrix_results.py matrix-results \
  --binary build/matrix/bench_matrix --source-root .
```

GCC is selected with `CC=gcc` and a separate BUILD directory. On Darwin the
Makefile retains `_DARWIN_C_SOURCE` and the existing target/strict-LDAR selection.
An accepting compiler/SDK and actual hardware execution remain required; an
`-mcpu` option does not identify the machine. Optional Python needs its matching
CPython development headers. Free-threaded/subinterpreter configurations remain
outside the inherited admission boundary.

`--plan-only` creates a reviewable plan without measurements. Actual runs require
a fresh output path. Removing `--small` increases the large-payload quotas; a
changed plan is a separately identified experiment. Resource estimates, all
unsupported cells and the selected CPU pool are written before execution.

Offline replay of the included campaigns can omit `--binary` when the original
binary is unavailable and use `--source-root` for compile-input consistency. Do
not compare an old hash against a newly rebuilt debug binary and call the old
result corrupt. The post-package check uses the rebuilt binary for a **new** smoke
observation and separately replays archived raw data.

## 10. Acceptance boundary and next target evidence

This turn resolves the missing **mechanism** for hardware-aware claim boundaries:
OS-visible topology, explicit workload cells, negative outcomes, exact raw timing,
precise token accounting and public documentation. A09-06 no longer needs to be
handled by repeating unqualified headline ratios.

It does not close every cell in the earlier qualification plan. The runnable
contrast design is not the full Cartesian product, not five 100M restarts, not a
calibrated open-loop service guarantee, and not a dependence-aware population-tail
analysis. The six explicit unsupported cells are visible release-work items, not
passed by omission. This distinction is preserved even when every available
mathematical replay returns zero.

No diagnostic weakens SC queue metadata, release/acquire publication, the
one-token-per-endpoint rule, nonwrapping identities, or the no-timeout-reclamation
contract. There is no hard-real-time, exactly-once, persistent-power-loss or
hostile-writer guarantee. A future valid contradictory witness still rejects the
relevant implementation or claim.

## 11. Integrity and deposit scope

The detached report/ZIP sidecars authenticate literal finalized bytes. The
canonical report hash follows the header convention and is separately scoped.
`SOURCE_MANIFEST.sha256` covers source/build/test/tool files and required license
material; `MANIFEST.sha256` covers every packaged file except itself. Historical
manifests and incomplete evidence are named as history, not substituted for the
current verification result.

The detached deposit receipt records actual sizes, hashes, fresh-extraction
checks and delivery mode after those actions occur. It does not infer a remote
upload from a local filename or a Drive folder read. The optional local Mac sync
helper verifies the ZIP and every member before copying, reports LOCAL_VERIFIED,
and never claims unobserved cloud replication.

## 12. Sources and evidence custody

[P11] Verified Turn11 archive; full identity in this header. Contains the hardened
parent and the completed adversarial audit, especially its eight-row matrix in
`docs/ADVERSARIAL_AUDIT.md` sections6.6–6.7. The original ABI and verification plans
remain under `docs/reference`.

[P7] Preserved Turn7 report and evidence summaries: exact Xeon container budget,
RTT boundaries, p99.99 versus maxima and100M membership/data provenance.

[U12] Current user commissioning statement: reports Apple native Turn11 tests and
2.014B versus124.3M RMW/s. Credited as LAB_REPORTED; not a new local execution or a
source of invented raw timebase/CPU assignments.

[E12] `evidence/turn12/`: exact plans, native/Python traces, bitmaps, snapshots,
commands, identities, compiler/test logs, complete and failed campaign histories,
RESULTS_SUMMARY.json and MATRIX_RESULTS.csv. These support this report's measured
claims; the vendor manuals do not supply those measurements.

[R1] Linux CPU topology documentation and sysfs ABI. Defines exposed identifiers
and relationships, not a physical-processor attestation.
https://docs.kernel.org/admin-guide/cputopology.html
https://raw.githubusercontent.com/torvalds/linux/master/Documentation/ABI/stable/sysfs-devices-system-cpu

[R2] Linux cgroup-v2 documentation. Visible hierarchy, cpu.max, effective cpusets,
memory limits and ancestor restrictions.
https://docs.kernel.org/admin-guide/cgroup-v2.html

[R3] Apple XNU kern_mib.c. Exposed performance-level names, counts and cache
queries. Consulted live source, not the exact SDK shipped with a user's Mac.
https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/bsd/kern/kern_mib.c

[R4] Apple XNU thread_policy.h. Affinity tags are cache/scheduler relationship
hints, not a universal P-core pinning interface.
https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/osfmk/mach/thread_policy.h

[R5] Linux sched_setaffinity manual. Per-thread CPU execution constraints and
readback; not exclusive core allocation or page placement.
https://man7.org/linux/man-pages/man2/sched_setaffinity.2.html

[R6] Linux NUMA memory policy. Memory-placement scope differs from CPU affinity.
https://docs.kernel.org/admin-guide/mm/numa_memory_policy.html

[R7] Linux false-sharing analysis. Cache-line interaction, tooling and causality
are distinct from a capacity comparison or one generic timing ratio.
https://docs.kernel.org/kernel-hacking/false-sharing.html

Mutable primary documentation supports interface choices. Executed binary,
workload and host evidence define each measured claim's actual scope.
