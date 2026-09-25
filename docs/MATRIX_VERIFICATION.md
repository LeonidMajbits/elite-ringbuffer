# Matrix execution and replay

## Build and run

From a clean source extraction:

```sh
python3 tools/verify_release.py --strict
make CC=clang BUILD=build/matrix benchmarks libraries python check-topology check-matrix
build/matrix/bench_topology > host-topology.json
python3 tools/run_matrix.py --build build/matrix --out matrix-results \
  --trials 3 --count 20000 --small
python3 tools/verify_matrix_results.py matrix-results \
  --binary build/matrix/bench_matrix --source-root .
```

A fresh output directory is required. `--plan-only` writes the complete plan
without running a cohort. `--small` changes the 64-MiB payload conditions from
64 to 16 measured messages; it does **not** change payload size, skip the long
view cases, or claim a full-sized qualification. Default `--timeout 120` is a
per-condition/worker-coordination budget, not a queue latency guarantee. External
launchers must allow the complete multi-condition campaign to finish. The runner
has owned-process-group cleanup and retains failed output; it does not erase a
timed-out condition or mark a partially completed plan green.

Linux CPU pools are selected from the actual affinity-eligible topology, bounded
by the tightest visible CPU quota when present. Quota fractions do not magically
produce fractional physical cores or exclusive service. All selected CPUs and
worker assignments are in the receipts. On Darwin, the runner requests
user-initiated QoS and leaves CPU identity unknown. It never substitutes that
request for a P/E proof. Run the same source and plan choices separately on each
machine; inspect actual unsupported/resource cells before comparing.

`--max-memory-mib` sets a workload-admission budget. The runner takes conservative
limits from visible cgroup memory headroom and host memory as well. Estimates
include ring storage, worst-case per-consumer trace space and coordinator
analysis; they are not reservations against later host pressure. Native allocation
or access can still fail. No global page pressure, affinity privilege change,
cgroup edit, NUMA page migration or cache flush is performed.

## Trace contract

Each native/Python run writes one `result.json` with schema `elite-matrix-v1` and
raw sidecars. Producer p owns a fixed contiguous ID quota; global schedule order
is `sequence * producers + producer_id`. Numeric IDs are not NCQ FIFO tickets.
All producer quotas divide the offered population exactly.

- `producer-PP.trace`: little-endian u64 triples `(id, offer_tick, entry_tick)`.
- `consumer-CC.trace`: triples `(id, read_complete_tick, release_complete_tick)`.
- `consumer-CC.bitmap`: one private bit per expected ID, with zero padding bits.
- `union.bitmap`: the complete checked union.
- `final-reconcile.json`: expected publications, exact backing geometry, and
  quiescent SPSC counters or NCQ live free-entry tickets/words/blocks/phases/epochs.

The native matrix always constructs/checks all payload bytes directly. Its pattern
matches the Python compiled helper: repeating the message ID xor
`0xa5a5a5a5a5a5a5a5` in little-endian eight-byte words. Payload sizes are multiples
of eight in this version. Python bytecode checks every byte instead. Message type
and length are validated under the read lease. The trace contains identities and
times, not redundant raw payload copies. Exact read validation is executable
oracle evidence, not a cryptographic proof of memory contents against a dishonest
recorder.

Arrival codes: 0 means closed-loop per-producer completion acknowledgments;
1 means saturation with actual offer times; 2 means fixed scheduled spacing;
3 means scheduled bursts. Closed-loop acknowledgments use separate test-control
atomics **after release** and are included as a workload perturbation, not a
claimed free component of the production queue. Fixed/burst schedules are derived
from the original common t0; retries never move them forward. Bursts use a fixed
burst interval; the default burst32/320000ns condition has the same mean offered
rate as the fixed10000ns condition.

A retained borrow holds the real C lease or Python memoryview for at least the
requested interval. `read_tick` precedes retention; `release_tick` follows it.
The verifier checks that difference for every selected message. The busy wait is
intentional in the named retention lane and consumes CPU. Releasing a view is not
permission for a writer to access it afterward.

## Replay contract

The verifier independently checks integer geometry, bounded types, source kind,
worker population, topology membership, affinity readback, before/after locality,
raw schedule equations, unique IDs, exact bitmap/trace agreement, end ordering,
retention intervals, final token membership and nonwrapping phase identities.
For every quantile p, it uses order statistic ceil(p*n); converted summaries use
outward integer nanosecond rounding. Elapsed/rate floating fields must be within
one binary64 ULP of the exact `Fraction` equation. Raw integer values have no
floating tolerance. JSON duplicate keys, booleans used as numeric configuration,
NaN/infinity, truncated trace files, unplanned or missing trials and mismatched
manifests fail closed, including under optimized Python.

Source/binary checks are additional: `--binary` checks each native result against
the given binary; `--source-root` checks its embedded compile-input source snapshot
and the Python caller's source identity. Python results separately hash the
interpreter, exporter, native ring library and bridge. Filesystem hashes and
self-reported topology do not constitute remote attestation. Rebuilding in a new
path or SDK may produce a different binary hash; verify old evidence against its
actual binary identity, not an unrelated rebuild.

The matrix archive preserves complete per-run raw data and a campaign manifest.
A valid record can be replayed by itself, but that does not establish completion
of the whole plan. `MATRIX_COMPLETE.json` binds the plan hash, completed record
count and skipped-cell list. The root manifest detects byte changes; the deeper
math/membership checks also reject deliberately corrupted inputs even when tested
without the root hash guard.

## Evidence limits

All Turn 12 goodput is timestamp-instrumented validated goodput. It is not a
substitute for Turn 7's no-per-message-clock lane. Raw entry-to-read observations
are direct single-host one-way intervals, **not qualified absolute physical-time
bounds**. No block-bootstrap population-tail inference or five-repeat100M
acceptance is claimed. A run that completes asymmetry and reconciles all tokens
supports a finite execution statement, not a theorem that every contender will
finish or that every future workload avoids deadlock.

See HARDWARE_BOUNDS.md for the eight audit axes, unsupported P/E/NUMA controls,
working-set capacity classification and historical Apple/Linux claim table.
