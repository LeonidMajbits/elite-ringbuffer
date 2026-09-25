# Incomplete and failed instrumentation records

- `python-first.log`: development wrapper/test protocol errors; fixed before the final36-test runs.
- `python-asan-ubsan.log`: outer60-second execution limit, no full-suite conclusion.
- `python-asan-processes.log`: outer60-second execution limit, no full process-suite conclusion.
- `python-asan-processes-retry.log`:10 process tests reached a final result with one error: the throughput subprocess exceeded20 seconds. This is NOT a clean full-suite result.
- `python-asan-release.log`: diagnostic timeout-scale5 trial hit an outer120-second limit and remains incomplete.
- `FINAL_PYTHON_1048576.log`: combined outer invocation ended during this condition; no complete JSON was produced. Separate `FINAL_PYTHON_1048576_RETRY.json` completed.

Successful scoped ASan/UBSan evidence: `FINAL_ASAN_LIFETIME.log`/`.exit` (26 tests), plus `python-asan-mpmc.log`/`.exit` (one isolated2P2C test). These do not erase incomplete/failed attempts. Leak detection was disabled in these CPython-hosted runs. No cause is invented for an outer tool expiry.
