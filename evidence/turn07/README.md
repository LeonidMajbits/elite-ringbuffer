# Turn 7 evidence

Everything here is newly executed Linux/x86-64 evidence except where explicitly
identified as source documentation. Earlier sibling `evidence/` files are
inherited Turn6 records and must not be treated as a fresh execution.

`Linux_100M_Summary/` preserves final JSON, command/binary/source provenance and
the complete raw run manifest. The large raw arrays and bitmaps are intentionally
in the separately supplied `Elite_Systems_LockFree_RingBuffer_Turn_07_Linux_100M_Evidence.zip`,
not duplicated inside the source bundle. Extract that archive to obtain
`linux-100M/`, then run the verifier against that full directory. The summary
folder alone is not a complete raw-replay input.

`native-sweep/` and `asan-benchmark-sweep/` are small finite functionality sweeps,
not100M results. Their raw artifacts are included and independently replayed.
`negative-controls/` contains deliberately invalid bitmaps, durations, incomplete
records and watchdog failure markers. A verifier MUST reject those records.
Do not point the whole-package verifier at a directory intentionally mixing
negative fixtures and positive trials and interpret rejection as a queue bug.

The first strict-build diagnostics were resolved before actual benchmark runs.
`replay-100M-first-invocation.txt` preserves an outer-tool timeout during offline
verification. The separately completed replay is in `replay-100M.log` with
exit0. No failed native 100M trial is hidden by that offline replay retry.

ASan/UBSan ran all seven new small benchmark conditions plus the separate cache
control. TSan ran only the one-process independent-counter control, not the
interprocess protocol. GCC native regression includes core68, both100k IPC
profiles, adversarial8, limits4, chaos3, and parser1M. Clang separately compiled
all new benchmark programs and ran core/100k IPC regression. No new native
Apple benchmark or hardware PMC observation was executed here.
