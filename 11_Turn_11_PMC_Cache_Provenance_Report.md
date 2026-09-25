# Turn 11 — PMC Engine, Cache Provenance, and Timebase Reconstruction
## ELITEIPC 1.0.1 / LE128-V1 / A09-05 remediation

**Project:** Leonid Majbits / Gemini Operator Lab, paired with Leon  
**Stage:** Turn 11 — diagnostic implementation and executed falsification  
**Evidence date:** 24 September 2026 UTC; execution timestamps are preserved in each campaign.  
**Disposition:** A09-05's missing-denominator problem is resolved for newly emitted v2 records. Hardware coherence attribution and new Apple-native execution are not certified.  
**Canonical-SHA256:** `cff54b70ddff2822e00f2928e520f0310fc12f084653e716a01645e8df7107a8`
**Hash convention:** Replace only this header field's 64 hexadecimal characters with 64 ASCII zeros before hashing. The sidecar hashes all literal finalized bytes.  
**Authoritative parent:** `Elite_Systems_LockFree_RingBuffer_Turn_10_Hardened_Release.zip`; 4,095,841 bytes; SHA-256 `e1a5e4e89a1ed210fa049538b1e4cbaf5cf4a4e7f69528e399416fe454fbdb79`.  
**Destination:** Deployment Research / Elite_Systems_LockFree_RingBuffer  
**Evidence labels:** IMPLEMENTED means source exists; EXECUTED means local observations with retained logs; SYNTHETIC means a deliberately constructed test input/backend; NOT_RUN means no target execution is claimed. Historical reports and lab-reported Apple numbers are not new measurements.

---

## 1. Decision and delivered scope

**The cache rate is now reconstructable from raw integer observations, rather than trusted because the executable printed it.** The optional PMC engine preserves event selection, counting scope, kernel outcomes, scheduling coverage, raw group reads, IDs, and supported derived estimates. Missing hardware produces structured absence, not invented zeros.

The final admitted GCC and Clang campaigns contain **160 complete cache-control records and exactly 3,000,000,000 reconciled relaxed atomic increments**. Both the software counters and the raw-tick equations replay successfully. All recorded executable hashes and compile-input source hashes were independently checked against the actual local bytes. The full raw receipts, including all workers, remain in `evidence/turn11/gcc_admitted/` and `clang_admitted/`.

This does **not** establish the requested causal explanation of the historical approximately 113.8× Apple ratio. On this Linux host every required group leader returned ENOENT. Thus none of the six requested hardware event metrics could be collected. The engine was tested with native unsupported-event paths and separately with explicitly synthetic successful/error transports. It was not tested against a working physical PMU here. The new Darwin branch is implemented but has not been compiled against an Apple SDK or executed on Apple hardware in this turn.

### 1.1 Scope corrections made explicitly

| Commission wording | Implemented interpretation |
|---|---|
| “Virtual cycle counter” CNTVCT_EL0 | System virtual timer, not retired/core-cycle PMC. It never supplies the denominator of instructions/cycle. |
| hw.tbfrequency is 24 MHz | Query the actual value and independently read CNTFRQ. No hardcoded 24 MHz; mismatches remain visible. |
| Generic LLC misses measure invalidation traffic | False. Coherence invalidations and causal attribution remain unavailable unless a model-specific, properly scoped event investigation supplies them. |
| Confirm the old ratio directly from new profiling | New experiments may test the hypothesis; they cannot reconstruct missing old t0/end values or establish a cause by declaration. |
| Within one ULP | A numerical serialization/reconstruction requirement, not a one-ULP bound on physical time, clock resolution, or scheduling uncertainty. |
| CPU placement fields | Linux read-back affinity constraints; Darwin QoS/tag requests. No automatic P-core identification. |
| PMC sampling engine | Aggregate hardware counting around a repeated workload; sample_period=0, no requested sampling interrupts. |

These distinctions follow the primary timer, perf-event, and Apple observability contracts [R1–R6] and preserve the earlier plan's separate performance/counter lanes [P5]. They are not silent changes to the queue's memory model.

### 1.2 Exact input and change boundary

The Turn 10 archive was fetched from its verified Drive project folder, hashed against its published receipt, and extracted independently. Its existing release verifier passed **567 complete-file entries and 61 source entries** before work began. The audit basis is that hardened 1.0.1 archive, not an older similarly named Turn 8 local copy.

**16 inherited files under `src/`, `include/`, and `bindings/` remain byte-identical.** The new PMC header/source are additions. No existing ring algorithm, atomic order, shared layout, endpoint lifecycle, Python exporter, `_Pin`, or GC implementation was modified. `CORE_BINDING_CONTINUITY.json` lists before/after hashes. The 1.0.1 library and `0x00010000` wire version remain unchanged.

The added `libelite_pmc.a` is a separate optional diagnostic archive. It is not included in `libelite_ringbuffer.a`, `.so`, or `.dylib`. Build/provenance tools use Python's standard library; the compiled profiling code uses C11 and OS interfaces only. No private Apple kpc dependency, external perf executable, third-party arithmetic package, or privilege-changing helper is required.

## 2. Implementation inventory

| File | Responsibility |
|---|---|
| `include/elite_pmc.h` | Thread-owned context API, raw group results, statuses, CPU snapshots, optional Darwin timer/process accounting interfaces |
| `src/elite_pmc.c` | Linux perf-event opening/counting/decoding/cleanup; Darwin accounting and admitted direct timer primitives |
| `benchmarks/bench_cache.c` | v2 records, exact software counters, worker and process windows, optional diagnostics, paired layouts and watchdog |
| `benchmarks/bench_common.h/.c` | Additive clock metadata, JSON string handling and cancellation query; inherited queue benchmark behavior retained |
| `benchmarks/bench_identity.h/.c` | Native executable path and SHA-256, streamed embedded build identity |
| `benchmarks/bench_ratio.h/.c` | Portable exact rational decimal export without compiler extended integers |
| `tools/cache_build_info.py` | Compiler/flags/source snapshot baked into the executable; Git optional |
| `tools/verify_cache_provenance.py` | Independent integer/rational replay and fail-closed CLI |
| `tools/run_cache_profiling.py` | Bounded owned-process campaigns, paired summaries, host observations and run manifests |
| `tools/falsify_cache_provenance.py` | Mutations of an actual receipt; requires all invalid inputs to return exit 1 |
| `tools/check_cache_math.py` | Differential native formatter testing against Python Fraction |
| `tests/test_pmc.c`, `test_pmc_backend.c`, `test_ratio.c` | Native decoder/lifecycle/hash tests, synthetic syscall transport, exact ratio test entry |
| `tests/test_cache_provenance.py` | 39 schema/arithmetic/metadata/counter and rejection tests |
| `docs/CACHE_PROVENANCE.md` | Complete operational and evidence contract |

The report and raw test artifacts are deliberately not a substitute for the source. The header documents owner-thread and single start/stop obligations; callers inspect per-group result status separately from API call validity.

## 3. Timebase and denominator reconstruction

### 3.1 The primary cohort interval

For worker i, the relevant observations are:

- `window_start_tick[i]`: after reaching the future start, before CPU/timer/PMC diagnostics;
- `start_tick[i]`: after PMC enable work, immediately before the RMW loop;
- `end_tick[i]`: immediately after that loop;
- `window_end_tick[i]`: after event reads and CPU/timer snapshots.

Every worker's exact final counter must equal the specified iterations. The coordinator retains the **scheduled** t0, not the earliest actual start. Define

\[
E=\max_i e_i,\quad D=E-t_0,\quad O=W I,\quad
T_{ns}=D\frac{n}{d},\quad
R=\frac{O\,10^9 d}{D n}.
\]

Here W is worker count, I iterations per worker, and n/d the primary clock's nanoseconds-per-tick ratio. No product or subtraction is performed through a floating representation in the verifier. It checks `end=max(end_tick)`, `delta_ticks=end-t0`, `worker_delta_ticks=end_tick-start_tick`, all phase ordering, exact worker population/identity, and every final software counter.

A worker descheduled after t0 reduces the cohort rate. The code does not subtract its delay, divide by summed CPU time, or time only the fastest participant. Diagnostics before the RMW loop are inside t0-to-end. Work after a worker's end is outside that worker's completion timestamp, although early finisher/controller work can perturb others still running. This scope is explicit, not assumed to vanish.

This is a cache **counter-control** interval, not message throughput or IPC latency. Its `ipc_measurement` field is false. Requested P/C CLI values must be equal for this executable; N denotes N worker threads, not 2N endpoints. The old IPC programs retain their distinct producer/consumer semantics.

### 3.2 Primary clock metadata

Darwin uses `mach_absolute_time` and the returned `mach_timebase_info` ratio. Linux uses `clock_gettime(CLOCK_MONOTONIC_RAW)` with a 1:1 nanosecond representation and `clock_getres` for its reported resolution. Both export integer ticks before conversion [R1,R4].

On Darwin, **reported_resolution_ns is null**. The Mach conversion API does not provide the reported effective resolution requested by that name. `nominal_tick_ns=n/d` is saved separately. On Linux the reported resolution is likewise not renamed “effective update granularity.” Timer scale, API-read cost, update granularity and measurement uncertainty are distinct.

The inherited ordered boundaries remain in the benchmark: compiler ordering and the target's selected instruction barriers. No new fence enters SPSC/NCQ publication. The timed control does not install a sampling profiler, but ordinary interrupts, preemption, clock reads, recording work and page faults are not removed. Wall-time observations are retained without a global overhead subtraction.

### 3.3 Exact decimal export and the ULP requirement

The code does not rely on an assumption that `long double` has the same precision on Linux and Darwin. `bench_json_ratio` forms unsigned 64×64 products using five 32-bit limbs, performs integer division, and emits the integer part plus up to 80 fractional decimal digits. The extra limb accommodates remainder×10. There is no `__int128`, arbitrary external library, or floating intermediate in the primary elapsed/rate formatter.

For nonzero products of two U64 factors, the positive rational is greater than 2^-128 and less than 2^128. Truncation after 80 fractional digits has error below 10^-80, far below the smallest binary64 ULP in that range. Parsing the decimal into binary64 can select an adjacent rounding value near a tie, but remains within the required one-ULP comparison. Zero and terminating fractions emit exact decimals.

The independent verifier computes `Fraction(delta_ticks*n,d)` and `Fraction(O*10^9*d,delta_ticks*n)` and compares each parsed finite numeric value to the exact rational, not to an already-rounded elapsed value. Integer counts have zero tolerance. It explicitly rejects NaN, infinity, duplicate keys, booleans masquerading as integers, zero denominators and nonwrapping violations. Tests cover t0 above 2^53 so an accidental binary64-before-subtraction implementation cannot pass.

**This proves arithmetic consistency of the recorded equation, not sub-nanosecond truth of the physical observation.** A 24-MHz hypothetical timer still has a roughly 41.667-ns nominal tick. Printing many decimal digits or passing this verifier does not certify the old 15/50-ns one-way targets.

### 3.4 Optional Darwin virtual-timer correlation

`CNTVCT_EL0` and `CNTFRQ_EL0` are read only after an opt-in disposable-child probe, run before the benchmark creates any threads. Failed or trapped access returns structured probe status; there is no signal recovery in a live worker. Each usable CNTVCT observation is bracketed by two Mach readings, with instruction/compiler ordering. The independently queried `hw.tbfrequency` value is retained, including query error and size handling. It never overrides CNTFRQ to make a fit [R3].

For samples `(a0,v0,b0)` and `(a1,v1,b1)`, the verifier compares

\[
\Delta V_{ns}=(v_1-v_0)10^9/f
\]

with the bracket interval `[(a1-b0)n/d,(b1-a0)n/d]`, allowing a declared compatibility tolerance of two nominal ticks from each timer. It reports compatible, incompatible, or unavailable. That tolerance is not a universal effective-resolution bound; a true result is not a calibrated physical-time certificate. Mismatched sysctl and CNTFRQ values remain visible separately.

CNTVCT observations are never called CPU cycles and never used in instructions/cycle. A core can execute many instructions between system-timer ticks or execute none while descheduled. This is the central category distinction [R3].

## 4. Linux PMC engine and raw-counter contract

### 4.1 Scope, selection, and resource behavior

Each measuring worker opens up to six descriptors in three pairs. `pid=0,cpu=-1` selects the calling thread, with `inherit=0`, `exclude_kernel=1`, `exclude_hv=1`, close-on-exec, and `sample_period=0`. The context belongs to its creating pthread. Wrong-thread or invalid start/stop order fails; unsupported hardware stays in the result [R1].

| Pair | Linux event type | Selections |
|---|---|---|
| 0 | PERF_TYPE_HARDWARE | CPU_CYCLES; INSTRUCTIONS |
| 1 | PERF_TYPE_HW_CACHE | L1D / READ / ACCESS; L1D / READ / MISS |
| 2 | PERF_TYPE_HW_CACHE | LL / READ / ACCESS; LL / READ / MISS |

Cache config is `cache_id | (op_id << 8) | (result_id << 16)`. The current numeric selectors are recorded with their symbolic event names and IDs. They are not universal vendor raw-event numbers. Pairing permits meaningful cycles/instructions and access/miss comparisons within a common group schedule. Failure to open one member closes that pair; it does not manufacture a partial ratio. Other pairs remain independently usable.

Descriptors are allocated before READY/start coordination, never per increment. RESET, baseline read and ENABLE occur before the worker loop, then DISABLE and final read afterward. Pair operations are sequential, so counted windows include some clock/group-control instructions and can differ between pairs. Raw enabled/running durations make those scopes visible; the report does not call the counter window exactly equal to the RMW wall window.

### 4.2 Exact group decoding

The required read format is GROUP plus TOTAL_TIME_ENABLED, TOTAL_TIME_RUNNING and ID. Each two-event frame has exactly seven U64 words:

`[nr, enabled, running, value0, id0, value1, id1]`.

Before and after frames are stored. The decoder requires nr=2, distinct matching IDs, correctly sized reads, nondecreasing counts/times, and a running delta no larger than the enabled delta. Reordering of event IDs is supported. RESET is not assumed to reset elapsed enabled/running time; subtraction uses the two actual reads.

For event difference c, enabled difference E and running difference R, the supported estimate is `c*E/R` when R>0. R=0 is **NEVER_SCHEDULED**, not a measured zero. A nonzero count difference with no running interval is rejected. Equal positive E and R yields OK; R<E yields MULTIPLEXED. The receipt preserves both the unscaled count and scheduling coverage. The estimate is not a proof that multiplexed intervals were representative of unobserved execution [R1].

The native formatter emits each scaled event estimate through the same exact rational exporter. Group arithmetic and aggregate diagnostic metrics are independently checked by the Python verifier. Missing any required worker's pair suppresses the corresponding combined metric rather than treating its unknown count as zero.

### 4.3 Permission and unavailable states

The library reads `/proc/sys/kernel/perf_event_paranoid`, preserving an unavailable observation as null. An access-denial errno with a value above 2 can be labeled RESTRICTED_PARANOID; at 2, the user-only per-thread request is consistent with upstream policy, so a denial is RESTRICTED_ACCESS. The receipt explicitly warns that errno and paranoid do not uniquely identify the access-control cause. Other failures include UNSUPPORTED_EVENT, OPEN_ERROR, START_ERROR, STOP_ERROR, READ_ERROR and INVALID_READING [R2].

The native run continues when counters cannot be collected. A malformed counter frame is nevertheless a verifier failure, not a successful “unavailable” measurement. There is no sudo invocation, capability modification, sysctl write, kernel patch, or fallback to invented values.

**Observed here:** paranoid=2, and all three required group leaders returned **ENOENT (2)**. Members of failed pairs were not subsequently opened. Thus the paired metrics are unavailable. This is not reported as a permission-denied experiment or as evidence that the underlying physical processor lacks all PMU hardware. The container/virtualized exposure boundary was not diagnosed beyond the actual syscall outcomes.

### 4.4 Why generic cache events cannot confirm invalidation traffic

L1D READ MISS and LL READ MISS are the requested generic cache metrics. They do not universally count every atomic ownership request, RFO, snoop hit, modified-line transfer, invalidation, or coherence stall. A cache line can bounce between private caches without a DRAM miss, and an LLC miss can arise without false sharing. A relaxed atomic RMW can also map differently to the implementation's read/write event accounting [R1,R5,R6].

Accordingly every result carries `coherence_invalidations:null`, `coherence_status:UNSUPPORTED_GENERIC_EVENT`, and `causal_attribution:NOT_ESTABLISHED`. A later causal study needs model-specific event definitions, an admitted collection path, target/CPU scope, coverage/multiplexing analysis, address or ownership evidence, and matched controls. Linux perf c2c/HITM or suitable vendor/uncore events may be relevant when supported; none was fabricated in this implementation.

## 5. Darwin accounting architecture

The Linux six-event backend is explicitly unsupported on Darwin. The implemented Darwin diagnostics are a separate public-API path:

- THREAD_BASIC_INFO provides cumulative user/system CPU times and the scaled instantaneous CPU-usage field for the calling thread.
- getrusage(RUSAGE_SELF) provides whole-process CPU/fault/switch accounting.
- proc_pid_rusage(RUSAGE_INFO_V4) supplies OS-accounted process cycles and instructions when available.
- Mach and optional CNTVCT timer records remain time sources, not PMCs.

Apple distinguishes core PMCs, uncore PMCs and its Recount accounting integrations. The OS-accounted process counters are not equivalent to per-thread user-only Linux perf groups. Their envelope starts before future-start waiting and ends after joins; it includes the controller and watchdog. Their separately derived instructions/cycle ratio is not merged into the worker aggregate or claimed as an RMW-only measurement [R5,R7,R8].

Per-thread utilization uses raw microsecond CPU differences divided by its wider recorded window. Unavailable thread fault/switch fields on Darwin remain -1. Coarse accounting can yield zero or apparent utilization above 100% in short windows; data is not clipped to look plausible. The verifier rejects decreasing valid cumulative times.

No private kpc function or guessed M-series raw event table is called. Consequently Apple L1D/LLC/coherence counts remain an explicit target-gated facility, not an implemented universal PMU promise. The current task's direct CNTVCT request is supplied without falsely presenting it as that facility. **Native SDK build, direct-register access, process-accounting behavior and actual timer correlation on the user's Mac remain NOT_RUN here.**

## 6. Executed native experiments

### 6.1 Machine and fixed workload

Execution used Linux 6.18.44 x86-64 with glibc 2.41, reported **AMD EPYC 9V74**, five allowed logical CPUs 0–4 and cgroup quota 400000/100000 (four CPU-time equivalents). The benchmark requested CPU IDs 0,1,2,3 round-robin and read back constraints before and after work. Eight workers therefore share four selected CPU IDs. This is not a dedicated Apple machine, an exclusive core allocation, or a demonstrated NUMA-placement experiment.

Compilers were **GCC 14.2.0** and **Clang 17.0.0**, C11, O3, strict warnings-as-errors, no LTO, PIC/native executable settings as retained in the Makefile and embedded build JSON. Actual binary identities:

- GCC: `b6b405651a9f61e761fbe25b507d53b624abcae49ae7122bc338dc781cb43da5`.
- Clang: `f7bf79828af3bd6e112dada29d42029f1a4ce22cd776ce6bdd5ac6b8f20f3830`.

For each compiler and each off/count mode, worker counts were 1,2,4,8. Each invocation ran five alternating packed8/isolated128 pairs, with fresh counter allocation and worker threads for each layout, **5,000,000 increments per worker**, zero warmup, and a 60-second per-layout native watchdog. Trials within an invocation share its process. They are not five independent OS/process reboots or the separate Turn 5 IPC qualification campaign.

The final admitted population is 2 compilers × 2 modes × 4 worker counts × 5 trials × 2 layouts = **160 records**. Its exact total is 3 billion increments. Both software-counter reconciliation and independent replay succeeded for all 160. CPU/context-switch/timing tails stay in the records; no fastest-only filtering occurs.

### 6.2 Observed rates

| Compiler | Mode | Workers | Packed median RMW/s | Isolated median RMW/s | Median paired ratio |
|---|---|---:|---:|---:|---:|
| gcc | off | 1 | 529,025,178 | 530,241,305 | 0.9687× |
| gcc | off | 2 | 254,008,063 | 977,680,627 | 3.8490× |
| gcc | off | 4 | 161,829,837 | 1,949,125,486 | 12.3383× |
| gcc | off | 8 | 184,851,213 | 1,913,539,493 | 10.5383× |
| gcc | count | 1 | 540,391,276 | 570,827,018 | 1.0589× |
| gcc | count | 2 | 291,380,046 | 1,017,349,367 | 3.6428× |
| gcc | count | 4 | 178,540,170 | 1,900,368,434 | 10.6439× |
| gcc | count | 8 | 194,587,561 | 2,043,840,689 | 11.2130× |
| clang | off | 1 | 549,883,930 | 559,223,377 | 1.0156× |
| clang | off | 2 | 240,331,927 | 1,055,449,518 | 4.1797× |
| clang | off | 4 | 213,546,087 | 2,048,161,703 | 9.4437× |
| clang | off | 8 | 174,936,595 | 2,039,559,396 | 11.4295× |
| clang | count | 1 | 530,102,507 | 544,536,556 | 1.0191× |
| clang | count | 2 | 260,044,221 | 1,047,915,079 | 3.9445× |
| clang | count | 4 | 215,732,428 | 1,986,650,898 | 9.0137× |
| clang | count | 8 | 220,246,006 | 1,883,215,853 | 9.1002× |


Rates are marginal medians of five measured layouts. Ratios are the median of five **matched per-trial isolated/packed rate ratios**, independently reconstructable as packed/isolated duration ratios for the same operation quota. A median ratio need not equal a ratio of marginal medians.

`off` is the primary no-hardware-profiling-request lane. `count` requested PMCs, but group opening was unsupported here; it is an **unavailable-counter diagnostic lane**, not a collection of active-PMU measurements. It cannot establish active-counter overhead. One-worker ratios near one are a useful baseline. Multiworker rate differences support a locality-sensitivity observation on this workload, not the universal magnitude 113.8× or a quantified invalidation count.

### 6.3 Interpretation and falsification boundary

The new results demonstrate that the benchmark's printed denominator is no longer opaque. A reviewer can reproduce each exact rate from t0, end, n/d, worker count and iterations, and detect a missing/duplicate RMW from the exact final counters. Per-worker timestamps expose skew and different completion times rather than hiding them behind a single rate.

They do not demonstrate message latency, lock-free FIFO progress, pipeline throughput, GPU transfer, NUMA cost, or whole-application freedom from false sharing. The control's separate independent counters intentionally isolate one dimension. The existing 128-byte ring profile remains a conditional byte-layout guarantee; shared NCQ controls still have true sharing. No memory order was weakened to improve a result.

Historical Apple 4.04-billion-RMW/s and approximately 113.8× observations remain lab-reported. Their old JSON lacks the required denominator; this delivery does not reverse-engineer a fake t0 from a rounded rate. A fresh v2 Apple run can resolve that provenance gap for a new observation. The missing hardware event mechanism is a separate limitation that a valid timebase cannot repair.

## 7. Verification, negative controls, and retained failures

| Lane | Executed outcome | Scope |
|---|---|---|
| Strict GCC and Clang builds | PASS | New cache executable and optional PMC archive, inherited benchmarks |
| Native PMC unit executable | PASS | Group decoder, ID reordering, scheduling conditions, statuses, owner-thread/start-stop discipline, CPU snapshots |
| Native SHA-256 vectors | PASS | Empty input, abc, million-a known answers |
| Synthetic perf syscall backend | Eight cases PASS | Actual native open/read/reset/enable/disable/close paths with substituted syscall transport; NOT PMU data |
| Rational formatter differential | 1,004 synthetic cases per compiler PASS; zero denominator rejected | Full-width unsigned products and exact Python Fraction oracle |
| Python verifier suite | **39 tests PASS** | Rational arithmetic, schema, identity, clocks, counter frames, permission status, CLI rejection and Python -O |
| Mutated actual native receipt | **18/18 rejected with exit 1** | Changed raw equations/counts/identities/nonfinite values/coherence claims; preserved mutant files and output |
| Native cache watchdog negative | Expected exit 1; no COMPLETE receipt | Oversized eight-worker run with a one-second limit; not a partial completed rate |
| Primary admitted replay | **160/160 PASS; 3 billion increments** | Exact counters, timebase equations, compiled executable bytes, compile-input source bytes |
| GCC native regression | PASS | Core 68, IPC 100k SPSC and NCQ4/4, adversarial eight, limits four, chaos three, parser one million, benchmark tools |
| Clang native regression | Same native suites completed successfully | Recorded before the separate broad Python hardening timeout |
| Baseline Python bindings | **36 tests PASS** | Inherited source unchanged; not new Apple/CPython3.14 qualification |
| GCC ASan+UBSan | New native PMC tests and instrumented cache runs PASS | Leak detection enabled for these C processes; no performance inference |
| GCC TSan | Instrumented independent-counter runs PASS | Single-process counter control only, not an interprocess ring proof |
| Apple-native new profiler | **NOT_RUN** | Requires the actual SDK and target execution |
| Real hardware event counts | **UNAVAILABLE here** | Required group leaders failed ENOENT; successful synthetic transport is not substituted |

### 7.1 Failed and intermediate records are retained

The first GCC scaffold build and first rational-export build encountered warnings promoted to errors. They were corrected before final admitted compilation; their logs remain. The first campaign runner incorrectly passed `--affinity-tag 0`, which the inherited CLI rejects. That run failed before collecting a completed campaign. The runner now omits an absent tag, and the original failed directory remains archived.

Earlier full v2 campaigns and short smoke tests are preserved as intermediate source/binary revisions, not pooled into the final admitted table. The exact final campaign names are `gcc_admitted` and `clang_admitted`. A no-session streaming tool invocation failed before execution; it supplies no benchmark observation.

The combined Clang regression plus full Python hardening invocation exceeded a 120-second outer execution window during `test_repeated_cycles_no_failures`. The inherited owned-process wrapper terminated its children; an immediate process listing found no remaining matching cohort. That broad invocation is **INCOMPLETE**, not a memory-safety failure diagnosis or a pass. A subsequent standalone 36-test Python baseline completed. The existing bindings are byte-identical to Turn 10; their broader qualification is inherited rather than claimed rerun completely here.

The complete current test/probe log inventory is under `evidence/turn11/`. Sanitized and synthetic outcomes are never included in the O3 rate table. A new future contradictory witness overrides the relevant success claim.

## 8. Standalone verifier guarantees and limits

The verifier accepts either one v2 receipt or a complete native campaign directory. A directory must contain exactly the declared trial×layout population and its completion marker. It does not accept a valid-looking first result as evidence that a later timed-out layout completed.

It checks the complete duration/rate equations, fixed workload fields, worker identities and final values, clock metadata, no-fabrication event statuses, raw read frames, event IDs/configurations, enabled/running differences, scaling, applicable aggregate diagnostics, CPU-accounting equations, and available process-count monotonicity. Duplicate keys and special floating constants fail before semantic validation. `python -O` cannot disable any of these checks.

`--binary` validates the external executable's size and SHA-256. `--source-root` validates every compile-input file and the source snapshot digest. The benchmark hashes its own executable before measurement and again after each layout. The embedded build record includes the actual compiler executable/hash/version and full compile/link arguments. Git is recorded when available; the source snapshot remains usable without a `.git` directory.

These checks do not constitute remote attestation or signature verification. A dishonest evidence producer can invent internally consistent ticks and hashes. Binary hash equality does not independently prove which hardware executed the binary; compiler flags in JSON are recorded build evidence, not a disassembly theorem. Rebuilding in a different path/SDK may legitimately yield a different debug/binary hash. The source and binary identity fields prevent accidental mixing; they do not eliminate the need for trusted execution custody.

## 9. Reproduction and integration

From a clean extracted source directory:

```sh
python3 tools/verify_release.py --strict
make CC=clang BUILD=build/profile benchmarks check-pmc
python3 tools/run_cache_profiling.py --binary build/profile/bench_cache \
  --out cache-v2 --workers 1,2,4,8 --iterations 5000000 --trials 5 \
  --modes off,count
python3 tools/verify_cache_provenance.py cache-v2/count-4 \
  --binary build/profile/bench_cache --source-root .
```

Linux can add an actually permitted `--cpus` list. On the Mac, use `--qos user-initiated` and optionally `--virtual-counter probe`; a nonzero affinity tag remains an explicit hint only. The runner records read-only host/sysctl observations. It never installs a PMU privilege or changes system configuration. A new output directory is required for each campaign.

For rejection coverage:

```sh
python3 tools/falsify_cache_provenance.py \
  --receipt cache-v2/count-4/cache-4-workers-stride-8-trial-0.json \
  --out cache-negative
```

For native integration, create one `elite_pmc_context` in each measuring thread, call start and stop once around the declared region, inspect every group status, then destroy the context in its owner thread. The caller should separately record wall-clock boundaries; enabled/running durations are not a common timestamp source. Never place PMC syscalls in each submicrosecond ring operation and then compare the resulting timings with an uninstrumented profile.

On an admitted Linux PMU host, preserve raw events before interpreting aggregate rates. On Apple, native cache events need the existing separately documented Instruments/model-specific path. Do not reinterpret process accounting or CNTVCT as an automatic replacement. The next target evidence should include the exact Mac model, SDK, binary, clock metadata, worker placement evidence, and all v2 JSON rather than only the printed ratio.

## 10. Acceptance record and remaining work

| Claim | Disposition |
|---|---|
| New cache JSON contains its complete rate denominator | **IMPLEMENTED and EXECUTED** |
| Independent verifier detects required arithmetic/software-counter discrepancies | **EXECUTED**, including deliberate invalid receipts |
| Event read/ID/scheduling/error paths are represented and tested | **IMPLEMENTED**, native unavailable path plus synthetic successful/error transport |
| All inherited queue/ABI/Python implementation bytes preserved | **VERIFIED by hashes** |
| Real local L1D/LLC/cycles/instructions were collected | **No: UNAVAILABLE** |
| New Darwin direct timer and process-accounting code ran on the Mac | **No: NOT_RUN** |
| Approximately 113.8× is explained by measured coherence invalidations | **NOT_ESTABLISHED** |
| Passing numerical replay proves one-way latency, hardware causation or all-target production qualification | **Rejected interpretation** |

**Turn 11 closes A09-05 as a forward-looking provenance defect.** It does not retroactively certify old incomplete receipts. The code now makes both favorable and unfavorable future results reproducible within their actual metric and collection scope. Hardware access restrictions and unsupported causal counters remain explicit outcomes, not reasons to discard the measurement or manufacture an explanation.

## 11. Sources and provenance

[P10] Verified Turn 10 archive and its detached receipt; identity in the header. Existing audit, hardening report and code are preserved in the updated repository. The audit's A09-05 supplies the missing-denominator finding; it does not supply raw values to reconstruct the old result.

[P5] Preserved `docs/reference/05_Turn_05_Verification_Harness_and_Test_Plan.md`, sections 5 and 14: separate timing/counter instruments, complete raw evidence, multiplexing and scope. This turn adds the concrete cache-control implementation; it is not the full earlier IPC qualification.

[R1] Linux man-pages, perf_event_open(2), consulted for event configuration, grouping, timing/ID read formats, counting versus sampling, and error handling. https://www.man7.org/linux/man-pages/man2/perf_event_open.2.html

[R2] Linux kernel, Perf events and tool security, consulted for per-thread/user-only paranoid policy and privilege boundaries. https://www.kernel.org/doc/html/latest/admin-guide/perf-security.html

[R3] Arm, The when, why and how of waiting and backoff in multi-threaded applications on Arm, consulted for CNTVCT/CNTFRQ, counter-read ordering and nominal versus effective update rate. No timing example or source routine was adopted as a benchmark result. https://developer.arm.com/community/arm-community-blogs/b/architectures-and-processors-blog/posts/multi-threaded-applications-arm

[R4] Apple QA1398, Mach Absolute Time Units, consulted for Mach timebase conversion. It is archived API guidance, not a measured M4 frequency table. https://developer.apple.com/library/archive/qa/qa1398/_index.html

[R5] Apple XNU, CPU Counters observability documentation, consulted for core versus uncore counters and counting versus sampling. https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/doc/observability/cpu_counters.md

[R6] Linux kernel, False Sharing, consulted for the distinction between a byte-layout hypothesis and tooling/address/event evidence. https://docs.kernel.org/kernel-hacking/false-sharing.html

[R7] Apple XNU, Recount documentation, consulted for OS accounting and thread/process resource scope. https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/doc/observability/recount.md

[R8] Apple XNU public structures and API declarations: resource.h, libproc.h and thread_info.h. These support the named versioned fields; no vendor SDK/header is redistributed and no native compilation is implied by reading them.
https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/bsd/sys/resource.h
https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/libsyscall/wrappers/libproc/libproc.h
https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/osfmk/mach/thread_info.h

The report's arithmetic design, test choices and measurements are project work, not claims from those manuals. Mutable vendor pages establish consulted contracts, not the precise SDK or kernel used by a future client. The separate deposit receipt records successful upload/readback only after those operations actually occur.
