# Executed evidence and scope

The final records are the `final-*` logs, `machine-and-toolchains.json`, binary
and tested-source identity lists, and the assembly records. Earlier successful
logs are retained for development provenance; do not sum them as independent
qualification trials. Shared-memory atomic admission remains a platform claim
with finite executed evidence, not an ISO guarantee inferred from a query.

## Results on the final source

| Record | What actually ran |
|---|---|
| `final-gcc-build-and-tests.log` | Strict GCC build, 8 adversarial groups, 3 real child-fault histories, 4 limit fixtures, 1M parser corpus |
| `final-gcc-native-runs.log` | 68 core groups; local example; parkable SPSC 100k; polling SPSC 100M; combined command later externally timed out during NCQ attempt |
| `final-gcc-ncq-10m.log` | Separate 16P/16C native spawn/exec NCQ, 10M messages, exact membership/reconciliation |
| `final-gcc-ncq-100m-retry.log` | Separate 16P/16C native spawn/exec NCQ, 100M messages, exact membership/reconciliation |
| `final-clang-build-and-tests.log` | Strict Clang build, core/adversarial/chaos/limits, 16P/16C 1M IPC and SPSC 100k |
| `final-asan-clang.log` | ASan+UBSan: core68, adversarial8 and stable-prefix 1M corpus; exit 0 |
| `final-tsan-gcc.log` | Unsuppressed TSan: one-mapping threaded adapter and controlled histories; 8 groups, exit 0 |
| `tsan-negative-control.log` | Standalone deliberate race detected; expected/actual exit 66 |
| `tsan-runtime-options.log` | Runtime help/current settings used for the unsuppressed configuration |
| `final-cross-lowering.log` | Actual queue source files lowered for AArch64; strict M4 LDAR/STLR check |
| `undefined-symbols.txt` | Per-object undefined symbols; no libatomic/mutex primitive fallback reference |

The external 200-second combined command expired after the SPSC completion
record and before any NCQ completion record. That NCQ attempt is **INCOMPLETE**.
Its root cause was not established. No later success turns it into a pass.
After the worker cohort was no longer present, only its specifically observed
orphaned test shared-memory object was removed. The isolated 10M and 100M retries
are separate successful integration trials, not an explanation of that timeout.

Both 100M success records check every payload and exact in-memory membership.
The raw private bitmaps are not exported in this first core harness. Summary logs
and runnable oracle source are retained; there is no claim that an independently
reanalyzable complete Turn 5 raw-data dataset has been deposited.

## Representative reproduction commands

Run from the project root. Use a fresh BUILD directory for changed flags.

```sh
make -B -j4 CC=gcc BUILD=build/final-gcc all check-hooks chaos limits fuzz
build/final-gcc/test_core
build/final-gcc/test_ipc park 1 1 100000
build/final-gcc/test_ipc spsc 1 1 100000000
build/final-gcc/test_ipc ncq 16 16 100000000
make -B -j4 CC=clang BUILD=build/final-clang all check-hooks chaos limits
build/final-clang/test_core
build/final-clang/test_ipc ncq 16 16 1000000
```

The complete sanitizer compile commands are preserved in their final logs.
The final TSan execution used:

```sh
TSAN_OPTIONS=halt_on_error=1:exitcode=66:force_seq_cst_atomics=0:report_atomic_races=1:ignore_interceptors_accesses=0:ignore_noninstrumented_modules=0 build/final-tsan/test_adversarial
```

ASan/UBSan used `ASAN_OPTIONS=halt_on_error=1:abort_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. No core ignorelist or race
suppression was configured. Sanitizer observations cover the adapter's scope,
not race detection across unrelated process shadow memories.

## Assembly evidence

The freestanding cross build lowers the real `src/elite_spsc.c` and
`src/elite_mpmc_ncq.c`, including the actual public ABI assertions. It supplies
compiler-owned freestanding scalar/atomic headers plus declaration-only shims
for `pid_t` and string prototypes. It does **not** compile `elite_shm.c` or
`elite_wait.c` against a fabricated Apple SDK.

`apple-m4-elite_spsc.s` shows eligible default RCpc lowering. The
`apple-m4-norcpc-*` files use explicit compiler feature suppression and show
LDAR/STLR plus strong-SC CASAL for NCQ. The baseline AArch64 source build shows
LDAXR/STLXR where exclusive loops are required. Full instruction paths and
operands are retained, not only a claimed mnemonic count.

## Unfulfilled qualification cells

Native macOS SDK/link/run, actual Apple core topology/placement, full five-run
V5 qualification, direct p50/p99/p99.9 metrology, complete bounded formal checker
and every mutant, all chaos repetitions, raw bitmap/latency export, quantitative
restoration objective, and CFFI/view-binding certification remain NOT_RUN or
incomplete. They must not be inferred from this implementation delivery.
