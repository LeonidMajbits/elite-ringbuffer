# Hardware claim boundaries — Turn 12

## What the topology detector establishes

`elite_topology` is a setup-time, read-only description of **OS-visible** topology.
It is not physical-hardware attestation, a scheduler, a memory-placement library,
or a changeset to the shared-memory ABI. It introduces no dependency in the
production ring library. `bench_topology` prints `elite-hardware-topology-v1`.
The matrix embeds before/after snapshots; older benchmark receipts now also
include `hardware_topology`, and their startup directory receives a snapshot.
Snapshots are collections of observations, not an atomic view of hotplug state.

On Linux, the authoritative execution eligibility is the caller's current
`sched_getaffinity` mask. Sparse CPU IDs are retained; a count is not converted to
an invented `0..count-1` map. Cache/core entries cover eligible CPUs. Online CPUs,
node maps, package/die/core/cluster IDs and SMT siblings describe the exposed OS
view. Cache entries retain the reported level, type, size, line size, ID and
shared-CPU set. One processor family's socket, die, NUMA and LLC domains must not
be substituted for another family's domains by vendor name.

The detector resolves the process's cgroup-v2 path through mountinfo, walks its
**visible** ancestors, and records quota/period, memory limits/current use and
effective cpusets. A child's unlimited quota does not erase a constrained
parent. Missing/v1/unresolved views are not called unlimited. Container markers
and a reported hypervisor flag yield `TOPOLOGY_VIRTUALIZED_CONTAINER`; absence of
those indicators does not prove bare metal. Hidden ancestor quotas, hotplug,
other processes and hypervisor contention remain possible. The Linux
`physical_cpus` field counts known `(package,die,core)` tuples **within the eligible
set**, not the machine's marketed core count. Unknown IDs make that count null.

On Darwin, numeric sysctls are queried with actual return widths. Perf levels
are enumerated from `hw.nperflevels` and classified by their **reported names**:
`Performance`, `Efficiency`, or unknown. Index zero is not hardcoded as a P-core
selector. Aggregate mixed P/E geometry can produce `TOPOLOGY_CLUSTER_MIXED` for
the host; it does not classify an individual worker. Logical/physical counts,
per-level L1D/L2/L3 sizes and CPUs per L2, system L3, page size, physical memory,
translation status, memory frequency and channels are retained when actually
available. A missing sysctl has a null value and its error; no memory channel
count or frequency is invented from a model name. The new Darwin discovery and
matrix branches still need native SDK build and execution of this exact release.

Discovery uses explicit **safety caps**, not guesses about hardware: CPU IDs below
1024, 16 cache entries per CPU, 256 nodes, 16 perf levels, 64 visible cgroup
ancestors, and bounded file reads. Exceeding these limits marks partial or
unavailable data; a truncated prefix is not labeled a complete topology.

## Placement is not residency

Linux worker CPU requests are set and read back before/after the measured phase.
This demonstrates those constraints at the observation points under the trusted
OS contract. It does not give exclusive CPU service, prove no mid-run change,
or identify the physical processor behind a vCPU. A `same_l2` result needs a
reported common data/unified L2 shared-CPU map. `cross_numa` requires all producer
CPUs and consumer CPUs to belong to known, disjoint OS-reported NUMA node sets.
The latter is **CPU topology only**: the initializer first-touches the backing,
and neither `mbind` nor physical page residency is measured here.

Mach QoS and affinity tags remain requests/hints. They are not P-core numbers.
Aggregate sysctls do not supply a per-worker P/E map. The planner therefore keeps
`intra_perf` and `perf_to_efficiency` as explicit unsupported cells rather than
running an unqualified cohort under a false label. A future trace-based placement
adapter needs its own contract; it must not relabel a tag readback as pinning.

The verifier computes payload-pool bytes, exact mapped-segment bytes, minimum
observed L2 capacity and deduplicated reported last-level data-cache capacity
where the view is complete. `pool_capacity_below_all_reported_l2` is a capacity
comparison, **not observed cache residency**. A payload exceeding an exposed LLC
is not evidence that the measured path saturates DRAM. System caches not exposed
through the OS, cache conflicts, active subset, prefetching, page placement and
CPU off-time all matter. Both residency and DRAM saturation stay NOT_ESTABLISHED.

## Metric contracts: do not merge them

| Metric | Exact scope |
|---|---|
| Historical Turn 7 RTT | Two rings, request and reply; one origin-clock interval through final reply release; 100M RTTs per named run |
| Turn 12 native-entry latency | Consumer's required full-payload-check tick minus producer's first native-reservation-attempt tick |
| Turn 12 offered-service latency | Same consumer read tick minus the original scheduled offer; generator delay/backpressure are retained |
| Turn 12 return-complete latency | Consumer release-completion tick minus producer entry; includes deliberate retention |
| Turn 12 cohort goodput | Complete measured message count divided by maximum consumer drain-quiescent tick minus scheduled start |
| Turn 11 cache control | Independent relaxed atomic increments; **not messages**, IPC latency, or interconnect byte traffic |
| Python helper lane | Python public ctypes/lease/view operations; compiled helper constructs and checks bytes; no native bulk queue loop |
| Python bytecode lane | Python loops touch every byte of a small payload; not a renamed helper bandwidth result |

Every Turn 12 transfer has timestamps and exact membership bookkeeping. Those
costs remain inside the workload and can perturb subsequent messages. It is not
the uninstrumented Turn 7 goodput lane. Raw OS ticks and conversion ratios are
preserved, but absolute timing uncertainty is NOT_QUALIFIED. RTT/2, reciprocal
throughput, a clock's nominal tick and one ULP arithmetic agreement cannot certify
one-way latency or the earlier 15/50-ns targets. There is no zero-latency claim.

## Historical comparisons: descriptive and explicitly unmatched

The historical records refer to different experiments and host budgets:

| Metric/condition | Linux Xeon container, retained Turn 7 | Apple ARM64, lab-reported Turn 8/post-release |
|---|---:|---:|
| SPSC RTT p50 | 660 ns | 250 ns |
| SPSC RTT p99 | 864 ns | 625 ns |
| SPSC RTT p99.99 | 277,748 ns | approximately 7,210 ns |
| SPSC RTT maximum | 213,535,770 ns | approximately 1,330,000 ns |
| NCQ RTT p50 | 805 ns | 375 ns |
| NCQ RTT p99 | 1,060 ns | approximately 1,250 ns |
| NCQ RTT p99.99 | 410,436 ns | approximately 11,880 ns |
| NCQ RTT maximum | 145,048,128 ns | approximately 1,840,000 ns |
| Native SPSC 64-byte goodput, 1/1 | 9,480,193 messages/s | 32,066,903 messages/s |
| Native NCQ 64-byte goodput, 8/8 | 3,575,176 messages/s | 4,105,985 messages/s |

The 38.5x and 34.6x figures compare **approximately matched p99.99 values**, not
maximum latency. They are not controlled estimates that Apple processors are
that many times faster. The Linux campaign had four CPU-time equivalents and
16 workers at 8/8. The Apple report describes bare metal. CPU models, placement,
power, concurrent load, repetitions, compiler, runtime and clock uncertainty do
not become equal by putting the numbers in adjacent columns.

The reported 22.66/22.79 decimal GB/s Python observations used 1-MiB payloads,
public Python/native buffer operations and full-byte checking according to the
lab. This turn did not receive sufficient raw Apple records to reconstruct the
exact old ring capacity, placement, compiler/CPython identities, resource regime,
or timer denominator. **No universal operating envelope or new verified Apple
row is inferred.** The new paired native/Python matrix freezes explicit capacity,
payload, caller implementation, timing and runtime identities for future runs.
The historical AMD EPYC and Xeon container results are also separately labeled,
not pooled into one x86 baseline or compared as matched samples.

## Eight-axis contrast design and remaining cells

The default plan is a predeclared contrast design, **not the Cartesian product**
of all eight audit dimensions. It exercises locality, population, working-set
capacity, arrival, retention, runtime, OS/yield observations and checksum work.
All exact combinations, quotas, resource estimates and skipped reasons are in
`matrix-plan.json` before execution. Default three repetitions are fresh ring
generations; this is not the earlier five-restart/100M qualification.

Unavailable P/E mapping and absent NUMA pairs are explicit. A special
all-tokens-held transition fixture and deliberate corruption are not silently
substituted for successful-rate samples. The existing native adversarial lane
handles selected integrity failures; host-wide memory pressure is not injected
on a shared host. Those entries remain declared unsupported here. Empty or
partial evidence, deadline expiry and failed cleanup cannot produce MATRIX_COMPLETE.

Successful exact membership and the final free-queue snapshot establish no lost,
duplicated or retained tokens **for that completed run**. They do not prove all
future schedules are deadlock-free, eliminate starvation, or make crash cleanup
lossless. Timeout never reassigns another owner's raw storage.

## Publication rule

Attach each claim to the result/plan digest, source/binary or interpreter/library
identities, exact workload, sample count, startup/end topology and actual
placement status. Keep excluded/unrun cells visible. Publish both favorable and
unfavorable results. None of these diagnostics changes the reference's memory
orders, one-token-per-endpoint invariant, managed lifecycle or Python export/GC
contract. Passing this matrix is not peer review, hard-real-time certification,
or the complete preregistered release qualification.

### Primary contracts

- Linux topology ABI: https://docs.kernel.org/admin-guide/cputopology.html
- Linux cgroups v2: https://docs.kernel.org/admin-guide/cgroup-v2.html
- Linux NUMA memory policy: https://docs.kernel.org/admin-guide/mm/numa_memory_policy.html
- Linux affinity: https://man7.org/linux/man-pages/man2/sched_setaffinity.2.html
- Apple exposed perflevel implementation: https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/bsd/kern/kern_mib.c
- Apple affinity contract: https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/osfmk/mach/thread_policy.h
- Linux false-sharing analysis: https://docs.kernel.org/kernel-hacking/false-sharing.html

These sources establish interface semantics. Local raw receipts, not the manuals,
establish this turn's finite execution results.
