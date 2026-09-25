# Turn 14 — Multi-Process Chaos and Crash Recovery
## ELITEIPC 1.0.1 / LE128-V1 / native V2–V3–V8 verification

**Project:** Leonid Majbits / Gemini Operator Lab, paired with Leon  
**Evidence date:** 24 September 2026; raw invocation and monotonic observations retained under `evidence/turn14/`  
**Disposition:** Executed finite multi-process fault histories and managed successor recovery. No automatic in-place orphan recovery, universal progress deadline, or v1.1.0 qualification is claimed.  
**Canonical-SHA256:** `ad628b1338964620b13812dbca907fe220b3741e4478b1735a007d3b15cd762f`
**Canonical convention:** Replace only the preceding field's 64 hexadecimal characters by 64 ASCII zeros before hashing the entire UTF-8/LF document. The detached sidecar hashes the literal finalized bytes.  
**Authoritative parent:** `Elite_Systems_LockFree_RingBuffer_Turn_13_Formal_Bundle.zip`, 41,003,261 bytes; SHA-256 `9605ceaa1372dae26e465e4752cf62e0298b5707464ff1ea65a7028d00ead543`.  
**Destination:** Deployment Research / Elite_Systems_LockFree_RingBuffer  
**Evidence vocabulary:** EXECUTED identifies retained local runs; DERIVED identifies the argument supplied here; LAB_REPORTED identifies the user's Apple account; NOT_RUN identifies unexecuted coverage. Source contracts and finite tests are not promoted to physical guarantees.

---

## 1. Outcome

Both final compiler campaigns completed their frozen population: **186 cases**, consisting of **180 real multi-process histories and six separate single-process resource negatives**. They checked **12,052,618 ordinary message reads**, excluding separately accounted seed/sentinel messages. Every admitted case passed independent raw-event replay. Across the primary multi-process population, the logs account for **1,620 child-process instances and 8,626 externally sent signals**. Sent signals are not exact handler-invocation counts.

The evidence establishes the selected outcomes: unpublished partial payloads were not delivered; healthy NCQ peers completed work around stopped or killed holders; already installed messages and free returns did not wait for their stopped publisher; unknown tokens were not reinserted; false-suspicion recovery preserved the successor's held payload; terminal snapshots reconciled every block; and all owned children and names were closed/reaped/removed at successful completion.

It also exposes the boundary rather than hiding it: **some old-generation tokens remain unavailable, and asynchronous cutover can leave committed messages pending in QR**. Those resources are accounted and retained with the old object until safe whole-object disposal. They are not called recovered capacity or delivered application messages. A live SPSC lane cannot continue the missing role after its sole endpoint dies; the new service is a distinct generation.

The production surface is unchanged. All **21 preexisting files under `src/`, `include/`, and `bindings/`**, including one binding README, match the parent byte-for-byte. The API, memory orders, static layouts, object quotas, Python GC implementation, and version 1.0.1 remain unchanged. No shared heartbeat, owner registry, new commit cursor, or timeout-revocation operation was added. [P13, P4]

### 1.1 Scope corrections carried into execution

| Requested phrase | Operational meaning in this delivery |
|---|---|
| Violent process failure | Signals target only explicitly owned children; normal user payloads still use shared memory |
| Lease timeout | External monotonic absence-of-progress suspicion; never permission to steal a slot |
| Zero unaccounted token leaks | Exact final partition after terminal quiescence; online unknown transfer responsibility remains explicit |
| Cursors never become unrecoverable | No invalid final cursor/entry/phase state in the selected histories; orphaned capacity and retired generations are legitimate outcomes |
| All peers continue throughput | Healthy NCQ peers with spare storage complete quotas; SPSC missing roles require a new generation |
| Async crash recovery | A live application authority coordinates quarantine, terminal evidence, and a fresh process cohort; recovery itself is not a lock-free data-plane operation |
| Committed data never corrupted | Every delivered payload is checked by its legitimate reader; pending/uncertain committed messages are not silently reported delivered |
| head/tail/commit_seq | The actual ABI is SPSC P/C or NCQ QF/QR heads and tails; no `commit_seq` member exists or is invented |

The user's M3 Max/commit `cf4ec7c` receipt is LAB_REPORTED. This turn authenticates the named parent archive, not an independently fetched Git object or new native Apple execution. Turn 13's state counts remain bounded model evidence, not a general C11 checker or proof of arbitrary OS lifecycle behavior. [P13 §§1,12]

## 2. Source custody and implementation inventory

The parent SHA-256 and ZIP integrity were verified before extraction. Its strict release verifier accepted 6,041 full-manifest entries and 107 source entries. The new tree is a derivative, not an overwrite of the release used for that evidence. `PRODUCTION_CONTINUITY.json` records the before/after hash of every inherited file in the three production directories.

| Added file | Responsibility |
|---|---|
| `tests/chaos_protocol.h` | Bounded native same-executable control/evidence records; not wire-ABI definitions |
| `tests/test_chaos_multiprocess.c` | Owned-process launcher/controller/worker, actual public queue calls, faults, heartbeat policy, quarantine, successor and terminal inventory |
| `tests/test_chaos_resources.c` | Four-object/two-quarantine and live-view destruction negative checks |
| `tools/run_chaos.py` | Frozen campaign, exact commands, source/executable identity, bounded owned subprocesses, raw logs and completion marker |
| `tools/verify_chaos_results.py` | Independent Python receipt/membership/frontier/lifecycle reconstruction |
| `tools/check_chaos.py` | Fresh temporary smoke cohort and replay |
| `tests/test_chaos_results.py` | 50 positive/negative oracle tests, including optimized Python |
| `tests/fixtures/chaos/*.jsonl` | Seven actual small native fixtures, including asynchronous retirement |
| `docs/CHAOS_RECOVERY.md` | Commands, semantics, result schema interpretation, failure handling and coverage limits |

`Makefile` gains `chaos-multiprocess` and `check-chaos-multiprocess`; the latter is added to the development `release-check`. Existing native library source/object membership is unchanged. README links the new lane and preserves the historical reports' evidence scopes. The manifest policy names the new report and required files; verification remains read-only.

There are two multi-process executables. `chaos_multiprocess` links the ordinary production static archive. `chaos_multiprocess_hooks` builds the same source with preexisting `ELITE_TESTING` hooks. No unsafe alternate queue or mutation is added to production. The hooks bracket native LPs but can add control synchronization; their results are not substituted for uninstrumented weak-memory or latency qualification.

## 3. Real process and mapping architecture

Each invocation owns one authority and a bounded cohort. Children enter through `posix_spawn` and `exec`-style executable re-entry. The authority constructs the object, records the owned PID, and delivers its one-shot grant before the child attaches. Each child opens and maps the same POSIX backing independently through the existing library. Bootstrap and evidence pipes carry commands, grants, IDs, counts, and cleanup receipts, never application payload bytes. [P4 §§10–11; R3]

The coordinator also opens a diagnostic alias and records its address. It reads only immutable header bytes during live operation; ordinary slot fields are inspected only when every process for that generation is terminal. It closes its observer alias before the library destroys the object. Equal numeric mapping addresses in different processes are not interpreted as a common address space. Old and successor backing names/session IDs differ, their parent aliases coexist at distinct addresses, and no old pointer is redirected into successor storage.

Successful cleanup requires both cooperative acknowledgment/exit and actual terminal reaping for killed children. `waitpid(WUNTRACED)` is used to confirm a stop, not as evidence of death. The library's owned-child reap operation must still return NOT_READY for a stopped child. Later SIGKILL terminal observations are independently recorded. POSIX wait statuses distinguish stopped and terminated children; a signal request is not that observation. [R2]

The sole-waiter/no-unregistered-descendants contract prevents a reaped/reused numeric PID from being targeted as though it were the old child. Each kill is against a recorded positive PID that has not been reaped. A production integration that transfers mappings or permits descendants must expand its holder accounting; this harness does not assume that a parent exit fences an unregistered child.

### 3.1 Workload and data checking

N=64, B=64, POLL_ONLY and checksum NONE are fixed for the new primary histories. Each producer constructs all eight test words directly in its writable slot. The native consumer checks length, type, ID and every expected word while it owns the read. The test seed is generation-specific. Only actual ownership grants authorize those reads.

Per-consumer bitmaps record accepted ordinary IDs. Native merging and independent Python merging reject duplicates within or across consumers. Producer quotas define exact disjoint ranges, not a global numeric FIFO order between producers. A special sentinel is outside the ordinary identity range and separately counted. The summary's message total excludes it.

The raw log does not store every payload word. Its offline oracle validates exact identities/token outcomes and binds the executed source/binary that performed full payload checks. It does not claim to be a second independent byte-for-byte reconstruction of every CPU load.

## 4. Heartbeat policy and asynchronous suspicion

Each child emits sequence-numbered progress evidence. The coordinator records both the child's monotonic tick and its own receive tick. Its health deadline is

\[
D_i=t_{\mathrm{last\ received},i}+\Delta,
\]

with raw CLOCK_MONOTONIC nanosecond observations, timebase 1:1, and recorded `clock_getres`. The verifier requires a suspicion event at or after that exact deadline, tied to the last observed child sequence and receive timestamp. It rejects a claim that the same event proved death or reclaimed a slot.

The data loop observes the command pipe every 256 attempted iterations and sends progress on the 4,096-attempt cadence. These are diagnostic costs. There is no heartbeat store in queue padding, per-slot epoch, or library fast-path state. A delayed controller, full pipe, long user lease or unscheduled worker can all delay this external evidence. Expiry is intentionally a possibly false suspicion.

For controlled stop histories, the victim reports the cut and raises SIGSTOP. The parent confirms STOPPED, holds the chosen 1/10/100-ms interval, and then either sends SIGKILL or tests quarantine followed by resumption. In killed NCQ cases, the parent deliberately withholds library failure notification until healthy peers complete their quota. This separates data-plane progress from authority-triggered retirement.

The `async_resume_write` case adds the important live transition: 2P/2C traffic runs with a one-million-ID budget while the victim remains stopped holding a partial write. The coordinator checks the deadline in its event loop and calls quarantine without waiting for that traffic to finish. Active operations then either complete an already admitted transfer, receive RETIRED before acquiring storage, or retain an owned token. All three outcomes are represented.

### 4.1 Why the timeout cannot transfer ownership

The timeout history can be identical whether the writer died or will resume later with its raw pointer. Reassigning the same bytes in both cases lets the resumed writer overwrite the new owner's data. A later epoch failure cannot undo an earlier pointer write. This is the retained impossibility boundary, not a new result claimed from a handful of tests. [P3 §3.3; P4 §9.6]

The implemented response is to retain the old storage and open service in a different generation. The successor holds a checked sentinel while the old writer is resumed and performs its remaining allowed old-buffer work. The successor rechecks its own payload before release. A failed unfenced-destroy attempt must return BUSY first. No `mprotect`, memory fence, phase reset, or orphan reinsertion substitutes for that lifetime rule.

## 5. Exact fault catalogue and population

A standard repetition contains 30 multi-process cases and one resource negative. Three repetitions use 1, 10, and 100 ms respectively: this is one repetition at each selected delay, not three repetitions at every delay.

| Family | Conditions per repetition | Coverage |
|---|---:|---|
| Hooked NCQ ownership/LP cuts | 8 | QF claim; RESERVED; COMMITTED; QR post-install; QR claim; QF before/after install; delayed already-admitted QR install |
| Caller-level partial/borrow and resume | 8 | Four cases for NCQ and the same four for SPSC |
| Asynchronous retirement | 1 | NCQ stopped writer plus live healthy 2P/2C |
| Unhooked timing kill | 1 | NCQ producer's reserve/write/abort loop, exact kill instruction unknown |
| Signal storms | 12 | Two SA_RESTART policies × SPSC1/1 and NCQ1/1,4/4,16/1,1/16,16/16 |
| Resource-budget negative | 1 | One process, live leases, two quarantines/four objects |
| Total | 31 | 93 cases per three-repetition compiler campaign |

Ordinary controlled cohorts each execute 10,000 old healthy NCQ messages and 10,000 successor messages. SPSC does not substitute a new role into its old one-shot registry; it executes the successor quota only. Each storm executes 32,000 messages. The async case stops an old one-million-offer workload and executes one million successor messages.

For controlled NCQ writer victims, the plan provisions 3P/2C, leaving 2P/2C healthy workers; reader victims use 2P/3C. Some filenames contain the input placeholders `1p1c`, but the authoritative native plan and actual grants explicitly state those larger populations. Each successor is a new 1P/1C cohort. Storm filenames do represent their actual P/C values. K≤N is preserved in every case, including the 32-worker stress condition.

### 5.1 Abrupt ownership failures

The `write_claim` hook stops immediately after the QF head CAS: the token has left QF but its descriptor can still be EMPTY, epoch zero. The kill does not somehow run an abort. The final token complement must contain exactly that block, not an arbitrary EMPTY-looking block.

`write_committed` stops after the status transition and before QR installation. Healthy consumers must not see that sentinel; COMMITTED is not publication. In contrast, `write_published` stops after the QR entry CAS, so healthy consumers must consume its valid sentinel while tail help remains pending. The returned-free-token analogue checks that QF post-install does not still belong to the old consumer merely because its release function has not returned.

Reader cuts cover QR ownership before CONSUMED, an active read borrow, and EMPTY before QF insertion. Those states have different packed phases but the same prohibition on guessing a token return. The chosen ordinary mid-write cut is after four of eight words. This is not coverage of every instruction or all seven inter-word positions.

### 5.2 No-hook timing kill

The random-named scenario uses a bounded deterministic timing displacement after a confirmed completed abort. Its producer repeats reserve/write/abort and never publishes. The exact interrupted instruction is unknown, with zero or one token possibly held. The final stable snapshot must show no ready messages and at most one unavailable token. It is not a random consumer-kill or arbitrary publish/release fault distribution. The last externally supported event and interval are retained rather than relabeled as an exact LP cut.

### 5.3 Signal storms

USR1 and ALRM handlers only assign `volatile sig_atomic_t` flags. They do not allocate, print, lock, call the library, reclaim, or longjmp. Mainline code handles EINTR; restart selection is explicit. SIGPIPE is ignored with checked pipe outcomes. SIGSTOP and SIGKILL cannot be caught, blocked, or ignored, and ordinary signals may coalesce; therefore the 8,626 sent signals are not claimed as 8,626 handler deliveries. [R1]

The coordinator sends signals while endpoint work is active, periodically stops one currently unfinished worker, confirms the stop, and resumes it. The logs prove signals overlapped the workload; they do not establish which machine instruction was interrupted. Successful completion and exact token return are required after the finite storm. No CPU scheduling fairness or constant completion latency follows.

## 6. Accounting: storage, messages, and online uncertainty

Let B be the set of N physical payload blocks. During the abstract live execution the partition is

\[
B=F\uplus Q\uplus W\uplus R\uplus T\uplus X.
\]

F and Q are live free and ready membership; W/R are uniquely owned write/read leases; T covers transfers before local records catch up with an LP; X is uncertain or retained responsibility. This is logical conservation, not a claim that all six sets are atomically observable at runtime. The inherited registration table is not a crash-consistent per-token journal. [P13 §5.5]

After all participants for the generation are terminal, live private accesses no longer exist under the holder model. The forensic snapshot reconstructs F and Q from their actual live ticket intervals and classifies

\[
X=B\setminus(F\cup Q),\qquad F\cap Q=\varnothing.
\]

Each QF/QR entry must have the correct cycle for its ticket; duplicate blocks or cross-queue overlap fail. The verifier derives the possibly one-step-ahead publication frontier from the tail's current entry and validates H≤U, 0≤U−H≤N, U−1≤T≤U. It admits H=T+1 rather than imposing an incorrect head≤tail condition.

This reconstruction is post-fencing evidence. It **never reinserts X into QF**, repairs a cursor or scans a live ordinary descriptor. Entire-generation quarantine separately charges all N blocks against the storage budget. Counting N plus |X| as distinct storage would double count.

### 6.1 What final phases mean

QF members require EMPTY phase and matching epoch mirror; QR members require COMMITTED. Fixed controlled victims require the expected exact block and phase, including the QF-claim orphan at EMPTY/epoch0. An async cooperative producer/consumer records its retained block before `elite_abandon_retained`; that identity must be in X with its applicable RESERVED/CONSUMED state. A killed abort-loop producer can be between its exclusive epoch and status updates, so its single orphan is allowed the specifically bounded phase/mirror mismatch, not arbitrary control corruption.

SPSC uses the real P/C cursors and dormant all-zero status words. A killed unpublished writer has P=C=0 in this fixture. A killed or quarantined reader holding the seed has P=1,C=0. The held slot is not called free; the whole old lane remains retained until disposal. Successor P/C account for its seed plus the full ordinary quota.

### 6.2 Read success is not returned capacity

In this actual native API, return operations permit the appropriate earlier lifecycle states but reject QUARANTINED. A resumed reader's `elite_read_release` therefore yields **RETIRED/RETAINED (status3,outcome5)**. After ending all aliases it can dispose of local responsibility with `elite_abandon_retained`, not republish the token. Old uncommitted write commit behaves similarly. This was verified without changing native source.

An already admitted QR operation is different: the `late_publication` hook resumes inside it after quarantine and successfully installs its old entry, yielding **OK/PUBLISHED (status0,outcome2)**. The resulting one ready block stays in the old object. Retirement is not a transaction that cancels every previously prepared CAS. [P4 §§11.4–11.6]

### 6.3 Asynchronous cutover keeps pending messages visible

Let P_ids be the union of known completed producer prefixes and R_ids the disjoint consumer bitmap union. The asynchronous oracle checks R_ids⊆P_ids. Every ID in P_ids−R_ids must appear exactly in the final live QR descriptors. This is stronger than requiring only received≤sent, and deliberately weaker than declaring all offered messages delivered.

The primary async observations were:

| Compiler | Suspicion interval | Old published | Old checked reads | Pending old QR | Unavailable old tokens |
|---|---:|---:|---:|---:|---:|
| GCC | 1 ms | 12,491 | 12,491 | 0 | 4 |
| GCC | 10 ms | 98,105 | 98,100 | 5 | 2 |
| GCC | 100 ms | 809,453 | 809,452 | 1 | 1 |
| Clang | 1 ms | 9,003 | 8,994 | 9 | 2 |
| Clang | 10 ms | 83,895 | 83,851 | 44 | 3 |
| Clang | 100 ms | 935,782 | 935,730 | 52 | 4 |

Each of these additionally checked one million ordinary successor messages. The old unknown/retained set includes the stopped writer and, where observed, cooperative leases caught by quarantine. Pending old messages are not replayed into the successor. Whole-old-object disposal discards any remaining application work; reliable application delivery would require its own replay/deduplication protocol.

## 7. Managed recovery sequence and its bound

A controlled victim is either confirmed killed or still stopped under false suspicion. After the configured old-generation witness, the application authority quarantines the old generation, ends cooperative old worker activity, and keeps its backing charged. An attempted destruction while the paused victim can still access it returns BUSY.

The authority constructs a new object with a distinct identity, spawns new 1P/1C endpoints, and obtains both READY observations. The successor seeds and holds a checked read. Only then does the controller resume an old victim, obtain its permitted outcome, and close its old mapping. The successor validates its held bytes again before proceeding. No stage writes a successor identity into the old header or resets one-shot records.

The successor processes are **new child PIDs**, not old healthy processes reattached in place. This tests safe replacement service and memory isolation, not preservation of all application state in the same processes. The management event order can be monotonic while the application stream has an explicit session gap. No external side effect is rolled back.

### 7.1 Resource negative

A separate single-process executable holds views in two quarantined objects while one object is active and another staged. The third quarantine, fifth object allocation and premature authority destruction all return BUSY. Retained bytes remain unchanged. Only after explicit alias closure, retained-lease disposal and normal detach does cleanup finish.

This validates the existing four-object/two-quarantine admission responses under that controlled setup. It does not certify authority crash takeover, persistent quota recovery, or arbitrary partial construction. Those are distinct failure domains and remain open. [P4 §§11,13; P5 §11.4]

### 7.2 Descriptive restoration intervals

The verifier separately reports authority-observed notification/suspicion to all new READY observations and to the successor's first validated held read. Source CLOCK_MONOTONIC ticks are retained, with no subtraction of controller/scheduler overhead. The raw event that terminally reaps a child also notifies the library, so those two events are one native adapter call here; they are not assumed identical in all systems.

Among this selected sample, the maximum observed all-attached interval was **63,126,061 ns for GCC** and **15,593,829 ns for Clang**; maximum first-validated-read intervals were **63,194,452 ns** and **15,656,652 ns** respectively. These maxima mix fault classes and are descriptive, not a measured population p99 or a promised recovery deadline. No 1,000-trial-per-class qualification or uncertainty model was run. The earlier 100-ms objective is not certified by these numbers. [P5 §11.3]

## 8. Executed verification ledger

Host: Linux 6.18.44 x86-64, glibc2.41, reported AMD EPYC 9V74, five eligible logical CPU IDs0–4, visible cgroup quota400000/100000 (four CPU-time equivalents), Python3.13.5. No new exclusive CPU pinning or Apple topology is inferred. GCC14.2.0 and Clang17.0.0 compile with the inherited C11/O3 warning-as-error profile. Full flags, compiler paths/hashes, platform observations and tested executable identities are retained.

| Lane | Actual outcome | Interpretation |
|---|---|---|
| GCC final primary | 93 cases; **5,972,043** checked ordinary messages | 90 multi-process +3 resource negatives |
| Clang final primary | 93 cases; **6,080,575** checked ordinary messages | Same frozen population, independent executions |
| Raw final replays | Both campaigns passed with source and exact local executable checks | No missing completion, membership or token mismatch |
| New verifier suite | **50 tests passed** | Actual fixtures, semantic negatives and `python -O` rejection |
| GCC native regression | Core68; immutable512; IPC100k SPSC/NCQ4/4; adversarial8; limits4; old chaos3; parser1M; formal34 and result15 passed | Native and scoped checker self-tests, not a new exhaustive V1 search |
| Clang isolated regression | Same native/formal suites plus new chaos smoke passed | Explicit later complete invocation |
| Baseline Python suite | **36 tests passed** in Clang release-check invocation | Inherited binding unchanged; not Apple or CPython3.14 evidence |
| GCC ASan+UBSan | Strict O1 build; ten smoke cases; **17,200** ordinary messages; replay passed | Nine process histories +one resource negative; leak detection enabled for surviving normal-exit C processes |
| Full Clang release-check | **INCOMPLETE** at a 200-second outer window | Not promoted to a full expanded-suite pass |
| Watchdog negative | Timed out as requested; coordinator exited1, no COMPLETE marker | Incomplete result rejected, not a fabricated successful quota |
| Apple-native Turn14 | **NOT_RUN here** | Requires native build and replay of this exact derivative |

The two main campaigns alone determine the headline counts. Sanitizer messages, development runs, fixtures, old regression trials and fresh packaging smoke are not pooled into them. A killed process does not run leak-sanitizer exit processing; the sanitizer row does not pretend otherwise. No new TSan claim is made for separate mappings/processes.

### 8.1 Negative evidence and earlier failures

Initial compilation of the new harness exposed misleading-indentation diagnostics promoted to errors. The new code was braced and rebuilt strictly; those original logs remain. A stopped-reader development oracle initially expected RETURNED after QUARANTINED. Native code correctly returned RETIRED/RETAINED; the oracle and missing-token expectation were corrected to the actual contract, not the library altered to satisfy an unsafe test.

An early asynchronous verifier conditional attached an alternate branch to the wrong condition and rejected a native receipt. That development verifier was corrected and its native record retained. Both complete primary campaigns were run after the final verifier and source-binding set were frozen. Older `gcc_admitted`, `clang_admitted`, and development directories represent intermediate revisions; their numbers are not combined with the final pair.

The full combined Clang release-check exceeded the external 200-second tool window during `test_valid_all_auxiliary_replay`. The recorded log has no complete expanded-suite success. A process inspection immediately afterward found no remaining matching cohort. Separate native/formal/new-chaos regressions subsequently completed; this does not retroactively repair the interrupted combined invocation or diagnose its precise cause.

One overlarge watchdog preparation used a100M count outside this harness's4M evidence bound and was rejected before a completed campaign. The actual watchdog negative then used valid counts and a20-ms outer timeout; its record has `timed_out:true`, exit1 and no COMPLETE. Both are retained as different negatives. A streaming execution tool request failed before launching a process; it supplies no test result.

## 9. Independent oracle and trust boundary

`verify_chaos_results.py` never calls the production library to decide expected outcomes. It parses raw JSONL with duplicate-key/nonfinite rejection; checks monotonic parent/child sequences and ticks; validates PID/grant/command lifetimes; distinguishes STOPPED from terminal SIGKILL; and requires cleanup evidence before clean exits and terminal evidence before ordinary snapshots.

It recomputes the immutable prefix CRC with Python's standard library, validates unchanged identity and actual mode/geometry, reconstructs NCQ live cycles and phase/epoch membership, and reconciles SPSC cursors. It joins producer quotas, child completion records and consumer bitmaps, then applies each case's retained/pending/free rules. A source-code token count or printed PASS is not accepted alone.

Campaign verification additionally requires the exact canonical profile/case list, selected arguments, plan hash, execution returncodes, timeout flags, stdout/stderr hashes, and all three binary identities. `--source-root` checks 18 relevant source/build files; `--build` checks the exact tested executables. Missing output cannot become an empty successful list. Verification uses explicit conditions, not Python assertions removable by optimization.

The 50-test oracle suite includes mutated raw times, counters, identities, stop/kill status, header bytes, sentinel outcomes, live-entry cycles, duplicate token ownership, missing orphan identity, wrong epoch/phase, false SPSC reuse, missing signal classes, premature destruction, fabricated zero-loss async cutover, unknown retained tokens, bad resource limits, malformed JSON, and incomplete plans. Mutations are rejected even when ordinary file hashes are recomputed in the test fixture.

Receipt consistency is not remote attestation. A dishonest recorder could fabricate internally consistent events. The trusted scope is this source/binary, native checks, retained local executions and separately reviewable analysis. Not every native LP or payload byte is exported, and a finite completed history is not proof of all possible interruptions. [P5 §§1,7]

## 10. Remaining coverage and explicit nonclaims

The frozen production model excludes arbitrary malicious writes, a caller using a released pointer, unregistered mapping transfer, or a host that loses volatile state. A SIGSEGV caused only by an otherwise conforming process stopping is conceptually different from a bug that overwrote shared control before the fault; no new SIGSEGV experiment or hardware power reset was run here. [P3 §1.2]

This turn does not execute every C01–C20 boundary100 times, all seven inter-word cuts,1,000 random kills per profile,1,000 recovery trials per class, parked-notification death gaps, every JOINING/detach/creator crash, full authority takeover or the planned complete holder formal model. All-tokens-retained exhaustion is represented by the resource contract and inherited tests, not a newly claimed many-process live-holder population. Fresh standard runs can increase repetitions, but cannot turn an absent scenario into implemented coverage.

A finite native run can demonstrate no observed invalid read, successful independent work under a selected pause, and complete final accounting. It cannot establish a universal no-deadlock theorem, a bound on LL/SC arbitration, per-caller fairness, a scheduler deadline, or survival of unlimited unfenceable failures. The earlier conditional queue proof remains relevant but is not replaced by signal counting. [P13 §§7,12]

There is also no gap-free cross-generation application-delivery claim. A pending old message may be deliberately discarded with the safely retired generation, and external side effects require application-level replay/deduplication semantics. An unknown outcome is useful evidence, not a license to call it corruption or success.

## 11. Reproduction and packaging

From a clean extracted repository:

```sh
python3 tools/verify_release.py --strict
make CC=clang BUILD=build/chaos chaos-multiprocess check-chaos-multiprocess
python3 tools/run_chaos.py --build build/chaos --out chaos-new --trials 3
python3 tools/verify_chaos_results.py chaos-new --source-root . --build build/chaos
```

Use a new output directory. GCC is an independent build profile:

```sh
make CC=gcc BUILD=build/gcc-chaos chaos-multiprocess
python3 tools/run_chaos.py --build build/gcc-chaos --out chaos-gcc-new --trials 3
python3 tools/verify_chaos_results.py chaos-gcc-new --source-root . --build build/gcc-chaos
```

The diagnostic sanitizer lane is separate:

```sh
make CC=gcc BUILD=build/asan-chaos \
  OPT='-O1 -g -fno-omit-frame-pointer -fno-lto -fsanitize=address,undefined -fno-sanitize-recover=all' \
  LDFLAGS='-fsanitize=address,undefined' chaos-multiprocess
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  python3 tools/run_chaos.py --build build/asan-chaos --out chaos-asan-new \
  --smoke --trials 1 --messages 1000 --storm-messages 3200
```

The primary source tree contains all raw final JSONL/bitmaps, run/source/binary receipts, negative inputs and logs. Compiled executables and third-party runtimes are not redistributed. Rebuilding at a different path may change debug/binary hashes; the old run's source identity can still be checked without pretending the new executable bytes match the original.

All shipped files except `MANIFEST.sha256` are in the complete manifest. All policy-defined source/build inputs, LICENSE, .gitignore and the audit are in `SOURCE_MANIFEST.sha256`. Neither verifier regenerates manifests. A final clean extraction and fresh build/replay are documented separately to avoid changing the packaged evidence after its manifest is sealed. The detached receipt records actual upload/readback outcomes rather than assuming synchronization.

## 12. Acceptance statement

**Accepted within the executed scope:** true separate-process shared-memory communication under the selected producer and consumer deaths; no observed partial/torn delivery; finite independent NCQ progress with spare capacity; strict final token partition including orphaned/retained blocks; distinct successor isolation under resumed old access; signal-storm completion; and rejection of unsafe cleanup beyond the fixed resource budget.

**Not accepted as a new guarantee:** autonomous old-token recovery, same-generation leak-free availability after every crash, full Turn5 V8 qualification, new Apple-native execution, quantitative population recovery certification, or a definitive v1.1.0 release.

The useful invariant is not “nothing can ever be lost.” It is that the system does not convert missing ownership evidence into permission to reuse memory. This turn makes that boundary executable and auditably distinguishes safe quarantine from silent capacity theft.

## 13. Sources and authority

[P13] Authenticated Turn13 archive and preserved `13_Turn_13_Formal_Verification_Report.md`. Its §1/§12 explicitly separates finite control graphs, handoff analysis, source correspondence and open lifecycle/tool coverage. User's Apple/M3 Max/commit receipt remains LAB_REPORTED.

[P4] `docs/reference/04_Turn_04_C11_ABI_and_Assembly_Specification.md`, SHA-256 `6aaa3d6706c29096cfab56ea1fe569afa561c4cba7caea26ebd3c09cc9812386`; §§8–13 govern exact fields, ownership, retirement, no-revocation and holder/resource accounting.

[P5] `docs/reference/05_Turn_05_Verification_Harness_and_Test_Plan.md`, SHA-256 `bc7ae2702cecb8d46820cf5365046fe40dda36c59dde9c80fda98ff7833ecdf8`; §§1,7,10–11 govern lanes, invariant evidence, cut catalogue, control-plane suspicion and managed generation recovery.

[P3] Governing Turn3 report, SHA-256 `7ad5fbd43134e7d3419c5a1f76d030e25d051510dbc6f20ce173b3d0eccff605`; §§1,3–4 supply process-failure/ownership-gap boundaries. The current source, not a wording shorthand, defines actual release behavior.

[R1] Linux man-pages, signal(7), consulted24September2026. SIGKILL/SIGSTOP disposition, signal coalescing and restart/interruption semantics. https://man7.org/linux/man-pages/man7/signal.7.html

[R2] Linux man-pages, wait(2)/waitpid, consulted24September2026. Owned-child status, WUNTRACED/WIFSTOPPED versus terminal WIFSIGNALED/WIFEXITED and reaping. https://man7.org/linux/man-pages/man2/waitpid.2.html

[R3] Linux man-pages, shm_open(3), consulted24September2026. POSIX object creation, mapping and name lifetime; unlink is not a mapping revocation primitive. https://man7.org/linux/man-pages/man3/shm_open.3.html

The exact hypotheses, native tests, counts, asynchronous observations and oracle equations are project work. OS references establish the interface contracts; they do not certify this new harness. Hash receipts provide byte integrity, not independent execution attestation or universal safety proof.
