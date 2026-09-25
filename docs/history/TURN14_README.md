# Turn 14 — Owned-process chaos and managed recovery verification

The queue, shared ABI, and hardened Python bindings remain **ELITEIPC 1.0.1 / LE128-V1**.
This extension adds real spawned-process fault histories, monotonic control-plane
heartbeat suspicion, bounded quarantine/successor orchestration, exact final token
accounting, and an independent receipt verifier. It does **not** add timeout-based
slot reclamation, automatic authority takeover, or a v1.1.0 certification.

```sh
python3 tools/verify_release.py --strict
make CC=clang BUILD=build/chaos chaos-multiprocess check-chaos-multiprocess
python3 tools/run_chaos.py --build build/chaos --out chaos-new --trials 3
python3 tools/verify_chaos_results.py chaos-new --source-root . --build build/chaos
```

The standard plan has 31 cases per repetition: 30 multi-process histories plus
one separate single-process resource negative. Every failed or incomplete run
stays a failure/incomplete record; no child is reclaimed from heartbeat age alone.
A safe old generation can contain unavailable orphaned tokens or pending committed
messages until whole-object disposal. Those are reported, not silently called delivered.

Read [CHAOS_RECOVERY.md](docs/CHAOS_RECOVERY.md) and the
[Turn 14 report](14_Turn_14_Chaos_and_Crash_Recovery_Report.md) for exact coverage,
source identity, commands, and exclusions. Historical sections below keep their
original evidence scope; a bounded native pass is not an all-execution proof.

---

# Turn 13 — Lane V1 formal verification extension

This tree retains ELITEIPC 1.0.1 and LE128-V1 unchanged. It adds executable
finite data-plane models, a separate release/acquire handoff-graph checker,
source identity checks, explicit mutation witnesses and replayable evidence.

```sh
python3 tools/verify_release.py --strict
make CC=clang BUILD=build/formal check-formal check-formal-results
python3 tools/run_formal.py --out build/new-formal-campaign
python3 tools/verify_formal_results.py build/new-formal-campaign --replay-baselines
```

The default campaign exhausts its **declared seven bounded configurations**.
It does not certify all ISO C11 executions, every planned Turn 5 bound, an
operating-system lifecycle, hardware scheduling fairness, or native latency.
An extended search that reaches a resource cap returns INCOMPLETE rather than PASS.
Strong relaxed CAS remains atomic; the mutation suite separates missing ordering
from broken compare/exchange and stale-retry logic.

Read [FORMAL_VERIFICATION.md](docs/FORMAL_VERIFICATION.md) and
[the Turn 13 report](13_Turn_13_Formal_Verification_Report.md) for exact bounds,
proof assumptions and the optional, unexecuted Spin/GenMC inputs.
Historical release and measurement text below keeps its original evidence scope.

---

# ELITEIPC 1.0.1
## C11 zero-copy shared-memory IPC, SPSC and publish-first NCQ-SC64

**Release candidate:** 1.0.1 · **Shared format:** LE128-V1 (`0x00010000`) · **License:** MIT (existing standalone license retained).

One C implementation owns the shared atomics. C++ calls its opaque C ABI. Python
uses a standard-library ctypes adapter and a small compiled CPython buffer
exporter that retains both the native mapping and the payload lease. No external
framework, payload socket/pipe, background daemon, or third-party native library
is required. Optional Python support needs matching CPython development headers;
the native library does not depend on Python.

Start with [the integration runbook](docs/INTEGRATION.md),
[the Python API](bindings/python/README.md), and
[the hardening report](10_Turn_10_Hardened_Release_Candidate_Report.md).
[The native API](docs/API.md) and [native benchmark guide](docs/BENCHMARKS.md)
retain the detailed ownership and measurement contracts.

## Topology-aware evidence (Turn 12)

The optional measurement tools discover OS-visible CPU/cache/NUMA and resource
boundaries, then predeclare an eight-axis **contrast matrix**, not an exhaustive
Cartesian qualification. Results retain raw offer/entry/read/release times,
exact membership and final token snapshots. Unsupported placement is explicit.
See [hardware claim boundaries](docs/HARDWARE_BOUNDS.md) and
[matrix execution/replay](docs/MATRIX_VERIFICATION.md).

```sh
make CC=clang BUILD=build/matrix benchmarks libraries python check-topology check-matrix
python3 tools/run_matrix.py --build build/matrix --out matrix-results --trials 3 --small
python3 tools/verify_matrix_results.py matrix-results --binary build/matrix/bench_matrix --source-root .
```

No generic Apple-vs-x86 speedup, zero-latency promise, Python-bytecode bandwidth
claim or universal absence of false sharing follows from an isolated benchmark.
The new detector does not infer P-core pinning from a Mach QoS request.

## Build

From the extracted project root, on Linux or native ARM64 macOS 14.4+:

```sh
make CC=clang CXX=clang++ BUILD=build/release all benchmarks python
make CC=clang CXX=clang++ BUILD=build/release release-check
```

On Linux, GCC/G++ are also supported. Use a fresh BUILD directory when changing
compiler, architecture, optimization, or sanitizer flags. Native library builds
use C11, `-O3 -Wall -Wextra -Werror -pedantic` and additional conversion/prototype
checks. macOS includes `_DARWIN_C_SOURCE` and the credited shm-permission fix.
The Darwin default `ELITE_STRICT_LDAR=1` requests the previously audited
RCpc-disabled compiler profile; unsupported target flags must fail explicitly.
M4 tuning is optional, never proof of the machine's actual processor identity.

Outputs include `libelite_ringbuffer.a` and either `libelite_ringbuffer.so` or
`libelite_ringbuffer.dylib`. The optional exporter is under `BUILD/python/`.
`elite_ringbuffer.h` exposes exact shared layouts to C11 and only the opaque
`elite_api.h` branch to C++; no foreign atomic overlay is supported.

## Python zero-copy example

Select the actual native library suffix for the host:

```sh
export ELITE_LIBRARY="$PWD/build/release/libelite_ringbuffer.so"  # .dylib on macOS
export PYTHONPATH="$PWD/build/release/python:$PWD/bindings/python"
python3 examples/python_roundtrip.py
```

```python
import struct
from elite_ringbuffer import EliteShm

with EliteShm("spsc", capacity=1024, max_payload=64) as shm:
    with shm.producer() as producer, shm.consumer() as consumer:
        with producer.reserve() as write:
            with write.buffer as view:
                struct.pack_into("<QQ", view, 0, 42, 99)
            write.commit(16, message_type=1, message_id=42)
        with consumer.borrow() as read:
            with read.buffer as view:
                assert struct.unpack_from("<QQ", view) == (42, 99)
```

Every derived view must end before commit/release. A retained slice/cast/ctypes
buffer keeps the slot and mapping live. A consumer's underlying exporter is
read-only, not a writable ctypes array disguised by one read-only view.
`read.copy()` is an explicitly copying convenience. Python objects and per-call
FFI overhead still exist; zero-copy describes the payload, not zero overhead.

## Installation without package downloads

```sh
make CC=clang BUILD=build/release PREFIX="$HOME/.local/elite-1.0.1" install install-python
```

The installer prints the exact version-specific site-packages directory. Add it
to PYTHONPATH when outside the interpreter's normal search path. That installation
includes a matching native library next to the package, so ELITE_LIBRARY is then
optional. The exporter is built for the selected interpreter; do not move a
CPython-version-specific binary into another interpreter.

## Evidence and supported claims

Turn 10 hardening evidence is in `evidence/turn10/`: exact pre-fix negative
controls, strict GCC/Clang builds, pending-export transaction tests, cyclic-GC
lifetime tests, and failure-injection probes. The library and exporter use version
1.0.1; the shared-memory ABI remains LE128-V1 / `0x00010000`. The exporter and
Python package must be rebuilt/deployed together; import rejects an old exporter.

The hardening report preserves its original Linux/GIL-enabled CPython 3.13.5
evidence. The lab subsequently reported native Apple hardening and Turn 11 passes;
those statements are separate from the newly implemented Turn 12 topology/matrix
branches, which require execution of this exact build on the target. The 1.0.1
version does not certify new latency or bandwidth. Historical benchmark evidence stays under its original
scope; see the hardening report for outstanding qualification.

This is not a hard-real-time, durable, exactly-once, or automatically self-healing
transport. A killed owner can strand a token. Retirement closes admission;
quiescence or effective fencing permits whole-generation reclamation. A timeout
never steals payload bytes. Managed successors share a four-object/two-quarantine
budget. A library or authority failure does not create safe takeover by itself.

## Verification and integrity

```sh
python3 tools/verify_release.py .
make CC=clang BUILD=build/release check-python-hardening check-cpp
```

`SOURCE_MANIFEST.sha256` covers source/build/test/tool inputs.
`MANIFEST.sha256` covers every packaged file except itself. Historical manifests,
reports and evidence remain named as such. The archive includes no compiled
libraries, executables, proprietary SDK, sanitizer runtime, or the multi-GB Turn7
raw dataset. It retains that dataset's existing identity and summary records.

Algorithmic attribution is in [NOTICE.md](NOTICE.md); the existing [MIT license](LICENSE) is unchanged.

## What 1.0.1 hardens

Buffer exports are counted before any Python validation callback. Failed
acquisitions unwind that count, including pending finalization. The C exporter
participates in cyclic GC and uses resurrection-aware finalization. Native
cleanup data lives outside user reference cycles: a surviving view still pins
both its slot and mapping, while an unreachable normal cycle can be collected.
No finalizer commits an unpublished write or steals an uncertain token.

`tools/verify_release.py` is read-only and checks inventory as well as hashes,
including `LICENSE`, `.gitignore`, and `docs/ADVERSARIAL_AUDIT.md`. Run `--strict`
on a fresh extracted source archive; ordinary mode ignores only documented
build/cache overlays. `tools/regenerate_manifests.py` is an explicit release-author
operation, never a way to make verification silently bless edited sources.


## Turn 11: cache-control provenance and optional PMCs

The hardened 1.0.1 queue/FFI code is unchanged. `docs/CACHE_PROVENANCE.md` describes
`elite-cache-control-v2`, the optional `libelite_pmc.a`, exact raw-tick replay,
and the independent verifier. Build with `make benchmarks check-pmc` and run
`python3 tools/run_cache_profiling.py --help`. Counter-control rates are not IPC
rates. Unavailable hardware is reported explicitly; the system timer is not a
core-cycle PMC, and generic cache misses are not coherence invalidations.
