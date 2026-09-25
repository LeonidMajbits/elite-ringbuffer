# Turn 15 — Definitive v1.1.0 Source Release Candidate
## ELITEIPC / LE128-V1 / release integrity and consolidated engineering record

**Project:** Leonid Majbits / Gemini Operator Lab, paired with Leon  
**Release:** v1.1.0 source candidate; wire ABI `0x00010000` unchanged  
**Evidence date:** 24 September 2026; exact local commands and outputs under `evidence/turn15/`  
**Disposition:** Consolidated, testable source candidate with sealed byte identity; not a universal deployment or timing certificate.  
**Canonical-SHA256:** `e21055ee9cbc47bacb7dfeb414916a9275cd942156a2f8947e288504d40baea9`
**Canonical convention:** Replace only this field's 64 hexadecimal characters with 64 ASCII zeros before hashing the complete UTF-8/LF document. The adjacent `.md.sha256` hashes all literal finalized bytes.  
**Authoritative parent:** `Elite_Systems_LockFree_RingBuffer_Turn_14_Chaos_Bundle.zip`; 48,040,449 bytes; SHA-256 `e06484c745af29c5d74e6f0a548824dd40b015265020e80614675d39d376fec5`.  
**Destination:** Deployment Research / Elite_Systems_LockFree_RingBuffer  
**Evidence vocabulary:** EXECUTED means a retained invocation of this derivative; INHERITED means retained predecessor evidence; LAB_REPORTED means the user's Apple account; NOT_RUN/INCOMPLETE remain unqualified. Hash agreement is byte integrity, not author authentication.

---

## 1. Release decision

Deliver **ELITEIPC v1.1.0** as the final consolidated source candidate for the
fifteen-turn campaign. It contains the native C11 SPSC and publish-first NCQ-SC64
implementations, managed POSIX backing, optional zero-copy Python bindings,
C++-compatible opaque ABI, topology and PMC diagnostics, benchmark matrices,
finite formal models, native chaos histories, and the independent receipt tools.
No new queue algorithm or memory-order optimization is introduced in this turn.

The seal has a precise meaning: every packaged member has a reproducible inventory
and SHA-256 identity, the release metadata agrees across language surfaces, and
the detached capsule binds the delivered archive to those identities. It does not
change an unsupported claim into a theorem. In particular, no same-generation
orphan recovery, universal false-sharing elimination, guaranteed latency bound,
standard-tool C11 refinement result, or exactly-once application side effect is
created by the version number. [P4, P5, P11, P13, P14]

The primary product distinction remains valuable. SPSC avoids a shared reservation
RMW in its polling path. NCQ obtains storage before constructing a message and
publishes its completed descriptor before advancing the ready tail. A paused
builder therefore holds capacity, not an unfinishable ready-queue reservation.
Python preserves both storage and lease lifetime, rather than exposing a raw
address and hoping that a garbage collector prevents reuse.

The original audit and all retained failed/incomplete records stay in the tree.
The dated 1.0.0 audit is historical evidence of defects that motivated hardening;
it is not silently deleted or edited into an all-green account. The current
README links the hardening and scope records instead of inheriting its old claims.

## 2. Source custody, change budget and version domains

The parent ZIP's literal digest, ZIP CRC integrity, all 7,811 full-manifest entries
and 114 source entries were verified before editing. This is a derivative of that
archive. The supplied short Git ID `a9bf0f7` was not independently fetched as a Git
object, and the packager did not push, sign or create a tag in Leon's repository.
`v1.1.0` is the tag-ready release identity expressed in files and documentation.

Four version domains must not be conflated:

| Domain | This candidate |
|---|---|
| Native source/library version | 1.1.0; encoded runtime number `0x00010100` |
| Python wrapper/exporter admission | Matching 1.1.0 / `0x00010100`; deploy together |
| Shared wire format | LE128-V1 / `0x00010000`, unchanged |
| Formal/benchmark receipt schemas | Existing named schemas; additive explicit wait-status ABI in new chaos plans |

The modifications to production-facing code are release metadata only:
`include/elite_version.h`, the version-return expression in `src/elite_version.c`,
the exporter's module API-version constant, and the Python version/admission
constants. The GC implementation, `_Pin`, ownership adoption, shared structures,
C status values, queue operations and phase/counter semantics are preserved.
`evidence/turn15/PRODUCTION_CONTINUITY.json` provides complete before/after hashes
and classifies changed files. All eight files in `formal/SOURCE_BINDING.json`
remain matched; that file is not regenerated to excuse a proof-affecting change.

The diagnostic chaos harness and offline verifier receive the recorded Darwin
portability correction in an explicit form (§7). Makefile current-version metadata
and the active version tests are updated. Documentation, the trust-capsule tools,
release metadata, new tests and new evidence complete the release delta.
Historical reports, numeric receipts and raw fixtures are not rewritten to claim
execution by the new version.

The preserved MIT license names Leonid Majbits. Algorithmic attribution in
`NOTICE.md` remains intact. No SDK, third-party sanitizer runtime, signing key,
compiled deployment library, or unrelated credential is deliberately bundled.

## 3. Fifteen-turn engineering history

The chronology below follows the retained artifacts, not invented releases.
`v0.1.0` in CHANGELOG is explicitly a retrospective planning label: no original
v0.1.0 executable or Git tag is established by the supplied record. There is no
fabricated v0.2.0–v0.9.0 tag sequence.

| Turn | Engineering decision or delivered mechanism | Evidence boundary retained |
|---|---|---|
| 1 | Separate SPSC, parkable SPSC and MPMC; reject volatile, ticket locks and bare FAA as proof | Planning only; no code |
| 2 | Select publish-first NCQ-SC64, dual QF/QR pool, two-way ownership proof, prior-art attribution | Parameterized abstract proof, not compiled execution |
| 3 | Kill-gate crash ownership gaps, no-live-wrap, post-LP access, scheduling and contention | In-place timeout reuse rejected; quantitative policies not measurements |
| 4 | Freeze LE128-V1 byte layout, atomics, lifecycle/grant rules and checksum definitions | One platform-admitted C ABI; no universal atomic overlay |
| 5 | Preregister V0–V8 lanes, exact membership, timing intervals, repetitions and failure oracles | Full planned programme not completed merely by later subsets |
| 6 | First authorized C11 implementation; strict builds, native IPC, parser and concurrency tests | Actual Linux results; early incomplete large attempt retained |
| 7 | Native 100M RTT/goodput harnesses, raw replay, Darwin shm permission fix | One full Linux trial per condition; different Apple resources |
| 8 | 1.0.0 source packaging, opaque C++ interface, ctypes and buffer exporter | New FFI subsequently falsified by audit; version not certification |
| 9 | Adversarial audit: pending export, cycles, manifests, compiler promotion, hook resurrection and acquisition interruption | Concrete reproductions, not rhetoric about hypothetical hazards |
| 10 | 1.0.1 hardening: pending-export transactions, cyclic GC, cleanup-state separation, safe finalization and uncertain acquisition retention | No timeout token repair; repaired binding tested within its scope |
| 11 | Complete raw cache timing/counter provenance and optional PMC engine | Real local PMU events unavailable; synthetic backend labeled |
| 12 | Topology discovery and eight-axis contrast matrix with exact trace/token replay | Host limits, skipped topology cells and adverse workloads remain visible |
| 13 | Executable exact finite models, handoff graphs, mutations and counter boundaries | Seven completed populations; larger bound and external tools still open |
| 14 | True multi-process faults, external suspicion, safe successor isolation and stable final partitions | Pending messages and unavailable old tokens are not relabeled delivered |
| 15 | Consistent v1.1.0 metadata, cross-OS wait replay, consolidated docs and cryptographic integrity capsule | Source candidate sealed; origin signing and deployment approval remain owner actions |

Primary references P2–P14 below resolve to retained files in this tree. The separate
user-provided Apple accounts are recorded under `release/LAB_RECEIPTS.md` and are
not substituted for unprovided raw trace inventories or the exact current binary.

## 4. Architecture invariants

### 4.1 SPSC: two ownership transfers

Let P count fully published messages and C fully returned messages. Only the
producer writes P and only the consumer writes C. Capacity is `0 <= P-C <= N`.
A private write reservation does not advance P, and a live read lease does not
advance C. Cached covering acquires are conservative: an old value may postpone
availability, but cannot authorize a future publication or return. [P2, P4, P13]

For a message k and covering observations v>k and u>k:

\[
W_k \to_{sb} P_{release}(v) \to_{sw} P_{acquire}(v) \to_{sb} R_k,
\]

\[
R_{k,last} \to_{sb} C_{release}(u) \to_{sw} C_{acquire}(u)
\to_{sb} W_{k+N,first}.
\]

These imply the required happens-before relations under the admitted atomic and
client model. Every valid alias must end before return. Multi-line payload bytes
need not be individually atomic because ownership orders conflicting accesses.
A checksum, a later epoch validation, or an unrelated lifecycle acquire is not a
substitute for either chain. No ordinary block access is permitted after transfer
linearizes, even if the former owner's function has not yet returned.

### 4.2 NCQ-SC64: QF/QR, not a claim-first payload queue

QF carries free block IDs; QR carries completed-message IDs. All queue heads,
tails and packed entry words are single-width SC atomics. A successful head CAS
is a removal LP; a complete entry-install CAS is a publication LP. Tail advance
comes afterward and can be helped. CAS losers invalidate their whole dependent
observation, not just the scalar expected argument. [P2, P4, P13]

For each queue with head H, tail T and ghost publication frontier U:

\[
H\le U,\qquad 0\le U-H\le N,\qquad U-1\le T\le U.
\]

`H=T+1` is legal after a consumer removes an installed entry before tail help.
The model reaches that state. Calling it corrupt would reject valid behaviour.
Token conservation is over disjoint live categories, not every historical word
left in an entry array:

\[
N=|F|+|Q|+|W|+|R|+|T_{transfer}|+|X_{retained}|.
\]

COMMITTED may precede QR insertion; EMPTY may precede QF return. Neither phase
is an independent recovery oracle. FIFO is publication order, not producer-ID
order, consumer completion order, or ordered external side effects.

### 4.3 Identity, memory geometry and platform admission

Tickets and epochs never wrap during a live generation. The checked limits are
`J=2^64-N-1` and `G=2^62-2`. Publication must check its successor before installation,
not discover overflow after publishing an entry it cannot finish. Normal physical
reuse is allowed; repeated logical identity while stale observers exist is not.

LE128-V1 retains 2,048-byte headers, 512 immutable CRC32-covered bytes, 128-byte
cells/descriptors, 256-byte participant records and a 16,384-byte format quantum.
It uses relative offsets and ordinary shared pages, not a huge-page guarantee.
Compile-time assertions and runtime range/stride checks establish the format for
an admitted compiler/platform tuple. Alignment does not make a pair of words
atomic, remove true sharing, authenticate a participant, or create a time bound.

The native implementation assumes coherent normal CPU memory and address-free
32/64-bit atomic operations on aliases, with construction complete before exposure.
C++ calls C rather than redefining shared `std::atomic` objects. Payload schemas
must not embed process-local pointers. GPU/DMA ordering is outside this CPU-only
handoff contract. [P4]

## 5. Python lifetime and native API boundary

The binding remains a standard-library ctypes facade plus the project-owned
CPython exporter. A native payload buffer is not copied into an intermediate
Python bytes object merely to cross the transport. This does not eliminate Python
objects, per-call FFI cost, serialization work, or an explicit copying convenience.

The hardened exporter counts pending acquisition before any Python callback can
release the GIL. Failure paths roll back that count. Public close/commit/return
cannot unpin storage while pending or issued exports remain. Slices and casts
retain CPython's underlying managed buffer, not just the outer Python lease.

GC traversal sees the exporter/container references. Separate native cleanup state
has no strong return path through the public wrappers, so unreachable cycles can
be collected without clearing native data required to end a lease. Finalization
is resurrection-aware, and a custom unraisable hook is not given a freed exporter.
Uncommitted abandonment aborts when safe; it never silently publishes partial data.
An interrupted acquisition with uncertain native outcome is retained and poisoned
rather than abandoned as though no token had been obtained. [P10]

The native API remains single-owner/nonreentrant per endpoint. The Python facade
serializes endpoint calls locally, but arbitrary simultaneous accesses through raw
buffer aliases remain the application's responsibility. Main-interpreter,
GIL-enabled CPython is the admitted exporter runtime. Free-threaded builds,
subinterpreters and alternate interpreters are not silently equivalent.

v1.1.0 updates the wrapper, native runtime and exporter admission identity together.
This strict matching policy intentionally rejects mixed 1.0.1/1.1.0 components.
The wire format does not change, but source-version compatibility does not prove
safe rolling upgrades of active endpoint cohorts.

## 6. Benchmarks and formal evidence: exact inherited scope

### 6.1 Historical native observations

The Linux data below are retained Turn 7 measurements from an Intel Xeon Platinum
8370C container with five allowed logical CPUs, a four-CPU-time quota and endpoint
placement on CPUs0–3. The Apple values were supplied by Gemini for bare-metal ARM64
runs. They are descriptive rows, not matched-hardware speedup estimates. [P7, U]

| Profile | Provenance | RTT p50 | RTT p99 | RTT p99.99 | RTT maximum |
|---|---|---:|---:|---:|---:|
| SPSC 64B 1/1 | Linux retained | 660 ns | 864 ns | 277.748 us | 213.536 ms |
| SPSC 64B 1/1 | Apple lab-reported | 250 ns | 625 ns | 7.21 us | 1.33 ms |
| NCQ 64B 1/1 | Linux retained | 805 ns | 1,060 ns | 410.436 us | 145.048 ms |
| NCQ 64B 1/1 | Apple lab-reported | 375 ns | 1,250 ns | 11.88 us | 1.84 ms |

Each native RTT condition means 100M completed round trips and 200M directional
records, not 100M one-way samples. RTT/2 is not substituted for the specified
one-way interval. The often quoted roughly38.5x/34.6x comparisons concern p99.99,
not maximum latency, and they cannot establish a universal architecture advantage.

| 64B goodput condition | Linux retained messages/s | Apple lab-reported messages/s |
|---|---:|---:|
| SPSC 1/1 | 9,480,193 | 32,066,903 |
| NCQ 1/1 | 7,669,929 | 26,599,872 |
| NCQ 8/8 | 3,575,176 | 4,105,985 |

Application GB/s counts delivered payload once, not DRAM bandwidth or doubled
read/write traffic. The separate Python1-MiB figures—22.66 and22.79 GB/s—are also
lab-reported and involve compiled full-span payload work. They do not describe
Python-bytecode serialization or every payload size. No new Turn15 nanosecond or
bandwidth result is manufactured from the regression suite.

Turn12's retained AMD EPYC constrained-host matrix is a separate experiment. It
includes adverse NCQ16P/1C observations near8.2–8.5k messages/s and roughly16ms
median-of-trial offered p99 under seventeen workers on a four-CPU pool. This is a
real service-envelope warning, not something the final README should hide. Cache
capacity, CPU NUMA placement and actual memory-page placement remain distinct.

### 6.2 PMC/timebase status

Turn11 records160 cache-control receipts and3billion exact atomic increments, with
raw timebase arithmetic and per-worker counters. Local perf groups failed ENOENT;
synthetic successful backends were labeled synthetic. Darwin code supplies public
CPU accounting and optional CNTVCT/Mach correlation, not a generic raw Apple
L1D/LLC/coherence-event implementation. CNTVCT is a virtual system timer, not core
cycles. No evidence supplied for this final seal establishes a measured M3 Max
coherence-invalidation explanation for a historical cache ratio. [P11]

### 6.3 Formal proof and exploration status

Turn13's seven exact bounded graphs contain2,158,499 states and5,700,615
transitions. Every accepted search exhausted its frontier and passed its specified
ownership/nonprogress checks. The SPSC handoff graph separately tests the required
release/acquire relationships; the SC ownership explorer is not secretly a general
C11 memory-model frontend. [P13]

Structural mutants expose double allocation, stale observation reuse, early tail
publication, post-LP access and early recycling. Removing QR synchronization
exposes an ordinary epoch read before status acquire. Relaxing strong CAS does
not itself split its atomicity; a positive control preserves unique claims.

The N4/2P2C/eight-reservation extended case remains incomplete at its retained cap.
Spin/GenMC/CDSChecker execution and a complete machine-checked source refinement
are not supplied by those results. Parameterized source-level arguments remain
conditional on the admitted primitive and client model. This candidate preserves
those limits while making the work reviewable and executable.

## 7. Chaos survival and the Darwin replay correction

The retained primary Turn14 evidence comprises186 cases:180 true multi-process
histories plus six resource negatives, 12,052,618 ordinary message reads,1,620 child
instances and8,626 sent signals. Signal count is not handler-delivery count.
Surviving NCQ peers with spare tokens completed quotas around stopped/killed
holders; committed entries remained consumable before the original helper returned.
No partial victim payload was promoted to valid ready membership. [P14]

The latest lab account reports M3 Max validation of285 tests and ten smoke profiles
with156,000 messages. That is valuable target evidence, retained as LAB_REPORTED;
it is not silently expanded to the entire original100/1,000-repeat fault catalogue.
The raw Apple receipts and Git object `a9bf0f7` were not provided as this task's
independently verified source base.

### 7.1 Portability fix in evidence decoding

A saved wait status belongs to the generating OS, not the interpreter replaying
it. Darwin encodes a continued observation with low7 bits0x7f and high bits0x13;
on Linux that high value is SIGSTOP. Calling the local `os.WIFSTOPPED` on a saved
foreign-OS trace can therefore misclassify it. Apple's wait header explicitly
separates these cases. [R1]

New native chaos plans record `wait_status_abi` as linux or darwin. The offline
verifier decodes that specified representation, validates the platform's named
signal numbers, and requires an actual SIGSTOP stop for the controlled witness.
It never treats a stop or continued observation as terminal fencing. Untagged
historical v1 fixtures have a documented legacy-Linux interpretation; untagged
foreign traces are not guessed correct from one magic value. Synthetic transformed
Darwin fixtures test cross-platform replay without pretending to execute Darwin.

This affects test/verification source only. Production `waitpid`/owned-child
handling continues to use the target platform's native interface. The correction
reconstructs the lab-reported issue rather than claiming to have fetched its
exact patch or Git commit.

### 7.2 What recovery preserves—and what it does not

A heartbeat timeout is absence-of-progress suspicion, not revocation. The failure
history is indistinguishable from a writer that will resume with its raw pointer.
A generation change or memory fence does not undo a later store through that
pointer. Safe replacement keeps old storage alive and uses physically distinct
backing without redirecting live old virtual aliases.

A quarantined reader's release can return RETIRED/RETAINED rather than putting its
token into QF. A previously admitted QR publication can still linearize in the
old object after retirement. The asynchronous oracle checks that known published
IDs equal checked reads plus pending QR messages; it does not call pending old
messages delivered. Final block accounting partitions QF, QR and unavailable
responsibility after every holder is terminal. It does not scan live ordinary
metadata and reinsert unknown blocks. [P14 §§6–7]

All four objects/two quarantines remain charged until safely disposed. A failed
sole SPSC endpoint requires a new generation; NCQ service cannot continue after
all tokens are retained merely because its queue primitives are lock-free.
Authority takeover, unregistered descendant mappings, malicious shared writes,
host power loss and exactly-once external effects remain separate requirements.

## 8. Current release validation

The current host is Linux/x86-64, Python3.13.5, GCC14.2.0 and Clang17.0.0;
full executable/compiler queries and allowed CPU IDs are in ENVIRONMENT.json.
The following are new executions, not inherited Apple results:

| Lane | Completed result | Scope |
|---|---|---|
| GCC strict build and release-check | Exit0; native gates and **317 Python discovery tests passed** | Includes36 original binding tests; repeated Make targets are not extra disjoint cases |
| Clang strict build and release-check | Exit0; same317-test discovery passed | C11 warning-as-error, static/shared libraries, Python exporter and C++ example |
| Clang PMC/topology/matrix diagnostics | Native unit/backends,39 cache oracle tests,1,004 rational cases,21 topology tests and50 matrix tests passed | No new measured Apple event counts or performance distribution |
| New portability tests | **14 passed** | Linux/Darwin status semantics and synthetic foreign-OS trace replay |
| New trust-capsule tests | **18 passed** | Valid archive, altered evidence, unsafe paths, signatures, canonical hash, repeat packing and optimized-Python rejection |
| Retained formal source binding | All8 reviewed files matched | No automatic source refinement or new seven-space exhaustive claim |
| Fresh GCC persisted chaos smoke | **10 cases;156,000 ordinary messages; replay passed** | Actual separate-process signals/leases/successor and one resource negative |
| Fresh Clang persisted chaos smoke | **10 cases;156,000 ordinary messages; replay passed** | Independently compiled current derivative |
| GCC O1 ASan/UBSan | Core68 plus **10 chaos cases /17,200 ordinary messages** passed | Leak detection requested for normal-exit C processes; killed children do not finalize |
| Staged install and examples | C, C++20 and Python SPSC/NCQ examples passed | Local temporary prefix, no system or Mac install |

The317-test discovery includes the32 new portability/capsule tests. It does not
mean317 new independent concurrency cases. The two persisted ordinary-compiler
chaos runs total312,000 checked messages; sanitizer and temporary Make smoke
messages are not pooled into that total.

The first trust-capsule test fixture omitted several members required by the
stronger archive verifier. It failed18 setup paths; the fixture was repaired to
include both declared required sets. The original failure log is retained. The
verifier's required-file checks were not weakened. A streaming execution request
was unavailable before launch; the actual builds used recorded native subprocesses.
Unlike the inherited Turn14 interrupted combined invocation, both current complete
release-check commands returned0. No new full100M latency sweep, native Apple
execution, new TSan IPC proof or external formal-checker run is claimed.


No current test count is pooled with inherited100M samples, historical model
states or Apple-reported results. A sanitizer is not a performance run and a clean
one-process race lane is not an interprocess proof. Builds are from separate GCC
and Clang directories; installation tests use temporary prefixes, not system-wide
changes. Exact final archive extraction/rebuild/replay happens after sealing and
is documented in detached post-package evidence so it cannot retroactively alter
the archive it verifies.

## 9. Cryptographic trust capsule

### 9.1 Integrity graph

`SOURCE_MANIFEST.sha256` covers the explicit source/build/license/audit policy.
`MANIFEST.sha256` covers every packaged regular file except itself, including the
source manifest and all included evidence. The report contains a canonical
zeroed-self-field SHA-256 and has a separate literal-byte sidecar. The detached
`15_Turn_15_Trust_Capsule.json` binds the final ZIP, manifest identities/counts,
report identities and release metadata. The detached delivery receipt then records
actual local/remote verification and file sizes.

The packer does not place its own ZIP hash inside the ZIP. The complete manifest
cannot contain its own ordinary hash without a separate convention, so its digest
is bound externally. This is an acyclic integrity chain, not a claim to have solved
a self-hash fixed point.

### 9.2 No invented digital signature

These artifacts are **unsigned SHA-256 integrity receipts**. A digital signature
uses an authenticated signing identity; hashing alone cannot prove who created
the bytes. The capsule explicitly rejects unsupported signed/attested claims. No
private key is generated on behalf of Leon, no production signing key is accessed,
and no remote Git tag is created. [R2]

Users can pin the capsule's digest from a separately trusted channel. Without that
anchor, internal consistency remains useful for detecting accidental corruption
but is not protection against someone replacing both data and hashes. Signing a
reviewed Git tag or the detached capsule is an owner-controlled subsequent action,
not something the packager falsely reports completed.

### 9.3 Verification implementation

`tools/verify_trust_capsule.py` is standalone Python3 standard-library code. It
streams ZIP members without extracting, importing or executing them. It enforces
one root, regular files, resource limits, complete inventory, source-policy
coverage, member hashes, canonical report math and version identity. Duplicate
keys, NaN/infinity, unsafe paths, symlinks, generated overlays and false signature
status fail closed. Optimized Python cannot disable those conditions.

`tools/build_release_archive.py` packages only the already verified inventory.
Names are sorted; timestamp/root/modes are fixed; repeated packing with the same
Python/zlib implementation is deterministic. A different compiler/debug path or
SDK may legitimately produce a different binary; no universal reproducible-binary
claim follows. Existing output paths are refused, and an interrupted attempt
cannot produce a valid success capsule. Newly compiled executables are not shipped
as target-neutral binaries.

## 10. Production integration and publication runbook

Verify the detached capsule and trusted archive digest before executing extracted
code. Then run the strict in-tree verifier, build the actual target tuple, and
execute its relevant tests. Use separate build directories on compiler/flag changes.
Linux produces `.a`/`.so`; Darwin produces `.a`/`.dylib` with1.1.0 current-version
metadata and the preserved compatibility version. Python needs its matching
interpreter exporter and native library. [docs/INTEGRATION.md]

```sh
python3 tools/verify_release.py --strict
make CC=clang CXX=clang++ BUILD=build/release libraries python benchmarks
make CC=clang CXX=clang++ BUILD=build/release release-check
make version
```

The new README supplies Mermaid diagrams plus concrete C/C++/Python entrypoints.
Native applications must distinguish status from transfer outcome: a post-LP
notification error does not make the message unsent. End every alias before
return. Do not reuse an active output lease for another reserve. Use bounded
application admission and a schedulable authority, not indefinite polling against
a lost role or an exhausted resource budget.

For native cohort reproduction, use `run_benchmarks.py` and its separate trace
verifier. For cache results use the v2 provenance tools, and for topology/arrival
conditions use the matrix runner. `run_formal.py` and `run_chaos.py` keep complete,
failed and cutoff results distinct. New directories preserve provenance instead
of replacing an earlier unfavorable run. Full100M raw sets and large model graphs
require their documented memory/disk/time resources.

Install into a new immutable version-specific prefix. Do not overwrite a library
currently loaded by Python or reinterpret a live shared generation during an
upgrade. Run a fresh cohort under the desired application contract before switching
service. Preserved wire compatibility does not prove mixed-release operational
compatibility.

To publish, the repository owner should review the final diff against the known
commit, run native Apple validation of this exact derivative, sign or tag the
chosen commit using their trusted configuration, and attach the sealed archive,
capsule and receipts. Do not force-move an existing tag. This turn prepares that
release; it does not claim remote publication or independent peer review.

## 11. Acceptance and remaining evidence boundaries

The source candidate can be sealed and delivered without lowering its core
invariants. The campaign supplies substantial evidence of defined ownership,
process-lifetime safety, bounded-state correctness and reproducibility. It also
supplies the counterexamples that forced the original Python fixes and the
unfavorable service measurements that prevent universal performance claims.

The remaining limits are explicit: no complete source-to-C11 external-checker
refinement, no exhausted expanded V1/lifecycle/parking universe, no complete
original repeated V5/V6/V8 qualification, no established generic Apple coherence
PMC attribution, and no universal15/50-ns or population-recovery guarantee.
Available local evidence and lab-reported Apple evidence are not equal to newly
executed native Apple tests of this final derivative.

A byte seal is final for this archive. A future valid counterexample still takes
precedence over its version string or pass counts and requires a newly identified
candidate. The useful assurance is that uncertainty never becomes permission to
reuse memory, and incomplete evidence never becomes a fabricated pass.

## 12. Sources and evidence inventory

[P1] Retained Turn1 architecture, proof obligations and source ledger, consolidated
under `docs/reference/` in this release; all were planning documents.

[P2] `docs/reference/02_Turn_02_Cache_Topologies_and_Barrier_Proofs.md`.
[P3] `docs/reference/03_Turn_03_Concurrency_Kill_Gate_and_Failure_Modes.md`.
[P4] `docs/reference/04_Turn_04_C11_ABI_and_Assembly_Specification.md`.
[P5] `docs/reference/05_Turn_05_Verification_Harness_and_Test_Plan.md`.
[P6] `06_Turn_06_Production_Implementation_Report.md`.
[P7] `07_Turn_07_Hardware_Benchmark_Report.md` and retained100M summary/manifests.
[P8] `08_Turn_08_Enterprise_Release_Report.md`; later audit supersedes its
unqualified lifetime conclusions for the audited1.0.0 artifact.
[P9] `docs/ADVERSARIAL_AUDIT.md`, completed audit revision2.
[P10] `10_Turn_10_Hardened_Release_Candidate_Report.md` and hardening evidence.
[P11] `11_Turn_11_PMC_Cache_Provenance_Report.md`.
[P12] `12_Turn_12_Hardware_Topology_and_Matrix_Report.md`.
[P13] `13_Turn_13_Formal_Verification_Report.md`.
[P14] `14_Turn_14_Chaos_and_Crash_Recovery_Report.md`.
[U] User-provided commissioning/qualification accounts, separately labeled in
`release/LAB_RECEIPTS.md`; not independent acquisition of raw Apple artifacts.

[R1] Apple XNU `bsd/sys/wait.h`, consulted24September2026, wait-status semantics:
https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/bsd/sys/wait.h .
The source distinguishes stopped and continued observations; it does not certify
our new cross-platform verifier or replace its tests.

[R2] NIST CSRC digital-signature glossary, consulted24September2026:
https://csrc.nist.gov/glossary/term/digital_signature . Used only to distinguish
identity-bearing signatures from unsigned hash receipts.

All current test/byte inventories resolve through `evidence/turn15/`. The detached
receipt supplies exact final hashes, file sizes and completed delivery operations.
