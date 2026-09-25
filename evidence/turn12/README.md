# Turn 12 evidence map

## Admitted observations

`gcc_admitted/` and `clang_admitted/` are the two final campaigns. Each contains
its predeclared 37-cell plan: 31 runnable cells with three fresh ring generations
per cell, and six explicit skipped cells. Each completed 93 trials and 729,216
measured messages. The two campaigns reconcile 1,458,432 messages and
38,895,095,808 application payload bytes. Warmup is separate.

Every completed trial includes immutable topology observations, exact producer
and consumer trace records, consumer bitmaps, quiescent token state, workload,
clock, binary identity and completion information. The campaign manifest covers
those bytes. `gcc_replay_final.json` and `clang_replay_final.json` are stronger
final-oracle replays. `MATRIX_RESULTS.csv` and `RESULTS_SUMMARY.json` describe
only these admitted observations, not pilots, synthetic fixtures or sanitizer
runs. Marginal medians and median per-trial p99s are not pooled quantiles or
population confidence intervals.

`CORE_BINDING_CONTINUITY.json` records 16 unchanged inherited implementation
files. A new diagnostic caller is not a replacement queue.

## Verification lanes

* `gcc_regression.log/.exit` and `clang_regression.log/.exit`: native regressions
  and the 36-test inherited Python baseline; no claim of a complete rerun of
  every previous hardening/qualification campaign.
* `matrix_tests_final.log`: 50 arithmetic, topology, membership, token,
  campaign, CLI and rejection tests. Rooted topology fixtures are synthetic.
* `ASAN_MATRIX_RUNS.json`, `asan-*`, `asan_build_topology.log/.exit`: six native
  GCC ASan/UBSan matrix scenarios and topology tests. Not performance evidence.
* `watchdog_negative*`, `watchdog_assertion.json`: deliberately oversized
  retained-view run; expected failure and no completed result. Finite completion
  in other trials is not proof of universal absence of deadlock.
* Cache/PMC compatibility logs preserve the unchanged preceding profiler's
  test scope. They do not supply new hardware cache-event measurements.

## Retained incomplete and intermediate work

`gcc_outer_timeout_01/` is the first campaign interrupted by an outer execution
window after 60 completed records. It has no full-campaign completion marker.
It is excluded from the admitted summary; the subsequent fresh campaign reran
all planned runnable cells. The cause is not relabeled as a queue defect or a
successful trial.

The first native build warnings and the Python bootstrap cleanup-ack ordering
error remain in their logs. The latter was a caller-harness defect, corrected
by reaping the registered child before acknowledging cleanup. Only the one
specifically observed orphaned smoke object was removed after its cohort ended;
`OWNED_SMOKE_CLEANUP.json` records that action.

`smoke_native*`, `python_smoke*`, `unit_base/`, `planning_inspection/` and initial
build/test records are development evidence. `driver_history/` preserves the
runner/verifier revision used during the primary campaign before tightening
L2-type and ambiguous-NUMA rejection. Actual primary metadata used unified L2s
and unambiguous nodes, so the stronger final verifier accepts every primary
record. No native measured binary or queue source was silently replaced.

## Limits

This host exposed AMD EPYC model text but only five eligible logical CPUs, four
CPU-time equivalents, and one NUMA node. No Apple SDK/native execution or
physical cross-NUMA experiment occurred here. Profile claims are OS-visible,
not remote attestation. Missing P/E mapping, memory-controller facts, PMC
causality and physical clock uncertainty remain unavailable/unqualified.
