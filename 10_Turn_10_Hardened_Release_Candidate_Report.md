# Turn 10 — Hardened Release Candidate Report
## ELITEIPC 1.0.1 / LE128-V1 / SPSC and NCQ-SC64

**Project:** Leonid Majbits / Gemini Operator Lab, paired with Leon  
**Date:** 2026-09-23, America/Toronto; execution logs also contain UTC timestamps  
**Stage:** Post-release remediation and regression verification  
**Disposition:** A09-01 through A09-04 remediated in this source candidate; additional A09-07/A09-08 hardening included. Native Apple execution of this new GC implementation remains a deployment gate.  
**Canonical-SHA256:** `5589080086f5e302eb47b157c1728bcf515a2073a50315c03438693482deeba9`
**Hash convention:** The canonical field hashes the complete UTF-8/LF document with only its own 64 hexadecimal characters replaced by ASCII zeros. The adjacent `.md.sha256` hashes the literal final file.  
**Baseline archive:** `elite-ringbuffer-1.0.0.zip`, 3,959,129 bytes; SHA-256 `9d8a9843ecbde3cd5dcc780a744f398c2638a92a0a656b3aaa86ddc72cec2907`.  
**Completed audit:** 84,479-byte `09_Adversarial_Stress_Test_and_Claims_Audit.md`; SHA-256 `9862ec19ccc05a2f8ae850a6fe0e15d4827e4337803594f4668fb39f49ac3200`. The initially commissioned shorter audit is not silently substituted for this completed version.  
**Destination:** Deployment Research / Elite_Systems_LockFree_RingBuffer  
**Evidence conventions:** [A09] is the verified completed audit. [U10] is the commissioning message's lab-reported Apple experiment. `evidence/turn10/` contains new local observations. [R1]–[R6] identify primary CPython/tool documentation. Proof arguments below concern this implementation and its stated runtime assumptions, not an exhaustive machine-checked theorem.

---

## 1. Decision and remediation matrix

The candidate preserves the native data-plane algorithms and repairs the Python ownership boundary rather than reducing it to a copying-only interface. Zero-copy memoryviews remain supported. The shared format remains LE128-V1, ABI `0x00010000`; native library and Python/exporter revision become **1.0.1**.

| Finding | Implemented response | New evidence / status |
|---|---|---|
| A09-01 — pending export race | Count an acquisition before executing Python validation; keep it counted through callback-result destruction and buffer construction; roll back every failure | Original four SPSC/NCQ × read/write histories safely blocked; repeated schedules and callback/FillInfo tests pass |
| A09-02 — unreachable cycles | GC-aware C exporter; independently rooted native lifetime state; ordered deferred unpin, token return and detach | Twelve primary cycle shapes and seven additional GC test methods pass without manual cycle breaking or new cleanup failures |
| A09-03 — incomplete/stale release receipts | Preserve LICENSE and `.gitignore` bytes, include both in the source manifest, include the completed audit, regenerate complete inventories; read-only verifier rejects omissions | Eight integrity tests, including pristine CLI verification with no file mutation; final archive validation is recorded in the detached delivery receipt |
| A09-04 — GCC O1 warning | Convert the byte operand to unsigned before its shift and bitwise operations | Original source still fails the strict lane; corrected GCC O1 ASan/UBSan native library and exporter compile; native instrumented tests pass |
| A09-07 — unraisable-hook resurrection UAF | Move cleanup to resurrection-aware finalization; never pass a dying exporter as the unraisable context; preserve failed native obligations independently | Original retained-hook probe no longer receives a freed exporter; candidate ASan/UBSan suite passes |
| A09-08 — acquisition/adoption exception gap | Allocate/install local recovery state before the native acquisition; poison and retain the endpoint when its result becomes uncertain | Deterministic original post-acquisition exception reports busy/uncertain state instead of a falsely idle public handle |

A09-05's cache-counter provenance gap and A09-06's performance-claim boundaries are **not converted into resolved hardware qualifications** by these fixes. Neither benchmark code nor its timing protocol is changed in this sprint. The version change is not an assertion of zero defects, hard real time, indefinite crash recovery, or a new bandwidth result.

The user's Apple experiments are valuable independent evidence that the original export race occurs on Darwin/CPython 3.14 and that counting pending exports blocks those four schedules. They validate the reported minimal candidate, not the larger GC implementation delivered here. This report keeps [U10] separate from new local execution.

## 2. Exact source custody and change boundary

The standalone ZIP is the subject, not an earlier Turn 8 archive with a different Python implementation, file population or hash. All **486 original regular files** in the extracted baseline were rehashed against their ZIP members after negative-control testing; none changed. Build products were written under separate build directories.

The completed audit ZIP was retrieved from the project's existing Drive folder. Its **158 manifest entries** matched. The complete audit is retained verbatim at `docs/ADVERSARIAL_AUDIT.md`; it includes the two late findings A09-07 and A09-08. It is not rewritten to pretend that the original release passed.

The following original files remain byte-identical:

- `src/elite_spsc.c`, `src/elite_mpmc_ncq.c`, `src/elite_core.c`, `src/elite_format.c`, `src/elite_wait.c`, and `src/elite_internal.h`;
- `include/elite_ringbuffer.h` and `include/elite_api.h`;
- `LICENSE` and `.gitignore`.

Thus the shared layout, its 103 compiled assertions, queue progress mechanism, release/acquire SPSC ownership edges, strong-SC NCQ metadata, and no-live-wrap rule are not altered. `src/elite_shm.c` changes only the requested unsigned promotion in name generation. `src/elite_version.c` and `include/elite_version.h` report library revision 1.0.1; they do not change the wire ABI.

The substantive binding edits are in `bindings/python/elite_buffer.c` and `bindings/python/elite_ringbuffer/__init__.py`. `_native.py` now requires the matching 1.0.1 native library. The C exporter exposes a matching revision marker, and the Python package rejects an old exporter instead of silently loading the vulnerable module. Rebuild and deploy all three components together.

`docs/HARDENING_1_0_1.patch` records edits to existing source/build/test files. `evidence/turn10/PROVENANCE_AND_DELTA.json` identifies the baseline, completed audit, and existing-file hash changes. Newly added tests, reproducers, documentation and tools are covered by the final manifests. Historical manifests remain under `docs/history/` and are explicitly historical, not current acceptance receipts.

The standalone MIT license chosen by Leon is retained without textual alteration. NOTICE and current README language are reconciled with that existing license; this sprint does not invent a replacement license or upstream algorithm attribution.

## 3. A09-01 — a real export acquisition transaction

### 3.1 The state which was missing

Let:

- `a` be the number of in-progress `bf_getbuffer` acquisitions which have passed admission but have not yet returned success or failure;
- `r` be the number of issued, unreleased `Py_buffer` roots tracked by the exporter;
- `e = a + r` be the exporter's `exports` count.

A slice or cast may share CPython's existing managed buffer; it does not necessarily increment `r` itself. The relevant property is that its underlying buffer ownership remains live until the last compliant dependent user releases it. [R3]

The previous count represented issued buffers only. A Python `_assert_live` callback could suspend after validating a lease while a second thread observed `exports == 0`, closed the exporter and transferred ownership. On resumption, the first caller could receive an invalid buffer. The GIL did not make that whole sequence atomic because the callback could run Python and allow another thread to execute. [A09, A09-01; R5]

### 3.2 New acquisition sequence

`buffer_get` now performs these steps:

1. Clear `view->obj` for a failure return; validate process identity and the open state.
2. Reject a count at `PY_SSIZE_T_MAX` before incrementing.
3. Increment `exports` **before** calling `_assert_live`.
4. Execute the callback, keeping the count through destruction of its returned object. A returned object's destructor may itself execute Python.
5. Call `PyBuffer_FillInfo` with the real exporter as owner and the correct readonly flag.
6. Return success without another count change, or unwind the pending acquisition on either failure.

The operations which start an acquisition and reserve its count occur while the admitted CPython GIL is held and before a Python callback. Free-threaded interpreters are explicitly rejected by the extension; no unproved lock-free access to these C fields is introduced.

A transfer or explicit exporter close checks this same count. Since `e > 0` throughout the suspended callback, the competing close must fail with `BufferError`; it cannot call the native view-end operation or transfer the lease.

### 3.3 Failure paths and requested finalization

A callback exception, including MemoryError or KeyboardInterrupt, decrements exactly the pending count. A failed writable request against a readonly exporter similarly unwinds a failed `PyBuffer_FillInfo` path. A later valid acquisition is permitted if the exporter otherwise remains open.

There is an additional state: finalization may be requested while an acquisition is in progress. That request prevents **new** acquisitions but does not revoke the one already counted. If the acquisition succeeds, the existing buffer retains ownership until its release. If it fails, `rollback_export` observes that the now-zero count permits requested closure and completes it. The original acquisition exception is preserved while any cleanup failure is reported separately.

This closes an error-path leak that a bare increment/decrement patch could leave if finalization had already run while the count was nonzero. Three direct tests exercise callback failure, FillInfo failure and successful acquisition during an explicit finalization request.

### 3.4 Closing is also transactional

`close_buffer` rejects reentrant close and any nonzero export count. It marks the exporter closed/closing **before** invoking cleanup, blocking reentrant acquisition through the callback. If cleanup fails, it restores the retryable close state while the native cleanup obligation remains retained. On success it clears the address and owner references only after the closing state is complete.

Reference decrements can themselves trigger Python finalizers. Therefore their placement is part of the state machine, not harmless postscript work. The tests verify callback-result destruction, reentrant close/export, failure followed by a successful retry, and exactly one successful native-pin drop in the fake-owner transaction fixtures.

The private raw-address factory is not a sandbox. These changes protect the supported binding's ownership graph; arbitrary forged pointers or malicious native code remain outside it.

## 4. A09-02 — cyclic GC without premature unpinning

### 4.1 Why adding the GC flag alone would be insufficient

The exporter contains Python object references, so it must expose them to cyclic collection. But collection can finalize and clear different objects in an isolate in an unspecified order. Calling an ordinary Python pin's cleanup method after that pin or its endpoint dictionary has been cleared would merely exchange a leak for a lifecycle error. Conversely, clearing ownership solely because GC found a cycle could invalidate a still-exported alias.

CPython's container-GC and object-lifecycle contracts require proper traversal/allocation/deallocation and tolerance of partially finalized or cleared referents. A finalizer may also resurrect an object. [R1, R2] The solution must address the whole graph, not only the C type's flag.

### 4.2 Two graphs with deliberately different responsibilities

The supported public retention graph is:

`view / compliant downstream consumer -> C exporter -> _Pin -> public endpoint -> public manager`

This keeps familiar public objects alive while an ordinary view remains reachable. For example, retaining a slice keeps its mapping and slot available; deleting the local producer variable does not revoke it.

The native cleanup graph is separate:

`C exporter -> _LeaseState -> _EndpointLifetime -> _ObjectLifetime -> _Authority`

These cleanup objects retain native pointers, locks, ownership results and necessary bookkeeping, but **have no strong path back to a public lease, public endpoint, public manager, exporter or view**. Their endpoint/object collections are weak where backedges would otherwise occur.

Weak finalizers for `_Pin`, public endpoints and public managers hold callbacks bound to these independent cleanup objects. They are not callbacks bound to the watched public object. This avoids retaining the object which the finalizer is waiting to collect, a constraint explicitly discussed in Python's weakref documentation. [R4]

The finalizer registry therefore roots the cleanup data while permitting the user-facing cycle to become unreachable. The exporter also retains `_LeaseState` directly. Arbitrary clearing of a public object's dictionary cannot destroy the only copy of the native cleanup data needed by the last buffer release.

This is the principal architectural change. `_Pin` remains an ordinary GC-visible Python container; no second C queue or shared-memory ownership record is introduced.

### 4.3 C type hooks and allocation

The C `EliteBuffer` implementation, exposed internally as `_elite_buffer.LeaseBuffer`, now has:

- `Py_TPFLAGS_HAVE_GC`;
- a side-effect-free `tp_traverse` visiting `owner` and `cleanup_owner`;
- `tp_finalize` which requests closure and attempts cleanup only when no export remains;
- `tp_clear` which severs Python edges only when no export remains, without directly invoking a second native cleanup transaction;
- `tp_dealloc` using `PyObject_CallFinalizerFromDealloc`, respecting its resurrection result, untracking the object, clearing references, then using the type's matching free function.

Allocation goes through the type's `tp_alloc`; the inherited generic allocator supplies the GC-compatible allocation and tracking behavior. Traversed fields are initially valid null references and are assigned before the factory returns. There is no duplicate manual tracking operation and no non-GC allocation paired with a GC free. [R1]

`tp_clear` is intentionally not a second route for arbitrary Python/native cleanup. Clearing a reference can indirectly run another object's finalizer; those callbacks use the independent, idempotent cleanup state rather than a half-cleared public wrapper. The direct close operation lives in finalization/last-buffer-release and normal explicit close.

### 4.4 Deferred cleanup order

An unreachable `_Pin` asks its `_LeaseState` to clean up. If the exporter still owns a native view pin, that request is recorded and deferred. An endpoint finalizer similarly requests detach but defers while a lease or uncertain acquisition remains; a manager finalizer defers while endpoint lifetimes remain active.

When the last admitted buffer root releases:

1. `bf_releasebuffer` decreases `exports`.
2. If finalization requested closure and the count is now zero, close the exporter.
3. End the one native view pin through the still-live `_LeaseState` and endpoint lock.
4. If lease cleanup was requested, abort an unpublished write or release a read. Do not auto-commit.
5. After a known returned/transferred result, mark the local lease inactive and complete any deferred endpoint detach.
6. Acknowledge local cleanup to the object lifetime, then attempt requested object destruction only when native holder rules permit it.

Explicitly closing a live outer lease follows the same native-pin and outcome rules. A finalizer cannot turn a guessed timeout into ownership, reset a shared cursor, or reclaim an unknown transfer. The fixed native authority still decides whether whole-object reclamation is permitted.

### 4.5 Safety argument

For supported execution, the following implications hold:

`pending or issued buffer -> exporter retained -> native view pin retained -> lease/mapping cleanup deferred`.

An external reference to a compliant view prevents its exporter chain from being an unreachable isolate. During finalizer resurrection, a view already holding a valid buffer remains pinned; requested closure only prevents new exports. A collector-induced release of the last view ends the pointer's permitted use before native unpin and token return.

Conversely, an unreachable ordinary cycle can be collected because the C exporter exposes every strong container edge and the finalizer registry does not point back into that cycle. The independent cleanup graph remains valid while public edges are severed. Once exports and leases end, pending detach/destruction requests can run rather than remaining dependent on a public object which was already cleared.

This is a conditional implementation argument supplemented by executed schedules. It is not a bound on when Python invokes GC, a promise to override `gc.disable()`, or permission to reclaim an indefinitely retained live buffer. A caller can still deliberately retain all queue capacity; that is resource retention, not a hidden publication hole.

### 4.6 What the GC tests actually check

`tests/test_gc_cycle.py` contains **19 test methods**. Twelve are the full SPSC/NCQ × read/write × endpoint/lease/manager-cycle matrix. They delete the public roots, invoke collection, require the watched objects' weak references to clear and the exact test backing name to disappear, and require no new `cleanup_failures` entry. No test manually breaks the unreachable cycle to obtain its passing result.

Seven additional methods cover externally retained negative-stride slices, two independent buffer roots, a downstream ctypes buffer, resurrection of an existing read view, cycles across two objects, zero-length views, and 64 repeated cycle variations. The externally retained cases verify bytes remain usable while the object is still present and that cleanup occurs only after final release.

These namespace checks are paired with actual native cleanup paths and sanitizer/native regression evidence. Unlink by itself is not treated as general proof of unmapping or fencing; no test uses a namespace sweep to hide an ownership failure. The historical GC witness is run against the old release only and is retained as a negative control.

## 5. Additional lifetime blockers from the completed audit

### 5.1 A09-07 — no dying exporter in an error hook

The old destructor sent `self` to `PyErr_WriteUnraisable` and unconditionally freed it afterward. A retaining custom hook could keep a Python reference to a freed exporter. The completed audit's AddressSanitizer witness identified that distinct UAF. [A09, §17.2]

The candidate reports finalizer errors with `Py_None` as the immutable context instead. It never exposes an about-to-be-freed exporter as the error-context object. Before reporting, it attempts to retain the native cleanup obligation independently. `tp_dealloc` invokes the CPython finalizer helper and stops destruction if the helper reports resurrection. Existing exceptions are saved/restored around finalization and cleanup reporting. [R2]

The exact original constrained hook/trace probe runs in an owned subprocess in the new suite. The trace confirms it reached the injected cleanup failure, but the hook receives no freed LeaseBuffer. Deliberately unknown native cleanup remains retained; the test is a safety result, not a claim that injected cleanup failure becomes successful reclamation.

### 5.2 A09-08 — native acquisition before Python adoption

The candidate preallocates lease/span storage, independent cleanup state, the pin and public lease wrapper before entering the native reserve/borrow call. It installs the local acquisition obligation before ctypes can release the GIL or acquire a token.

An exception during that acquisition/adoption interval marks its outcome uncertain, poisons the endpoint and retains the cleanup state. `busy` then reflects the obligation instead of falsely reporting idle. A second reserve is blocked; close reports the uncertain state instead of unmapping or inventing an abort result.

The original deterministic post-native-reserve trace test now observes this explicit state. This is **fail-closed exception safety**, not transactional exactly-once acquisition or an automatic same-generation token repair. The controlled child is reaped before its exact backing name is removed by its test controller. Future application use requires the same owned-process fencing/recovery discipline.

The normal native return path is unchanged. Existing tests for a lost publication result, process death and retained aliases remain enabled. Arbitrary asynchronous interruption at every possible Python/native/control-plane instruction is not claimed to be universally transparent; cooperative cancellation remains the production integration policy.

## 6. A09-04 — strict GCC warning repair

The byte is explicitly promoted to unsigned before shifting, with unsigned shift arithmetic and mask:

`(((unsigned)id[pos/8] >> (7u - (pos % 8u))) & 1u)`.

No queue behavior or shared representation changes. The old release, compiled with GCC 14.2.0 at O1 and `-fsanitize=address,undefined` under the full warning-as-error set, still fails at `src/elite_shm.c:66` with `-Werror=sign-conversion`. This is a deliberately preserved negative result.

The candidate's same strict O1 lane compiles the native library, exporter, core test, adversarial test and parser fuzzer. Native execution passes core, adversarial and one-million-input parser tests with ASan/UBSan. The separate Python instrumentation lane uses Clang's matching runtime; GCC objects and Clang sanitizer runtimes are not mixed.

The exporter has a GIL-build compile guard; native core C11 flags remain `-Wall -Wextra -Werror -pedantic -std=c11` plus the existing conversion/prototype checks. The Makefile retains Darwin `_DARWIN_C_SOURCE` and the earlier creation-time permission behavior.

## 7. A09-03 — immutable verification, complete inventory

The original complete manifest failed on `.gitignore` and omitted the standalone LICENSE. A matching outer archive SHA alone did not make that inner manifest complete. The candidate preserves both files' supplied bytes and hashes their actual content.

The new verifier requires equality between the declared full inventory and the actual source inventory, excluding only the manifest's self-reference. It then verifies every declared file hash, checks mandatory files, checks exact source-policy coverage, requires agreement between source and complete manifests, and validates this report's canonical field. Manifest entries with unsafe/noncanonical paths or duplicates are rejected; symlinks are rejected.

The source policy includes C/C++/Python/shell sources, Makefile, LICENSE, `.gitignore`, NOTICE and `docs/ADVERSARIAL_AUDIT.md`. The complete manifest covers every packaged regular file except itself, including the source manifest, historical evidence, this report and its literal sidecar.

`tools/regenerate_manifests.py` is an explicit authoring operation. Verification never invokes it and never rewrites a source, digest or receipt to make a mismatch pass. It also disables local bytecode writing before importing its helper: a pristine `--strict` verification must not create `tools/__pycache__` and invalidate the inventory it is checking.

Default development verification ignores only the documented generated build/cache/VCS overlays. `--strict` is the pristine-archive gate and rejects unlisted build files too. It does not claim adversarial authentication against an attacker who can replace both the verifier and its trusted checksums. Store the detached archive hash through the trusted release channel.

Eight integrity tests include edits to LICENSE, deletion of the audit, an unlisted hidden file, source-manifest omission despite updating its outer hash, a symlink, a strict-mode build overlay, a pristine fixture, and a subprocess CLI check that the complete fixture's file inventory and bytes remain unchanged.

## 8. Executed verification and negative controls

### 8.1 Execution tuple

New local execution used **Linux x86-64, CPython 3.13.5, GCC 14.2.0 and Clang 17.0.0**. The host reported **Intel Xeon Platinum 8573C**, five allowed logical CPU IDs and a four-CPU-time cgroup quota. It is not the earlier benchmark host and not the lab's Apple machine. `evidence/turn10/environment.json` records these observations and the actual compiler/runtime paths.

A CPython 3.14 installation attempt was unavailable because the runtime download could not be completed. No CPython 3.14 or Darwin execution is claimed for this new candidate. The source uses the reviewed public CPython lifecycle/buffer APIs; documentation compatibility is not a substitute for that target's tests.

### 8.2 Candidate results

| Lane | Observed result | Evidence |
|---|---|---|
| GCC O3 native | Strict libraries, core68, mutation512, IPC100k SPSC/NCQ4P4C, adversarial8, limits4, chaos3, parser1M, benchmark-tools8 and C++ example pass | `native-gcc.log` |
| Clang O3 native | Same requested native regression targets pass | `native-clang.log` and JSON |
| Final GCC Python | **76/76 methods passed**, 40.805 s unittest time | `python-release-gcc.log` |
| Final Clang Python | **76/76 methods passed**, 46.106 s unittest time | `python-release-clang.log` |
| Clang ASan/UBSan Python | **76/76 methods passed**, 94.604 s unittest time | `python-release-clang-asan.log` |
| GCC O1 ASan/UBSan | Strict build passes; native core/adversarial/parser1M pass | `gcc-o1-asan-ubsan-build.log`, `final-gcc-o1-exporter.log`, `native-gcc-asan.log` |
| Native GCC TSan | Unsuppressed one-process adversarial lane passes | `native-tsan.log` |
| Staged installation | Native/Python install into a local prefix; both-profile Python example passes from installed package | `install-stage.log` |
| Pristine final artifact | Recorded separately after actual extraction, strict verification and rebuild | Detached deposit/integrity receipt |

The **76 methods** comprise the unchanged 36-test baseline (only version assertions updated), 11 export-transaction methods, 19 GC methods, two isolated failure probes, and eight integrity methods. A method may contain several mode/role subcases. The original four export-race combinations are repeated three times inside their regression method: twelve owned child executions per suite, not twelve distinct bug classes. Six additional pending-export abort/close schedules are also exercised.

The normal parent process and GC lifetime cases require no new `cleanup_failures`. Deliberately failed/uncertain cleanup probes execute in separate owned children and retain their native obligations until process fencing. They are not included in a misleading claim that all injected failures transparently clean up.

### 8.3 Sanitizer boundaries

The Python ASan/UBSan lane instruments both the native library and C exporter, uses `PYTHONMALLOC=malloc`, the matching Clang runtime, and **`detect_leaks=0`**. It is a memory-access/undefined-behavior lane with explicit lifetime assertions, not a leak-sanitizer certificate for the whole interpreter.

The native GCC ASan lane explicitly used `detect_leaks=1`. POSIX namespace objects and protocol token ownership still require their separate oracles; heap leak instrumentation does not account for all OS resources.

TSan covers its declared single-process, single-mapping native adapter. No core suppression or invented acquire/release annotations are used. A clean TSan result is not advertised as certification of all cross-process or Python-buffer schedules. ASan likewise cannot identify every logical use-after-lease while backing memory remains allocated. [R6]

### 8.4 Before/after controls

Against the exact unchanged 1.0.0 input, the same four exporter schedules all returned **SIGSEGV / -11**, with the invalid-export state captured. The historical GC witness reported `exporter_gc_tracked=false`, a collected outer lease, and a retained busy endpoint until its explicit test-only cycle break. The old strict GCC O1 build failed on the named conversion warning. The old release verifier rejected `.gitignore`.

Against 1.0.1, the race regression requires `transfer_blocked=true` and a normal child exit; GC positive cases do not manually break the cycle; strict builds pass. These controls establish that the tests can distinguish the identified defects from this candidate, rather than merely printing green output for either version.

### 8.5 Incomplete execution records are preserved

Early combined suite commands hit the outer tool execution window. `races-suite-initial.log`, `python-full-gcc.log` and `python-full-gcc-retry.log` are retained as incomplete observations; they are not counted as passes. The later owned runner allowed up to 300 seconds, recorded an exact command/exit status, and completed the corresponding suites. No product bug or harmless scheduler explanation is inferred solely from an external interruption.

The separate CPython 3.14 download failure is also recorded. Neither an interrupted local command nor an unavailable runtime is silently converted into a test success. `TEST_RESULTS.json` summarizes the completed lanes and these distinctions.

## 9. Production integration and unchanged limits

Use explicit lease and buffer scopes for normal work. A context exit aborts an unpublished write or releases a read, but cannot force-close an externally retained buffer. Avoid holding views in long-lived caches unless deliberately reserving that capacity. Reachable zero-length views also retain their lease: length zero is not permission to recycle an outstanding buffer.

The Python wrapper serializes operations on one endpoint using its process-local RLock; different endpoints can run independently. This is not a claim that Python object management, locks or garbage collection are lock-free. Native payload access through several user aliases still requires application data-race discipline. GIL-enabled main-interpreter CPython is required; free-threaded Python, subinterpreters and arbitrary raw-pointer escape are not qualified by this release.

Use spawn/exec and managed one-shot grants for cross-process work. The hardening adds no automatic authority takeover, no timeout-based payload theft, and no persistent power-loss recovery. SIGKILL cannot execute a finalizer. A failed owner can still strand a token in an old generation; the authority must fence/retain it and, when necessary, use a distinct successor. No epoch or memory fence revokes a raw pointer.

The supplemental acquisition fix deliberately chooses truthful uncertainty over unsafe recovery. Applications must not clear `_FAILED_CLEANUP`, reset private `poisoned` flags, or invoke raw native functions to bypass the retained obligation. A supervised process boundary is the recovery route when the result of a transfer is unknown.

The wrapper now allocates independent cleanup-state/finalizer objects. This has a per-lease cost which must be measured on the target; no previous Python bandwidth result is automatically assigned to the revised wrapper. Native algorithm hashes are unchanged, but that alone is not new hardware qualification. The 250-ns RTT, 22.8-GB/s native-assisted Python workload and cache-counter ratios keep their original workload/provenance labels.

## 10. Updated engineering scorecard

These scores are audit judgment, not probabilities, formal assurance levels, or an average which can waive a blocking witness.

| Category | Completed A09 | This candidate | Rationale |
|---|---:|---:|---|
| Code quality | 7/10 | **8/10** | Explicit state separation, matching runtime versions, strict two-compiler builds, original reproducers and reproducible failure paths |
| Memory safety | 4/10 | **8/10** | Known pending-export/UAF/GC witnesses addressed; normal and hostile callback tests and ASan/UBSan pass within stated scope; no all-runtime theorem |
| Benchmark reproducibility | 6/10 | **6/10** | Release integrity is repaired, but missing independent Apple/raw-counter provenance and full original metrology do not disappear |
| Architecture elegance | 8/10 | **8/10** | Native minimalism preserved; additional Python state is purposeful and isolates cleanup from user graph destruction |
| Production hardiness | 4/10 | **7/10** | Fail-closed exception state and controlled cleanup improve deployability; exact Apple candidate admission, operational supervision, and workload qualification remain |

This is a credible **1.0.1 hardened release candidate**, not an unconditional HFT or latency-certified distribution. The correct next deployment step is to execute this exact candidate's new lifetime cases on the lab's Darwin/CPython 3.14 target, not to repeat only the old 36 tests or infer safety from a successful performance run.

## 11. Reproduction and release admission

From a freshly extracted root:

```sh
python3 tools/verify_release.py --strict
make CC=clang CXX=clang++ BUILD=build/hardened libraries python
make CC=clang CXX=clang++ BUILD=build/hardened check check-hooks limits chaos fuzz check-bench-tools check-cpp
make CC=clang CXX=clang++ BUILD=build/hardened check-python-hardening
```

The Python target uses `PYTHON ?= python3`. Select the lab's intended CPython explicitly with `PYTHON=/absolute/path/to/python3.14` when several interpreters are installed. Build the exporter with that interpreter and use the same one at runtime. The Makefile sets the appropriate Darwin or Linux dynamic-library name and Python path for tests.

The original schedule runner can be invoked separately:

```sh
python3 repros/run_export_races.py --root . --build build/hardened --out new-race-results --repeats 3
```

Its output directory must not already exist. It kills/reaps only its owned test children and removes only their exact reported test name after termination. A nonzero result may be a vulnerable or incomplete schedule; inspect the recorded outcome rather than labeling every nonzero exit a UAF.

The strict GCC sanitizer build arguments used here are:

```sh
make CC=gcc BUILD=build/gcc-asan \
  OPT='-O1 -g -fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-lto -fsanitize=address,undefined -fno-sanitize-recover=all' \
  LDFLAGS='-fsanitize=address,undefined' PY_SANITIZE=address,undefined libraries python
```

The report's local sanitizer command/runtime paths are evidence for Linux, not copy-paste Darwin preload paths. Use the target compiler's supported runtime arrangement on Darwin and record it. Keep performance binaries separate from instrumented builds. Do not copy a version-specific `.so`/`.dylib` or exporter from this Linux environment to the Mac.

After target testing, retain compiler/SDK/interpreter identities, logs, binary/source hashes, all new GC/race/failure outcomes, and exact archive hashes. Run the verifier before generating local build outputs, or use ordinary development mode afterward. Adding output logs inside a pristine source root intentionally changes its strict inventory.

## 12. Artifact integrity and delivery semantics

The source ZIP contains no compiled libraries, executables, CPython distribution, sanitizer runtime or SDK. It contains the complete source, existing tests/evidence, new tests/reproducers, report, patch, exact manifests and scoped local logs.

The complete manifest excludes only itself from the packaged inventory. The source manifest includes all defined source inputs and the named license/audit requirements. A detached ZIP checksum authenticates the whole archive. This report's embedded digest uses the stated self-field normalization; its literal checksum is a separate sidecar, not a purported fixed point.

Final fresh-extraction, rebuild and upload/download verification are recorded in a separate receipt generated after those actions. This immutable report does not predict an upload, a cloud-sync event or a future native Apple pass. Uploads add new Turn 10 files and leave the standalone 1.0.0 archive and audit records unchanged.

## 13. Primary sources and evidence ledger

**[A09] Verified completed project audit.** `docs/ADVERSARIAL_AUDIT.md`, identity in the header. Original four findings plus §17's A09-07/A09-08. Its source/proof/measurement boundaries are retained. All original 158 evidence-manifest entries were verified before selecting reproducers.

**[U10] User's Turn 10 commission.** Reports native Apple CPython 3.14 failure reproduction, successful minimal pre-increment candidate, the proposed unsigned fix and cyclic-reference reproduction. These are LAB_REPORTED facts; no raw logs for this new GC design were supplied as part of that statement.

**[R1] CPython 3.14, Supporting Cyclic Garbage Collection.**
https://docs.python.org/3.14/c-api/gcsupport.html
Container flags, traverse/clear responsibilities, generic type allocation and matching GC deallocation. This implementation's ownership design and tests are project work, not certified by the manual.

**[R2] CPython 3.14, Object Life Cycle.**
https://docs.python.org/3.14/c-api/lifecycle.html
Finalization versus clearing, resurrection and `PyObject_CallFinalizerFromDealloc`, and tolerance of arbitrary finalization/clearing order. The actual executing interpreter here is 3.13.5, recorded separately.

**[R3] CPython 3.14, Buffer Protocol.**
https://docs.python.org/3.14/c-api/buffer.html
Exporter references, flags, `PyBuffer_FillInfo` and release obligations. Underlying slot ownership is supplied by the project, not by a raw buffer address.

**[R4] Python weakref documentation.**
https://docs.python.org/3/library/weakref.html
Finalizer retention and the requirement not to retain the watched object through callback arguments or bound methods. The independent cleanup graph enforces that structural requirement.

**[R5] Python ctypes documentation.**
https://docs.python.org/3/library/ctypes.html
Foreign-call GIL behavior and native access responsibilities. The package uses per-endpoint serialization and explicit exporter counts, not a presumed all-encompassing GIL transaction.

**[R6] LLVM sanitizer documentation.**
https://clang.llvm.org/docs/AddressSanitizer.html
https://clang.llvm.org/docs/ThreadSanitizer.html
https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html
Instrumentation scopes are stated in §8; concrete passing and negative results come from the included logs, not documentation examples.

**New execution index:** `evidence/turn10/TEST_RESULTS.json`. **Source provenance:** `PROVENANCE_AND_DELTA.json`. **Existing-file source patch:** `docs/HARDENING_1_0_1.patch`. **Current source/tree receipts:** `SOURCE_MANIFEST.sha256` and `MANIFEST.sha256`. **Original negative reproducers:** `repros/`. The final delivery receipt binds the packaged bytes to subsequent readback and fresh extraction.

---

**Final decision:** preserve the native design, publish the exact remediation evidence, and admit this 1.0.1 candidate to the lab's target qualification. The known Python release blockers are not dismissed because the benchmarks were fast; they are addressed through explicit lifetime state, reproducible counterexamples and passing post-fix tests.
