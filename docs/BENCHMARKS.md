# Benchmark families and hardware boundaries

Turn 12 adds `bench_matrix` and native OS-visible topology discovery. Start with
[MATRIX_VERIFICATION.md](MATRIX_VERIFICATION.md) and [HARDWARE_BOUNDS.md](HARDWARE_BOUNDS.md)
for exact workload cells, raw-tick and token replay, and comparison limits.
The source/data-path distinction below remains: **RTT, timestamped one-way
service, uninstrumented-throughput, Python helper/bytecode, and cache controls
are separate experiments**. Matched quantiles from unlike environments are
descriptive observations, not universal hardware ratios.

The following preserved Turn 7 instructions still run. Turn 12 additionally
emits `hardware_topology` in their JSON headers and a startup topology sidecar;
new setup diagnostics do not make old archived records contain missing metadata.

# Turn 7 — Native RTT, validated goodput, and cache diagnostics

These executables exercise the unchanged LE128-V1 public API. They do not
implement another queue, a socket payload path, timer-driven slot reclamation,
or P-core pinning disguised as an affinity tag. The single native-library
change from Turn 6 is the documented Darwin creation-permissions fix.

## Build

From the extracted project directory on Linux, with GCC or Clang:

```sh
make CC=clang BUILD=build/native benchmarks check-bench-tools
```

On a native Apple Silicon Mac with an installed macOS SDK:

```sh
make CC=clang BUILD=build/apple benchmarks check-bench-tools
```

An explicit M4 candidate, only on an M4 and an accepting compiler:

```sh
make CC=clang BUILD=build/m4 \
  ELITE_ARCH_FLAGS='-arch arm64 -mmacosx-version-min=14.4 -mcpu=apple-m4' \
  ELITE_STRICT_LDAR=1 benchmarks check-bench-tools
```

Use a fresh build directory when changing flags or toolchains. The Makefile
preserves the core's strict warnings, `-O3`, no LTO, and strict LDAR target
feature selection on Darwin. Benchmark ordering instructions do not alter the
core's release/acquire or sequentially consistent operations. Native Mac
execution of the new benchmark must be established on that machine.

## Whole sweep

Use an output directory that does not already exist. The default count is
100,000,000 **per condition and per trial**, after 1,000,000 warmup operations.

```sh
python3 tools/run_benchmarks.py --build build/apple \
  --out apple-turn07 --count 100000000 --warmup 1000000 --trials 1
python3 tools/verify_results.py apple-turn07 \
  --analyzer build/apple/bench_analyze
```

This runs two RTT conditions (SPSC and NCQ, each 1P/1C in each direction), then
SPSC 1P/1C goodput and NCQ 1/1, 2/2, 4/4, and 8/8 goodput. `--trials 5` creates
five fresh cohorts for every condition, not five pieces of one 100M trial.
It still does not implement Turn 5's separate one-way/open-loop qualification
or its dependence-aware population-tail inference.

For a shorter functionality run, change `--count 100000 --warmup 10000`.
For Linux, append an actually permitted CPU list, for example
`--cpus 0,1,2,3`; do not assume that example is allowed on another host.
Each worker is assigned round-robin and reads its one-CPU mask back before and
after work. The controller is not pinned to a dedicated isolated CPU. Multiple
workers may share a CPU; list cardinality and worker count remain visible.

Darwin `--qos user-initiated` requests QoS and records the returned error.
`--affinity-tag 1` invokes `thread_policy_set` and `thread_policy_get`, recording
both errors and tag readback. These are scheduler/cache relationship hints in
a task-local namespace, not CPU identifiers. Separately spawned processes do
not gain a common affinity set by using the same number. No output from this
program claims P/P, P/E, or cross-cluster isolation without additional external
placement evidence. The Darwin path rejects `--cpus` rather than silently
pretending to apply a Linux mask. Linux rejects Darwin-only requests.

## Individual executables

```sh
build/apple/bench_latency --mode spsc --count 100000000 \
  --warmup 1000000 --out rtt-spsc
build/apple/bench_latency --mode ncq --count 100000000 \
  --warmup 1000000 --out rtt-ncq
build/apple/bench_throughput --mode ncq --producers 8 --consumers 8 \
  --count 100000000 --warmup 1000000 --out goodput-ncq-8
```

SPSC admits exactly one producer and one consumer. The throughput executable
also supports asymmetric NCQ counts and 16P/16C when K <= N. The default sweep
implements the newly requested 1,2,4,8 counts. A producer quota divides count;
warmup also must divide the producer population. Each endpoint holds at most
one token. POLL_ONLY, checksum NONE, 64 valid payload bytes, N=1024 are the
primary configuration. No ring batching or unchecked internal fast path is
substituted. `--capacity` permits other valid powers of two up to 65536.

## RTT definition and semantics

`--count 100000000` means 100,000,000 **complete round trips**, hence 200,000,000
measured directional payload records, plus warmup. Two distinct POSIX shared
memory rings connect two separately exec'd workers, with four endpoints.
Each round trip starts on the origin immediately before its first public
request reservation attempt and ends on that same origin after it has checked
all response bytes/metadata **and completed response release**.

The responder checks and releases the request, then directly constructs the
response in its own writable reply lease. There is no intermediate copied
transport message. The reply is deterministically derived from the request
identity; it is not advertised as a reference-counted forwarding mechanism.
Only one request awaits a response. Any scheduling delay, polling, contention
inside the native calls, responder work, and timer-boundary cost lies in the
measured interval. NCQ RTT is not a multi-client contended result. A contended
RTT service would need correlation/routing for replies and a separately
specified workload.

Record names are explicit: `roundtrips`, `directional_records`, raw RTT
quantiles. No value is divided by two and called one-way latency. The frozen
15-ns median / 50-ns p99 one-way goals are **NOT_MEASURED** by this harness.
Closed-loop RTT also does not represent an open-loop service deadline tail.

## Timing and perturbation

Darwin uses `mach_absolute_time()` plus the actual nonzero
`mach_timebase_info` numerator/denominator. Linux uses
`clock_gettime(CLOCK_MONOTONIC_RAW)`, with clock resolution reported separately.
The code stores raw integer ticks and converts differences with checked
quotient/remainder arithmetic. Nanosecond summary values round upward, not to
fabricated fractional resolution.

The AArch64 boundary uses compiler ordering, DSB ISHLD and ISB before the OS
clock read, then ISB and compiler ordering afterward. The x86-64 boundary uses
compiler ordering and LFENCE around the OS clock read. These conservatively
instrumented boundaries include their own cost. There is no median-overhead
subtraction. Neither a returned scale nor the smallest observed clock-pair
delta certifies actual resolution or a five-nanosecond uncertainty budget.

One million ordered empty clock pairs are collected before warmup and after
the measured phase. Their raw unsorted deltas are retained. The origin stores
all starts and durations in preallocated, prefaulted private arrays, not in a
transferred slot. Raw arrays are written only after all measured workers have
stopped, then sorted for exact nearest-rank p50, p90, p99, p99.9, p99.99, and max.
No samples, negative results, or scheduler tails are trimmed. A backward clock
is an error rather than an unsigned wrap or a discarded observation.

`sampling_profiler_enabled:false` means no sampling profiler is installed by
the primary run. It does not mean interrupts are disabled, the machine is
isolated, `clock_gettime` is always a vDSO path, or timestamp/log-array writes
have zero overhead. getrusage observations outside the inner measured loops
record faults, CPU time, and context switches. The controller's poll watchdog
and the OS still consume resources. Precise sub-50-ns certification requires
further native clock/binary admission, not merely a displayed percentile.

## Goodput and exact membership

Every producer constructs all eight 64-bit words directly. Every consumer
checks all eight words and API metadata under its lease, then returns the token.
Producer p owns a disjoint quota. Warmup has a disjoint identity range and seed;
it completes in the same generation, with no cursor/epoch reset.

A consumer keeps an M-bit private bitmap for the entire measured ID universe.
Its own repeated bit fails immediately; the offline pairwise-disjoint merge
catches duplicates across consumers. The union must contain exactly [0,M), with
no high trailing bits, matching producer quotas and consumer populations.
Raw consumer bitmaps and their union are exported after timed work, permitting
independent bounded-memory replay. All shared free/ready states are reconciled
only while participants are quiescent or detached. Historic NCQ entry values
outside the live interval are not treated as extra tokens.

A future common start time has a 100-ms preparation lead. Consumers run until
all producers have reported completion, the separate benchmark-control drain
word has been acquired, and a **second** queue read observes empty. The second
read is necessary: an earlier stale empty snapshot cannot justify stopping.
Workers complete all their outstanding reads/releases before reporting.

Primary goodput = M / (latest consumer drain-quiescent tick - scheduled start).
This includes actual start skew, validation, bitmap writes, final empty
observation and local control-tail delay. It excludes offline file writes and
bitmap merging. GB/s is decimal **delivered application payload bytes/s**, not
DRAM/interconnect bandwidth, total bus traffic, or messages replicated twice.
Throughput is not inverted to produce a latency distribution.

## Evidence format and memory requirements

A trial directory contains `result.json`, warm/final reconciliation JSON,
clock and worker records, and raw files. The runner saves exact command lines,
binary SHA256, source SHA256, cohort status, and an enclosing
`RUN_MANIFEST.sha256`. Source and binary files must not change while a campaign
runs. A manually run native executable supplies its data but not the runner's
extra source/build provenance; use the runner for audit campaigns.

RTT binary files are headerless little-endian unsigned 64-bit arrays:

| File | Length | Meaning |
|---|---:|---|
| start_ticks.u64le | 8M | Entry tick indexed by request ID, in original order |
| rtt_ticks.u64le | 8M | Response-release endpoint minus that entry, original order |
| clock_before.u64le | 8C | Ordered empty-pair deltas before warmup |
| clock_after.u64le | 8C | Ordered empty-pair deltas after measured work |

M and C, clock scale, mode, interval semantics, and placement are in JSON.
For M=100M,C=1M, raw RTT data uses **1,616,000,000 bytes per profile**.
The origin holds these arrays during collection. Its libc sort may require an
additional 800,000,000-byte temporary buffer; account for roughly 2.4GB plus
library/OS overhead, not only the small ring. An offline C analyzer processes
one duration array at a time and streams entry ticks. Insufficient memory or
output capacity fails; no silent sampling/spill replaces the requested trace.

A throughput consumer bitmap uses ceil(M/8) bytes. Eight consumers at 100M
use 100,000,000 private bytes, plus a 12,500,000-byte saved union and offline
merge buffers. The full default seven-condition sweep uses about 3.5GB of raw
evidence per repetition. Five repetitions scale that disk requirement by five.

The code writes an explicit failure marker and returns nonzero on invalid data,
child failure, incomplete native phase, exhausted memory, file failure, or
watchdog expiry. The runner keeps all previous results, marks the campaign
FAILED_OR_INCOMPLETE, and stops rather than choosing a favorable retry.
The controller watchdog is per trial from setup, default 600 seconds, and
covers ordinary native collection and waiting for raw export/analysis results.
It cannot supply a realtime deadline for a kernel stuck in uninterruptible I/O.
Use a larger explicit value for a slow target; never silently count incomplete
work as 100M completion.

## Offline replay

```sh
python3 tools/verify_results.py apple-turn07 --analyzer build/apple/bench_analyze
```

The verifier checks the runner's manifest if present and rejects a failed
campaign or any FAILED marker. It checks exact file sizes, raw RTT ranks,
monotone sequential starts, calibration ranks, exact bitmap membership and
matching union/counts. Do not run the verifier with Python -O. RTT rank replay
uses the same small C statistics routine as the online summary; the unit test
independently fixes known ranks and rounding cases. This is raw-evidence
consistency validation, not an independently different queue implementation
or a distribution-free population-tail proof.

## Cache and hardware-counter diagnostics

The frozen headers, entries and descriptors remain full128-byte cells. Static
assertions plus canonical runtime stride/address checks establish designated
64/128-byte line separation under the admitted granularity assumptions. The
same logical MPMC cursor is deliberately shared and contended. Cache misses,
prefetch effects, migration, or generic event totals cannot by themselves prove
that all observed traffic is or is not false sharing.

Run a separate sensitivity control with isolated versus packed independent
atomic counters. It never changes a live ring layout:

```sh
build/apple/bench_cache --producers 8 --consumers 8 --count 10000000 \
  --trials 5 --out cache-sensitivity
```

This command has eight counter workers (the equal producer/consumer options
select its diagnostic population), not sixteen IPC endpoints. Packed counters
have8-byte stride; isolated counters128-byte stride. Order alternates across
trials. Counter totals must reconcile, but no fixed speed ratio is a pass gate.
It is a thread-only RMW control, not transport goodput. Its work is iteration
bounded; unlike the IPC controllers its current standalone path does not
implement the parsed per-trial timeout. Use an external owned-process watchdog
for unattended hostile-scheduling tests of that optional control.

Discover actual profiler availability in a new separate directory:

```sh
python3 tools/profile_counters.py --out counter-discovery
```

On Darwin, read the installed-template list, then pass the exact installed
CPU-counter template name with `--template` and a native workload after `--`.
The tool records the trace and marks it for scope review. Inspect Instruments
for the spawned worker PIDs, per-CPU/P/E attribution, event definitions,
multiplexing and sample interval. A launch-scoped trace might record only the
controller; that does not characterize the workers. There is no hardcoded M4
PMU event table, private kpc dependency, or invented P-core placement.

On Linux the script uses perf stat, when installed/authorized, with explicit
cycles/instructions/cache/context-switch/fault events. Retain unavailable
and multiplexed event records; an unavailable event is not zero. Repeat small
nonmultiplexed groups when needed. Diagnostic runs are never merged into the
unprofiled latency distribution.

## What this turn does not establish

The new executables do not perform the frozen one-way/open-loop measurement,
cross-domain clock-handshake or five-nanosecond uncertainty qualification,
moving-block bootstrap, full16P/16C qualification sweep, every chaos schedule,
or safe arbitrary-authority takeover. They preserve the native queue as the
subject of those later experiments. A100M result is a finite exact membership
and measured-duration record, not an all-execution lock-freedom proof or native
Apple observation made on a Linux host.

Counter-collection scope is the wrapped command's lifetime unless the installed
profiler is configured more narrowly. That includes setup, warmup, validation,
raw export, and offline sorting. Do not divide those totals by measured messages
and label the result exact steady-state queue cycles. A future interval-gated
collector must bind its enable/disable points and worker coverage before making
that narrower attribution. This script is an honest collection/discovery adapter,
not a completed P-core cache-miss analysis.
