# Turn 11 evidence index

## Authoritative new observations

`gcc_admitted/` and `clang_admitted/` contain the two final 80-record campaigns (160 records, 3,000,000,000 reconciled increments in total). Each condition uses five paired trials. `CACHE_RESULTS.csv` and `.json` summarize only these campaigns; paired-ratio medians are not ratios of separate medians. `final_raw_replay.log` records exact receipt, binary, and compile-input source validation. Native unsupported-event status is evidence of missing access/support, not a measured zero counter.

`environment.json` and campaign provenance identify the actual Linux container, CPU budget, compiler, requested masks and clock. `CORE_BINDING_CONTINUITY.json` proves the 16 inherited native/header/binding files unchanged. `PARENT_IDENTITIES.json` binds the input.

## Tests, diagnostics, and scope

`verifier_final_tests.log`: 39 unit tests. `negative_mutants/`: 18 falsified real-receipt derivatives with expected exit 1, including Python -O. Native mock-backend cases are **synthetic** and test the decoder/control path; they do not claim actual PMU measurements. The native rational formatter is checked against 1,004 exact Python Fraction vectors per compiler plus zero-denominator rejection. `watchdog_negative/` intentionally lacks a completed campaign. Its native command exits 1.

`asan_admitted/` and `tsan_admitted/` hold final instrumented small cache campaigns, eight records each. Instrumentation results are not pooled into performance claims. TSan concerns one address-space counter control, not interprocess shadow-state or Python race certification. ASan/UBSan leak detection was enabled for the named C scope.

`gcc_regression.log` retains complete native regressions. `clang_regression.log` retains successful native sections followed by an **incomplete broader Python hardening invocation** stopped by its outer 120-second limit. `python_baseline.log` is a later independent complete 36-test baseline, not a substitute claimed to complete the timed-out suite.

`gcc_cache_disassembly.txt`, `clang_cache_disassembly.txt`, and `pmc_undefined_symbols.txt` record inspection of the local admitted builds. No Apple-native result or physical hardware counter value is invented.

## Preserved development history

Initial build warnings, a rejected zero-affinity-tag runner invocation, and pre-final campaigns are preserved. `initial_*`, `gcc_primary*`, `clang_primary`, `gcc_release`, and non-admitted sanitizer folders are historical development evidence, not additional final qualified samples. Only paths named as authoritative above determine the primary results.

`EXECUTION_STATUS.json` is the machine-readable scope summary. Full release SHA manifests authenticate these files. Fresh extraction/build and remote download-back checks occur after packaging and are recorded separately in the final delivery receipt, not retroactively inserted into the immutable archive.
