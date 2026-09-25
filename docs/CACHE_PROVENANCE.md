# Cache-control v2: measurement and replay contract

This is a **threaded independent-counter control**, not the SPSC/NCQ message benchmark. Neither the shared-memory ABI nor Python lease code is modified. The optional `libelite_pmc.a` is not linked into `libelite_ringbuffer`.

## Run and verify

Build with an explicit compiler and a new build directory:

```sh
make CC=clang BUILD=build/profile benchmarks check-pmc
python3 tools/run_cache_profiling.py --binary build/profile/bench_cache \
  --out cache-v2 --workers 1,2,4,8 --iterations 5000000 --trials 5 \
  --modes off,count
python3 tools/verify_cache_provenance.py cache-v2/count-4 \
  --binary build/profile/bench_cache --source-root .
```

The runner's output directory must not already exist. The individual benchmark accepts `--pmc off|count`, `--virtual-counter off|probe`, `--producers N --consumers N`, `--count`, `--trials`, `--timeout`, and the inherited placement options. For this control, **N is the worker count**, not 2N; the equal P/C options preserve the existing CLI shape. Only this control interprets them this way. `--count` is per-worker increments. There is no warmup; JSON explicitly records zero. `--warmup` from the generic CLI is not a request to reset or warm a ring.

Linux may add `--cpus 0,1,2,3` only when those CPUs are in its allowed set. They are assigned round-robin and masks are read back. Darwin may add `--qos user-initiated --affinity-tag 1`. A Darwin tag is a request, not a P-core identifier. Do not pass a Linux CPU list to Darwin.

On Apple ARM64, `--virtual-counter probe` first executes `CNTFRQ_EL0`/`CNTVCT_EL0` reads in an owned disposable child, before creating threads. Failed/trapping access is recorded, not recovered in a live measurement thread. This optional diagnostic is a **system timer**, not a core-cycle PMC. The primary duration always uses `mach_absolute_time` and its own timebase. The library's direct-register functions have an admitted-access precondition; embedding applications must not call them blindly.

`make pmc` builds only the optional archive. `make install-pmc PREFIX=...` installs its archive/header explicitly. Production core installation remains separate. Building benchmark provenance uses Python's standard library; running the resulting C benchmark does not require Python. Git is optional; the source snapshot and executable SHA-256 are retained without it.

## Primary schema: `elite-cache-control-v2`

Every `cache-N-workers-stride-S-trial-T.json` is self-contained for the mandatory arithmetic checks. A directory also has `cache-campaign.json`, emitted only after all expected pairs complete, and `machine.txt`. The Python runner adds commands, host observations, validation output, paired summaries, and a run manifest.

| Field | Meaning |
|---|---|
| `t0` | Scheduled future start in the primary raw clock domain |
| `end` | Maximum of the worker `end_tick` values |
| `delta_ticks` | Exact integer `end - t0`; no modulo wrap |
| `timebase_numer`, `timebase_denom` | Nanoseconds per primary tick, as an exact positive ratio |
| `elapsed_ns` | Decimal export of the exact rational `delta_ticks * numer / denom` |
| `operations_per_second` | Decimal export of `workers * iterations * 1e9 * denom / (delta_ticks * numer)` |
| `reported_resolution_ns` | Linux `clock_getres(CLOCK_MONOTONIC_RAW)`; **null on Darwin** |
| `nominal_tick_ns` | Scale of one tick; not measured update granularity or uncertainty |
| `resolution_status` | Explicit distinction between reported resolution, scale, and missing resolution |
| `binary` | Executable path, size, SHA-256, and successful after-run rehash |
| `build` | Compiler path/version/hash, flags, optional Git commit, source-file hashes and snapshot hash |
| `per_worker` | Raw worker timestamps, exact final counter, placement, event frames, CPU accounting and optional timer pairs |
| `metrics` | Supported diagnostic estimates only; missing events produce null, not zero |

Timestamps and counters are JSON integers. Consumers must not parse 64-bit integers through binary64 before subtraction. JSON decimal estimates may be decoded as binary64; the verifier compares them to independently reconstructed rational values within one ULP. Exact integer software-counter and identity checks have **no tolerance**.

Darwin `mach_timebase_info` returns a conversion ratio, not a reported effective clock resolution. Rather than equating these, the schema sets `reported_resolution_ns=null` and exposes `nominal_tick_ns` separately. No fixed 24-MHz assumption appears in the implementation.

## Windows and overhead

After placement and event-open setup, every worker reports ready. The coordinator publishes t0 approximately 100 ms in the future. At/after t0, each worker takes a broader window start, reads CPU accounting and optional timer brackets, enables its PMC groups, records `start_tick`, performs exactly the specified relaxed atomic increments, and records `end_tick`. It then disables/reads PMCs, samples accounting/timer diagnostics, and closes its window. JSON export and self-hash rechecking occur after all workers finish.

The cohort rate uses t0 through the **latest RMW completion**, not the sum or average of worker durations. Late starts, preemption and pre-loop enable overhead therefore remain visible. Per-worker start/end and enclosing window values show the distinction. PMC groups enable/disable sequentially around the RMW loop: their exact counted instruction envelopes can include timing and other group-control overhead. They are not represented as identical to the wall-time denominator or as exactly one isolated instruction loop. No profiler-created sampling interrupt is requested. Ordinary OS interruptions still exist.

The process accounting envelope starts before the future-start wait and ends after worker joins. It includes coordinator/watchdog activity. Never sum it with thread accounting or relabel it as queue-only work. Thread CPU utilization uses cumulative user/system microsecond differences divided by the enclosing worker window. Accounting granularity can yield zeros or apparent values over 100% in short windows; these are not clipped.

## Linux hardware counters

Three independent pairs are opened per worker with `pid=0`, `cpu=-1`, no inheritance, user-only counting, close-on-exec, and no sampling period:

1. CPU cycles and retired instructions.
2. L1D READ ACCESS and READ MISS.
3. Last-level cache READ ACCESS and READ MISS.

The cache selector is `cache | (READ << 8) | (result << 16)`. These are Linux generic selectors; actual hardware semantics and availability remain target-dependent. **They do not count all coherence invalidations, RFOs, ownership transfers or stall cycles.** No number is manufactured for these missing mechanisms.

A pair's group read contains `[nr, time_enabled, time_running, value0, id0, value1, id1]`. Receipts preserve before/after arrays and IDs. The decoder rejects wrong lengths, duplicate/missing IDs, decreasing values/times and impossible scheduling intervals. IDs, not array positions, associate values. Counter reset does not justify assuming enabled/running times reset; deltas use both reads.

For raw count difference c, enabled difference E and running difference R:

- R=0: `NEVER_SCHEDULED`; no numeric event estimate.
- 0<R<E: `MULTIPLEXED`; estimate `c*E/R`, retaining both raw quantities and times.
- R=E>0: `OK`; no scaling needed.

Scaling is an estimate of unsampled execution, not proof of stationarity. Ratios within one scheduled pair have a common schedule; sums across workers/groups retain different execution windows and scaling assumptions. The combined metrics are absent unless every required worker has the corresponding pair. Malformed frames fail the verifier even when software RMW counters reconcile.

Open denials preserve errno and paranoid observations. `RESTRICTED_PARANOID` requires an access-denial errno and a value above 2; at 2, upstream policy permits user-only per-thread profiling and a denial is conservatively `RESTRICTED_ACCESS`. No single errno/paranoid pair uniquely excludes other access-control causes. Unsupported event requests, resource failures, read failures and never-scheduled groups are distinct statuses. No sysctl, capabilities or security policy are changed.

## Darwin diagnostics

The generic six-event Linux backend is `UNSUPPORTED_PLATFORM` on Darwin. Supported code paths instead collect:

- `mach_absolute_time` and `mach_timebase_info` for the primary interval;
- optional `CNTVCT_EL0` brackets, `CNTFRQ_EL0`, and independently queried `hw.tbfrequency`;
- `THREAD_BASIC_INFO` thread user/system times and its scaled instantaneous CPU usage;
- `getrusage(RUSAGE_SELF)` whole-process accounting;
- `proc_pid_rusage(RUSAGE_INFO_V4)` whole-process OS-accounted cycles/instructions, when available.

The verifier derives a separately labeled process-envelope instructions/cycle ratio from valid nondecreasing process snapshots. This is neither a per-worker PMU reading nor a substitute for unavailable cache events. No private kpc interfaces, undocumented raw M-series event numbers, or privileged PMU-register access are introduced.

Timer correlation brackets an observed CNTVCT read between two Mach reads. For two samples, it compares the CNTVCT delta converted with CNTFRQ against the outer/inner Mach elapsed bounds, allowing two nominal ticks of each timer as a declared **compatibility tolerance**. The result is a boolean or null, not an absolute metrology certificate. A false result remains visible. A differing sysctl frequency is recorded independently rather than overriding a register value to force agreement.

## Verifier and falsification

```sh
python3 tools/verify_cache_provenance.py RUN_DIRECTORY
python3 tools/verify_cache_provenance.py ONE_RECEIPT.json --binary EXACT_EXECUTABLE --source-root .
python3 tools/falsify_cache_provenance.py --receipt ONE_RECEIPT.json --out NEW_NEGATIVE_DIRECTORY
```

A valid unavailable-counter status does not fail the benchmark or arithmetic replay. A mismatch of any required timestamp equation, counter, worker population, event ID, scaled estimate, or declared metric exits 1. The verifier uses explicit checks, not removable `assert`; it is also tested under `python -O`. Duplicate keys, nonfinite values and boolean-as-integer inputs are rejected. A campaign missing a record or completion marker cannot pass.

`PASS_ARITHMETIC_AND_COUNTER_RECONCILIATION` means internal receipt consistency. It does not authenticate a dishonest producer of evidence, prove effective clock resolution, certify the compiler from flags alone, or prove that a cache miss caused a slowdown. `--binary` and `--source-root` verify external bytes against the recorded identities; they never execute paths inside the receipt.

The 8-byte/128-byte ratio is computed per alternating trial pair before taking its median. The median of ratios is not necessarily the ratio of marginal medians. One-worker controls expose nonsharing baseline variation. Hardware locality, oversubscription, CPU quotas, frequency and scheduler behavior remain part of the experiment. The old 113.8x claim needs its own raw v2 rerun; it is not backfilled from new timings.
