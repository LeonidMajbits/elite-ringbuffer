# Turn 14 evidence custody

Primary admitted data is **gcc_final/** and **clang_final/** only. Each has
93 cases (90 multi-process / 3 separate resource negatives). The corresponding
`*_final_replay.json`, `CHAOS_RESULTS.csv`, `ASYNC_RESULTS.json`, and
`RESULT_SUMMARY.json` summarize those exact receipts. Do not pool development,
sanitizer, fixture, predecessor, or temporary smoke counts into their totals.

`gcc_admitted/`, `clang_admitted/`, and `dev_*` are intermediate revisions,
retained to avoid hiding development or verifier failures. Their manifests or
bound source hashes may intentionally differ from final source. They are not
current-source qualification records.

`asan_smoke/` contains a separate final-source, ten-case GCC ASan/UBSan campaign;
it is not a performance dataset. `asan_build.log` records strict O1 flags.
Leak detection was requested for normally exiting C processes, not claimed for
processes intentionally killed before any exit handlers could run.

`watchdog_negative/` rejected an overlarge count before a valid campaign.
`watchdog_timeout_negative/` used valid counts and a 20-ms outer watchdog,
recorded timed_out=true, and deliberately lacks COMPLETE. This is an expected
negative test, not a completed data transfer. Failed/partial directories are
never accepted by the independent campaign verifier.

`clang_release_check.log` is an **INCOMPLETE** combined invocation: the outer
execution window ended at 200 seconds. Its adjacent INCOMPLETE text records the
scope. `clang_isolated_regression.log` is the later complete native/formal/new
chaos invocation; it does not retroactively turn the interrupted full discovery
into a pass. The inherited 36-test Python baseline passed in the earlier log.

`PRODUCTION_CONTINUITY.json` compares all 21 inherited production-directory
files against the authenticated Turn 13 ZIP. `HOST_AND_COMPILERS.json` records
the actual Linux host and compiler identities. Source/binary custody for each
campaign is in its immutable plan, checked before and after execution.

New stdout JSONL describes observer events, not a transactional persistent log
inside the IPC library. It is neither a substitute for physical access fencing
nor an authentication proof against a dishonest evidence producer.
