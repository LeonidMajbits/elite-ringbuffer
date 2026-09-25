# Changes

## 1.0.1 — Turn 10 hardened release candidate

- A09-01: Count pending buffer exports before Python callbacks; unwind callback
  and FillInfo failures; honor pending finalization without revoking live views.
- A09-02: GC-track the exporter and separate native cleanup state from public
  reference cycles; defer native unpin until every admitted buffer export ends.
- A09-03: Regenerate complete/source manifests with license and audit coverage;
  verify exact inventory without modifying the source tree or writing receipts.
- A09-04: Fix the unsigned operand promotion in POSIX shared-memory name encoding
  for strict GCC O1 ASan/UBSan builds.
- A09-07: Do not expose dying exporter self through unraisable error reporting;
  use CPython resurrection-aware finalization before deallocation.
- A09-08: Install acquisition ownership data before ctypes; poison and retain an
  endpoint when an interruption makes native acquisition outcome uncertain.
- Preserve LE128-V1, native SPSC/NCQ/core/format/wait implementation and headers.
  The Python exporter/library revision changes; deploy them together.
- Preserve the existing standalone MIT license. This candidate still requires
  native Apple/CPython 3.14 validation of its new GC implementation. Historical
  benchmarks and old test logs are not reclassified as new candidate evidence.


## Turn 11 profiling extension (native wire/library version remains 1.0.1)

- Add complete cache-control v2 timing, per-worker and executable/build receipts.
- Add separately linked per-thread Linux perf counting plus optional Darwin
  timer/CPU-accounting diagnostics, without changing the queue or Python core.
- Add arbitrary-precision rational replay, portable exact decimal export,
  independent rejection tests, and preserved native evidence.
- Historical cache ratios are not retroactively certified; unavailable coherence
  events remain unavailable rather than being inferred from elapsed time.
