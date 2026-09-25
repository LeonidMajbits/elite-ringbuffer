# Turn 7 benchmark extension

Start with [`docs/BENCHMARKS.md`](docs/BENCHMARKS.md) for native RTT, validated
goodput, raw evidence replay and separate cache/counter diagnostics.
The Turn 6 core below is preserved except for the credited Darwin creation-mode
fix in `src/elite_shm.c`. Earlier reports/evidence remain historical records.

# ELITEIPC — LE128-V1 C11 implementation

Turn 6 implementation of the frozen SPSC and publish-first NCQ-SC64 protocols.
The native library has no third-party dependencies. It uses C11, POSIX shared
memory, and isolated Linux/Darwin system adapters. It does not wrap payloads in
sockets or pipes. The test launcher uses pipes only for bootstrap and evidence.

**Status:** compiled and exercised on Linux/x86-64 with GCC 14.2 and Clang 17.
Native Apple SDK compilation/linking/execution and the <15 ns median / <50 ns
p99 qualification have **not** been completed. The M4 assembly-only checks are
not a substitute for those gates. See the Turn 6 report and `evidence/README.md`.

## Build and run

Run these commands from the extracted project directory:

```sh
make CC=cc
make CC=cc check check-hooks limits chaos fuzz
build/native/example
```

The default warning policy includes `-O3 -Wall -Wextra -Werror -pedantic` and
additional conversion, prototype, and format checks. `all` builds a static
library, shared library, two test executables and a minimal example. Tests and
hooks are separate from the release objects. No sanitizer runtime is linked
into the normal library. Use a fresh `BUILD` directory when changing compilers
or flags; Make does not fingerprint changed command-line flags automatically.

Linux cross-compiler check:

```sh
make CC=gcc BUILD=build/gcc check check-hooks limits chaos fuzz
make CC=clang BUILD=build/clang check check-hooks limits chaos fuzz
```

On macOS 14.4+ with the current Xcode SDK and Apple Clang:

```sh
make CC=clang BUILD=build/apple
make CC=clang BUILD=build/apple check check-hooks limits chaos fuzz
```

This is a supplied build path, **not a claimed native macOS test result**. The
Darwin default explicitly disables RCpc instruction selection using Clang's
`-Xclang -target-feature -Xclang -rcpc`, so the requested acquire mnemonic profile
uses LDAR instead of an eligible LDAPR. A compiler that rejects these arguments
is not silently admitted. Inspect the actual Apple compiler's generated code.
For an M4-specific build, without widening the architecture independently:

```sh
make CC=clang BUILD=build/apple-m4 \
  ELITE_ARCH_FLAGS='-arch arm64 -mmacosx-version-min=14.4 -mcpu=apple-m4'
```

`ELITE_STRICT_LDAR=0` permits the frozen specification's eligible LDAPR mapping;
that is a separately labeled binary profile, not a source memory-order change.
Do not use an M4-tuned executable on an unadmitted CPU. GCC is the supported Linux
cross-check compiler; Darwin GCC/runtime support is not claimed.

## Source map

| File | Responsibility |
|---|---|
| `include/elite_ringbuffer.h` | Exact shared structs/assertions; native API; status/outcome registry; RA helpers |
| `src/elite_spsc.c` | Private reservation, covering RA cursors, zero-copy borrow/return |
| `src/elite_mpmc_ncq.c` | Strong-SC publish-first index queues and payload phases |
| `src/elite_shm.c` | Exclusive POSIX creation, grants, enrollment, child lifecycle, bounded management |
| `src/elite_core.c` | Native lease identity, public dispatch, view accounting, failure reporting |
| `src/elite_format.c` | Canonical geometry, stable-prefix validation, CRC32/CRC64 |
| `src/elite_wait.c` | Shared futex / public Darwin single-waiter adapter |
| `src/elite_internal.h` | Private state, never a shared-format overlay for clients |

`examples/local_roundtrip.c` is the smallest complete use of two independently
mapped endpoints. `tests/test_ipc.c` demonstrates managed spawn/exec bootstrap
with one endpoint in each worker process. The tests are executable examples,
not a substitute for integrating your application's trusted grant channel.

## Run the finite IPC checks

```sh
build/native/test_ipc spsc 1 1 100000000
build/native/test_ipc ncq 16 16 100000000
build/native/test_ipc park 1 1 100000
```

Each consumer checks every 64-byte payload and maintains a private exact
membership bitmap; the controller checks disjointness, completeness, quotas,
returns and final queue membership. Numeric producer message IDs are not global
publication tickets. Processing completions may be out of order.

The printed `controller_elapsed_ns` includes control/collection/bitmap work.
It is **not latency** and is **not** the frozen Turn 5 drained-cohort benchmark
interval. These integration trials do not implement its complete five-restart,
1M-warmup, common-start, raw-evidence and metrology protocol. A watchdog or
external timeout is incomplete evidence, not a successful loss-free run.

The 32-worker test is oversubscribed on a 16-core host, and even more so on the
five-CPU/four-CPU-time-quota container used for this delivery. Test-host metadata
is recorded rather than pretending to be Apple Silicon.

## Sanitizers and assembly

```sh
make CC=clang asan
make CC=gcc tsan
make CC=gcc BUILD=build/gcc inspect
CLANG=clang tools/cross_lowering.sh
```

The TSan adapter explicitly shares one virtual mapping among threads. It does
not certify cross-process race detection. No core suppressions or synthetic
synchronization annotations are used. `tests/tsan_negative.c` is an intentional
race detector control and is **never** linked into the library.

`tools/cross_lowering.sh` compiles the actual two queue translation units to
AArch64/Apple-target assembly using compiler resource headers and minimal
**declaration-only** shims. It does not provide an Apple SDK, link Darwin, or
execute the code. The shims must never be placed in a production include path.

## Ownership and lifecycle rules

Every connection is single-owner and nonreentrant; the application serializes
calls and shutdown on that one handle. Different endpoints operate concurrently.
The local entry guard is not a mutex or a promise of safe concurrent misuse.

A successful reserve owns one writable span. End all aliases before commit or
abort. A successful borrow owns one read-only span. End all aliases before
release. Optional `elite_view_retain` / `elite_view_end` let wrappers account for
tracked aliases. C raw pointers cannot be revoked; an escaped untracked pointer
is an unsafe caller responsibility. Do not reuse the storage of an active lease
as the output of another reserve/borrow attempt; use separate output variables.

Check **both** `result.status` and `result.outcome`. PUBLISHED is irrevocable even
if a later notification reports an OS error. RETAINED is never permission to
reuse bytes. No data-plane allocation occurs per message.

A timeout does not steal a token. Retirement does not establish quiescence.
`elite_abandon_retained` may end local responsibility only after aliases end in a
retired generation; it does not return the token to QF or recover its contents.

The authority records potential holders before issuing grants. Cleanup receipts
or terminal owned-child evidence resolve them. A zero attachment count does not
permit destruction by itself. Failed creation/cleanup remains charged when its
outcome is uncertain. At most four backing objects and two quarantines are
allowed. The manager is application-owned, serialized, and has no automatic
handoff. See `docs/API.md` before deployment.

## Not provided as certified behavior

No timed slot revocation, persistent power-loss recovery, exactly-once external
effects, MPMC parking, automatic authority takeover, malicious-writer isolation,
Python binding, hard scheduling deadline, or achieved nanosecond target.

## Integrity and attribution

`SOURCE_MANIFEST.sha256` authenticates the source/build/test files.
`MANIFEST.sha256` authenticates the complete delivered project except itself.
The enclosing ZIP has its own sidecar. Executables are not distributed; build
identities from executed tests are retained under `evidence/`.

NCQ algorithmic prior art is Ruslan Nikolaev's DISC 2019 work, Figure 5. This
implementation follows the supplied NCQ-SC64 refinement, not an unmodified
upstream source file. See `NOTICE.md` and the frozen reference documents.
