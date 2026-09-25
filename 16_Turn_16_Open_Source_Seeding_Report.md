# Turn 16 — Open-Source Seeding Kit, CI Matrix, Man Pages and Live Demonstration
## ELITEIPC v1.1.0 / LE128-V1 / publication and installation engineering

**Project:** Leonid Majbits / Gemini Operator Lab, paired with Leon  
**Date:** 24 September 2026; exact invocation logs under `evidence/turn16/`  
**Release identity:** Library 1.1.0; native version `0x00010100`; wire ABI `0x00010000`, unchanged  
**Distribution:** A new seeding derivative, not a silent replacement of the sealed Turn 15 archive  
**Canonical-SHA256:** `0641786e220b77865e346f2cb8eda2df89d76054856f365199da8e4c6fd6b869`  
**Canonical convention:** Replace only this field's 64 hex characters with 64 ASCII zeros before hashing the entire UTF-8/LF report. Its sidecar authenticates the literal finalized bytes.  
**Parent:** `Elite_Systems_LockFree_RingBuffer_v1.1.0_Final_Release_Bundle.zip`; 48,422,042 bytes; SHA-256 `81c4ed6e42f62328b79777bb603558f9f721a99e3da1a1734fda78a9d113a01d`  
**Destination:** Deployment Research / Elite_Systems_LockFree_RingBuffer  
**Evidence terms:** EXECUTED is a retained local invocation. IMPLEMENTED is reviewable source. LAB_REPORTED is the user's Apple account. NOT_RUN and INCOMPLETE are not passes.

---

## 1. Delivery decision

The seeding kit supplies the missing publication and developer-integration surface: a pinned, least-privilege GitHub Actions matrix; installable section 3 and section 7 manuals; a relocatable CMake package; Homebrew formula generation and a local vcpkg overlay; and a real two-process console demonstration with raw timestamp export and independent replay.

The code does not gain a second queue implementation. The demonstration and installation clients call the same native C library. No lease is revoked by a signal handler, no ownership field is added to reserved bytes, and no throughput reciprocal is presented as a latency percentile.

The kit is ready for repository review and first hosted execution. This report does not claim that GitHub Actions, a FreeBSD VM, Homebrew, or vcpkg ran merely because their recipes exist. The locally completed Linux tests and the remaining target gates are separated below. The lab's earlier M3 Max qualification remains an account about the preceding derivative, not evidence that these new build files and demo were executed there.

## 2. Source custody and exact change budget

The parent ZIP digest, CRC and original inventory were checked before creating this separate tree. Its strict verifier accepted 7,989 complete entries and 126 source entries. Historical reports and failed/incomplete evidence are retained rather than rewritten as new successful runs.

Nineteen of twenty inventoried existing C/Python implementation/header files under `src/`, `include/`, and `bindings/` are byte-identical. The only changed existing native implementation file is `src/elite_format.c`. It receives two compile-time FreeBSD branches: an explicit opt-in to platform admission and an early refusal of non-polling profiles on that OS. No public field, exported function, status number, source memory order, queue algorithm, byte stride, Python exporter or GC mechanism changes. `PRODUCTION_CONTINUITY.json` records every compared digest; `docs/FREEBSD_PORT.patch` records the exact native delta.

The FreeBSD admission is deliberately not unconditional. A new operating system needs a genuine interprocess atomic/mapping/lifecycle qualification, not a claim that the word POSIX proves the entire protocol. The unopted FreeBSD build stays unsupported. `ELITE_EXPERIMENTAL_FREEBSD=1` admits only POLL_ONLY; PARKABLE_SPSC returns UNSUPPORTED before object creation. Existing Linux futex and Darwin public address-wait adapters remain untouched.

For Linux, GCC preprocessed the parent and current `elite_format.c` with the same C11 definitions/include paths; the outputs were byte-identical, SHA-256 `5bdee833ff6381b73341de40db367384fc211c965212d383ccfcf0516c5af704`. This directly supports the narrow statement that the new guarded source does not change that Linux preprocessing result. It is not an Apple or FreeBSD compilation certificate.

The full-file formal binding must nonetheless change when those bytes change. The initial full regression correctly rejected the historical Turn 13 receipt against the new source-binding set. Its oracle was not weakened. We retained the original evidence, reran all seven bounded baseline searches plus the designated negative/auxiliary population into `evidence/turn16/formal_admitted`, and pointed the current fixture at that new campaign. The graphs again exhausted 2,158,499 states and 5,700,615 transitions. That corroborates the unchanged abstract model under the updated manual source review; it is not an independently proved FreeBSD refinement or a new general C11 checker. [P13]

## 3. GitHub matrix: precise jobs and failure semantics

The workflow is `.github/workflows/ci.yml`. It is triggered by pushes to main, version-like tags, pull requests, and manual dispatch. Default token permission is `contents: read`; checkout disables credential persistence. There is no `pull_request_target`, secret access, self-hosted runner, automatic tag movement, or release publication. Run/job timeouts and command process groups bound the work. Evidence upload uses `if: always()` so ordinary failures are not discarded. [R1–R2]

| Matrix lane | Selected environment | Work actually configured |
|---|---|---|
| Native GCC | Ubuntu 24.04 | Make release checks, new surface tests, CMake/CTest, relocated C/C++ consumers and two demo receipts |
| Native Clang | Ubuntu 24.04 | The same scopes through an independent compiler |
| ASan + UBSan | Ubuntu 24.04 / GCC | Separate O1/debug instrumented core, controlled adversarial tests and small live demonstrations |
| TSan | Ubuntu 22.04 / GCC | Deliberate detector control, followed by the one-process/one-mapping thread adapter |
| Valgrind | Ubuntu 24.04 / GCC | Core and the one-mapping thread adapter with explicit error/leak exit policy |
| Native ARM64 macOS | macos-15 | `uname -m` must be arm64; SDK/compiler observation, full native lane, manual lint, local Homebrew source formula/install/test |
| FreeBSD guest | FreeBSD 14.4 / AMD64, QEMU | Opt-in polling-only CMake tests, real-process SPSC and 4P/4C NCQ, platform contract, demo, relocated installation |

The TSan job intentionally uses a separate Ubuntu baseline; the source records its actual compiler/runtime. A runtime mapping failure or an unsupported sanitizer flag fails the lane. The runner does not disable ASLR, retry until a lucky startup, change kernel sysctls or relabel an exit code as clean. Detector activation requires both the expected exit and an actual `ThreadSanitizer: data race` report. The production adapter uses no project suppressions or invented synchronization annotations. Same-address-space instrumentation is not an interprocess proof. [R12]

Valgrind's selected scopes are ordinary C processes; its leak checks do not understand the logical ownership of every shared slot. Sanitizer and Valgrind runs are not performance samples. OS package installation occurs in CI's disposable runner and supplies development tools, not dependencies of the native fast path.

All action uses are pinned to complete source commits: checkout `08c6903cd8c0fde910a37f88322edcfb5dd907a8`, setup-python `e797f83bcb11b83ae66e0230d6156d7c80228e7c`, upload-artifact `b7c566a772e6b6bfb58ed0dc250532a479d7789f`, and freebsd-vm `77ed28d336d03fe19a3f4f7266c1d2c4714dd79d`. These are selected revisions, not claims to be the latest versions. Monthly Dependabot proposals still require human review and an intentional manifest update. [R3–R6]

The FreeBSD VM action is third-party infrastructure. Pinning its source does not freeze the OS image, package repository or GitHub host kernel. The job disables its remote VNC/error debugging and image cache, requests 2 virtual CPUs/4 GiB, synchronizes results, and records guest identity. It does not qualify FreeBSD parking, Apple PMC events, Python's FreeBSD exporter, or bare-metal performance. The new FreeBSD source path remains NOT_RUN locally.

Hosted macOS is not asserted to be the lab's M3 Max, nor does an ARM64 runner label establish a physical core count, exclusivity, P-core placement or cache-transfer latency. CI therefore tests behavior, not a universal speed threshold. The earlier raw-trace campaigns remain the performance instruments. [R1, P7]

## 4. CMake, installation and package recipes

The CMake frontend builds the same seven native implementation units with C11, PIC and the warning-as-error profile. It can build static, shared or both library variants. It supplies `elite::ringbuffer_static`, `elite::ringbuffer_shared` and a generic `elite::ringbuffer` imported target, with the latter selecting shared when both are installed. C++ consumers include the opaque C-compatible API; they never overlay independent C++ atomics on the shared format.

The install contains the three actual public headers, library artifacts, version/config/export metadata, pkg-config data, both manuals, attribution/license, and optionally the live executable. The relocated demo uses a platform-relative library path. `find_dependency(Threads)` and the Linux link requirements are represented in the imported interface. Darwin retains a macOS 14.4 minimum for its existing wait API, an @rpath install name, and the strict-LDAR selection option on ARM64. The new build does not silently substitute native CPU flags for a portable target. [R7]

Local tests install to a temporary prefix, move that prefix, configure independent C and C++ clients, compile/run them, run the installed demo, verify both man pages, and validate the relocated pkg-config prefix. Both compiler/both-library combinations and static-only/shared-only configurations are exercised separately. This catches absolute build-tree paths that a normal in-tree example cannot detect.

### 4.1 Homebrew without an invented public URL

`elite-ringbuffer.rb` is a local seed adapter requiring explicit source URL, source SHA-256 and homepage inputs. `packaging/homebrew/elite-ringbuffer.rb.in` is the literal formula template. `tools/seed_packages.py` derives an actual ZIP digest and emits a ready-to-review literal formula from the owner's `OWNER/REPO` and intended HTTPS archive URL. Local mode uses the existing archive's file URI. No nonexistent release URL, dummy published checksum, bottle or tap submission is claimed. [R8]

The generator rejects unsafe archive paths, symlinks, multiple roots, wrong version, unsuitable URLs and output replacement. Its source-structure check does not replace verification of the complete release manifests/capsule. Formula tests build a version client and execute both public demo modes. The formula packages C libraries and the demonstration, not an untracked Python extension for a different interpreter.

The macOS CI step packages the exact verified checkout into a temporary archive, derives its literal recipe, and invokes a temporary local tap's install/test. Those steps are configured, not locally executed here. Ruby syntax and formula generation are local checks with a deliberately narrower scope.

### 4.2 vcpkg overlay

The root `vcpkg.json` and `packaging/vcpkg/elite-ringbuffer` describe a local Linux/macOS 64-bit overlay. The overlay uses the complete authenticated source tree; it honors static/shared triplet selection, invokes the official CMake/config/pkg-config fixups, and installs the license and attribution. Unsupported Windows/FreeBSD package-manager profiles are not offered. Native FreeBSD CI is a separate experimental CMake path. [R9–R10]

This is not a public registry entry or a promise that `vcpkg install elite-ringbuffer` works without the supplied overlay. No vcpkg installation was executed in the current environment. Underlying CMake configure/build/install/relocation was executed, which is useful evidence but not equivalent to the package manager's own lint and deployment logic.

## 5. UNIX manuals and operational safety

`man/elite_ringbuffer.3` documents the actual API: construction and grants, read/write lease lifetimes, tracked views, all status and transfer outcomes, waits, retirement, child accounting and destruction. It states the two happens-before chains and the publication-before-tail NCQ behavior. It distinguishes an operation status from a transfer outcome: a notification error after publication does not mean that a message can safely be resent.

`man/eliteipc.7` explains the architecture, exact layout profile, process trust boundary, finite-capacity behavior, signal policy, benchmark vocabulary, installation and evidence scopes. ASCII diagrams remain plain text, editable and renderable by ordinary manual readers. This is not a separate user-facing executable named `eliteipc`; it is a section 7 overview.

The manuals preserve the difficult but essential recovery boundary. SIGKILL does not run an abort/finalizer. A stopped process is not fenced, unlink does not revoke an existing mapping, and a timeout cannot transfer a token held by a resumable raw-pointer writer. Managed recovery retains old resources and constructs a physically distinct generation under the existing four-object/two-quarantine budget. A quarantined read release may return RETIRED/RETAINED; unavailable storage is not described as repaired capacity. [P4, P14]

The manual checks executed locally validate required sections, balanced display modes and names against the public header. Native `mandoc`/groff rendering was not available. The workflow explicitly runs `mandoc -Tlint -Werror`; a local download attempt failed before a formatter could be installed and its log remains. A structural check is not relabeled as a formatter pass. [R11]

## 6. Live demonstration: what the numbers mean

The new executable uses two independently constructed POSIX rings and one separately exec'd responder. Each direction has one producer/consumer, including the NCQ mode; it is not a contended multi-client latency benchmark. Each payload contains eight checked 64-bit test words. The request and reply both pass through the public reserve/commit/borrow/release functions. Pipes carry bootstrap grants and cleanup receipts, not payload data.

At the origin, sample i begins immediately before request reservation and ends after validation and release of the reply:

`RTT[i] = tick_after_reply_release[i] - tick_before_request_reserve[i]`.

Each window stores every sample, sorts its complete set, and uses exact nearest-rank p50/p99. Nanosecond display uses overflow-checked outward integer rounding through the queried timebase. Darwin uses `mach_absolute_time`; Linux uses `CLOCK_MONOTONIC_RAW`; the experimental FreeBSD path uses `CLOCK_MONOTONIC`. Clock scale is not physical uncertainty. Compiler/instruction timing barriers and all public-API/payload work remain in the samples.

Directional rate is `2 * completed_RTTs * 1e9 / window_ns`, explicitly two delivered records per round trip. It is not request/s, it is not an inverse-latency estimate, and dividing RTT by two is never used to populate a one-way column. The cohort window starts just before the first request and ends at the last completed reply. Sorting, console rendering and JSON writing happen between windows, outside that window's denominator, but can perturb later cache/scheduling conditions.

The ASCII console shows actual window rate, p50/p99 RTT, checked count and progress. Redirected output is plain; a terminal can use the in-place bar. `q` followed by Enter, SIGINT or SIGTERM request stopping at a completed RTT; they do not abandon an owned slot halfway through the transfer. Normal end uses an untimed stop record, child detach receipts, parent detach, terminal reaping and object destruction. Successful cleanup is required before a COMPLETE record. Early clean cancellation has its own status and exit 130 and is not accepted as a complete planned run.

There is no warmup or automatic P-core pinning. The displayed observations are an interactive demonstration, not the preregistered one-way/open-loop qualification. Its cooperative timeout checks do not constitute a bound on an arbitrary stalled machine primitive. CI supplies an additional owned-process command timeout. A SIGKILL of the coordinator cannot run its cleanup handler; the demonstration does not replace the full application recovery authority. [P5, P7]

### 6.1 Independent receipt verifier

`--json NEW_FILE` writes a JSONL header, every raw `(start, delta)` sample per window, and a final cleanup/quota record. Existing output paths are refused. `tools/verify_live_demo.py` uses Python's standard library to reconstruct timebase arithmetic, population, nondecreasing intervals, nearest ranks, directional rate, exact checked counts and planned completion. It rejects booleans posing as integers, duplicate keys, nonfinite values, forged rate/quantiles, missing samples and missing cleanup. These checks also run under optimized Python.

All integer observations and ceil-quantile results must match exactly. The rate expression allows four binary64 ULPs for its specified native floating operations and JSON serialization; that tolerance is not a physical clock-confidence bound. The verifier does not authenticate an execution host or replay every original payload load from the timing file. Native payload checking plus preserved source/binary hashes supply the stated execution custody, not remote attestation.

## 7. New execution ledger

| Lane | Executed outcome | Scope |
|---|---|---|
| Strict GCC 14.2 full release-check | Exit 0; **345 Python discovery tests**, native gates and separate new surface tests passed | Includes the inherited 317 and 28 new tests; repeated targets are not extra independent cases |
| Strict Clang 17 full release-check | Exit 0; same **345-test** discovery passed | Independent local compiler, not Apple Clang |
| Live demo suite | **12 tests passed** | Two modes, exact quantiles, semantic corruption, output no-overwrite, clean cancellation, optimized Python |
| Seeding surface suite | **16 tests passed** | Workflow/source checks, Ruby syntax, package generation and rejection, manual/API correspondence |
| Formal current campaign | Seven complete graphs; **2,158,499 states / 5,700,615 transitions** | Same bounded models and new reviewed binding; 18 record population includes designated negative/auxiliary outcomes |
| Formal independent receipt replay | Passed all structural witnesses and auxiliary results | Replay uses the supplied model; no external model checker was invoked |
| CMake/CTest | GCC and Clang dual-linkage; GCC static-only and shared-only configurations passed **six CTests each** | Actual native core/platform/IPC/demo behavior |
| Relocated installation | Four configurations passed C/C++ consumers, installed demo, man inventory and pkg-config prefix checks | Explicit move of the installed prefix |
| Recorded live demonstration | Four conditions, **400,000 RTTs / 800,000 directional records**, all replayed | Five 20k-RTT windows per compiler/mode; no warmup or placement control |
| GCC O1 ASan/UBSan | Core, eight adversarial groups and both 2k-RTT demo conditions passed | Leak detection enabled for these normal-exit C processes; not performance observations |
| GCC TSan | Deliberate activation race reported with exit 66; one-mapping adversarial adapter passed | No suppressions; not an interprocess/FFI proof |
| YAML parsing | PyYAML BaseLoader plus explicit shape checks passed | Not actionlint or GitHub validation |
| Native macOS / FreeBSD / hosted jobs | **NOT_RUN here** | Supplied runnable native/guest jobs, not target qualification |
| Valgrind, Homebrew, vcpkg, man formatter | **NOT_RUN here** | Tools absent; Ruby syntax and underlying CMake tests have their narrower scope |

### 7.1 Actual live demonstration observations

The four primary demo runs were allowed to overlap ordinary functional checking on the same constrained Linux host. They are examples of live, correctly accounted displays, **not a matched compiler or hardware comparison**. Each row gives medians of five per-window rates or quantiles, not a pooled p99 or confidence bound.

| Compiler | Mode | Directional Mmsg/s | Median window p50 RTT, ns | Median window p99 RTT, ns |
|---|---|---:|---:|---:|
| gcc | spsc | 2.262737 | 739 | 978 |
| gcc | ncq | 1.748443 | 952 | 1293 |
| clang | spsc | 2.248859 | 741 | 944 |
| clang | ncq | 1.809800 | 916 | 1278 |

Every raw sample and final count is retained in `evidence/turn16/live_admitted/`; `LIVE_RESULTS.csv` is a convenience summary. No historical M3 Max number was inserted into the display. Sanitizer demo numbers are kept only in their separate diagnostic logs.


The local host and every compiler/tool observation are in `evidence/turn16/ENVIRONMENT.json`. Historical tests, current demo samples, bounded formal states and lab-reported Apple runs are different populations. No combined headline count disguises them.

## 8. Failed and unavailable checks retained

The initial demo build failed a misleading-indentation diagnostic in its newly written display loop; braces repaired it before accepted runs. A CMake attempt reused a directory with the wrong generator; a fresh directory resolved that setup error. The initial install helper tried a nonexistent consumer executable name even though the package imported and built; the helper now executes the generated independent C/C++ CTest tests. The subsequent relocation checks passed.

The first full GCC and Clang discovery runs reached their formal receipt test and failed because the new full-file source binding was not the old receipt's source set. The new bounded campaign resolved that gap by actual re-execution. The original failed logs remain named separately and are not counted as final passes. Nothing in the graph oracle or source binding was weakened to forgive stale provenance.

GitHub-hosted jobs, the FreeBSD VM, native macOS execution of this derivative, Homebrew install/test, vcpkg deployment, actionlint and local man-page formatter execution remain NOT_RUN. A missing tool is a visible capability gap. The CI definitions are runnable artifacts awaiting the owner's first hosted run, not badges proving success.

## 9. Reproduce, seed and publish

From a clean extracted source directory:

```sh
python3 tools/verify_release.py --strict
make CC=clang CXX=clang++ BUILD=build/native release-check check-seeding
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Release -DELITE_BUILD_TESTS=ON
cmake --build build/cmake --parallel 2
ctest --test-dir build/cmake --output-on-failure
build/cmake/live_throughput_demo --mode spsc --windows 10 --messages 100000 --json live-spsc.jsonl
python3 tools/verify_live_demo.py live-spsc.jsonl
```

Use a new raw output path and separate build directories when compiler/flags change. Installed clients use `find_package(elite_ringbuffer CONFIG REQUIRED)` and link `elite::ringbuffer`; the C/C++ header contract remains unchanged. `make install-man` and CMake installation both supply section 3/7 pages.

The seeding guide gives the local workflow runner, optional static/shared builds, vcpkg overlay command and literal Homebrew recipe generator. Repository identity and a public archive URL must be supplied by the owner. No GitHub tag, package registry, tap, signing key or remote release was created. Do not force-move an already published v1.1.0 tag to point at this new source derivative merely because the library API version is unchanged.

## 10. Integrity, packaging and delivery

The new source policy covers workflow YAML, Ruby recipes, CMake files/templates, vcpkg metadata, manual sources and every existing source category. The complete manifest covers every packaged file except itself, including the source manifest, this report and all retained evidence. The old sealed archive remains unchanged at its original digest.

The deterministic packer uses the verified inventory, sorted paths and fixed archive metadata. A detached capsule can bind its archive and internal manifest identities without a self-hash cycle. The requested `16_Delivery_Receipt.json` independently records the actual new report, archive, sidecars, completed checks and remote readback outcomes. SHA-256 receipts are unsigned integrity evidence, not author-authenticated signatures. [P15]

Post-package extraction/build/replay evidence is kept outside the archive it verifies. Download-back equality is recorded only after actual connector fetch and byte comparison. Upload acknowledgement alone is not the requested integrity check.

## 11. Acceptance and boundaries

The new developer-facing surfaces are implemented, tested where available, and tied to explicit bytes. Linux runtime validation, actual CMake relocation and the replayable demonstration support this deliverable. The separate native FreeBSD and hosted macOS jobs create falsifiable admission gates rather than advertising unrun cross-platform guarantees.

The queue still transfers exclusive ownership; publication is not a memory copy, phase is not membership, and timeout is not fencing. Finite formal evidence stays bounded. The live display cannot certify a latency target through prettier formatting. A recipe or workflow becoming part of the repository does not by itself mean that a package was publicly published or a target passed qualification.

## 12. Sources and authority

P15. Preserved `15_Turn_15_Definitive_Release_Candidate_Report.md`, `release/RELEASE.json` and parent integrity inventory.

P14. Preserved `14_Turn_14_Chaos_and_Crash_Recovery_Report.md` and `docs/CHAOS_RECOVERY.md`; especially retained ownership, external suspicion and post-fencing cleanup.

P13. Preserved `13_Turn_13_Formal_Verification_Report.md`, current `formal/SOURCE_BINDING.json`, and new `evidence/turn16/formal_admitted` with raw graph/negative outcomes. The current run does not imply external Spin/GenMC execution.

P7. Preserved `07_Turn_07_Hardware_Benchmark_Report.md` and `docs/BENCHMARKS.md`; request/reply versus one-way and counter-control distinctions.

P4/P5. Frozen ABI and verification-plan documents under `docs/reference/`; admission, lifetime, exact outcomes and independent verification lanes remain governing contracts.

R1–R13. Current primary-service/tool documentation and pinned action sources, with URLs and scopes, are recorded in `evidence/turn16/PRIMARY_SOURCES.md`. They establish the consulted tooling contracts, not completed results for this new project.
