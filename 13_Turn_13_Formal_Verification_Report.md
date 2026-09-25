# Turn 13 — Formal Verification, Executable Models, and C11 Ordering Boundaries
## ELITEIPC 1.0.1 / LE128-V1 / Lane V1

**Project:** Leonid Majbits / Gemini Operator Lab, paired with Leon  
**Evidence date:** 24 September 2026; exact runtime/tool output retained in `evidence/turn13/`  
**Status:** Executed exact finite-state and designated handoff-graph verification, with explicit abstraction and resource bounds. Full preregistered V1 qualification is not claimed.  
**Canonical-SHA256:** `997ee821fb32756b6b791a3340cdd9e1e496181a06c277b2ce814a2f1a475fc5`
**Canonical convention:** Replace only this field's 64 hexadecimal characters with 64 ASCII zeros before SHA-256 hashing. The detached `.md.sha256` hashes the literal finalized file.  
**Authoritative input:** `Elite_Systems_LockFree_RingBuffer_Turn_12_Matrix_Bundle.zip`, 40,273,115 bytes; SHA-256 `b970be9a985870dc98c92eeab04b9acd470f09c99fd503bf77bd7720b3e10e93`.  
**Destination:** Deployment Research / Elite_Systems_LockFree_RingBuffer  
**Evidence labels:** EXECUTED means a retained local invocation; DERIVED means an analytical argument in this report; MODEL means an explicit abstraction; NOT_RUN means no tool/target result is claimed. Historical Apple observations remain LAB_REPORTED.

---

## 1. Verdict

**The frozen SPSC and NCQ-SC64 reference survives the completed finite searches and the declared release/acquire handoff checks. The negative controls are rejected by the same oracles. The queue implementation is not changed to make the models pass.**

The deliverable separates four questions that cannot safely be compressed into one “formally verified” label:

1. Does the concrete control/ownership model have a bad reachable state within the stated population and reservation limits? The standalone exact-state explorer answers that question exhaustively for seven completed configurations.
2. Do the designated ordinary-memory handoffs have the required happens-before paths? A separate small execution-graph checker enumerates covering reads-from choices for the specified SPSC skeletons and examines the NCQ first-ready-handoff skeleton.
3. Why should the properties continue beyond those finite executions? Sections 4–8 give parameterized induction and progress arguments, conditional on the source-to-model correspondence, nonwrapping lifetime, coherent memory and admitted atomic primitives.
4. Has a standard external C11/RC11 checker or Spin independently verified the actual source? **No.** Reviewable C11 inputs and Promela models are supplied, but GenMC/CDSChecker/Spin execution remains NOT_RUN. A native compiler accepting a C11 file is not an external model-checking result.

**Executed baseline total:** 2,158,499 exact states and 5,700,615 explored transitions across seven separately bounded configurations. Seven structural mutation witnesses, four order-only mutation families, a reduced-width wrap-identity witness, and the cutoff control produced their required adverse outcomes. The complete expanded Python discovery run passed **235 tests**, including **34 formal self-tests and 15 formal-receipt tests**; those 49 are a subset of 235, not extra disjoint tests.

The current positive disposition does **not** discharge the entire Turn 5 V1 register: its N=4/2P2C eight-reservations-per-endpoint exploration, full lifecycle/holder model, and parking model have not all been exhausted. An attempted larger search reached a resource bound; its result is retained as INCOMPLETE. No finite threshold, workload limit or successful check establishes a wall-clock deadline.

### 1.1 Important correction to the requested negative result

Changing a strong atomic head CAS from sequential consistency to relaxed ordering does **not** split its read-modify-write or allow two callers to claim the same nonrepeated expected head. Atomicity and cross-location ordering are different guarantees. The suite includes a positive control for this distinction rather than manufacturing a duplicate-ticket trace. [R1]

A genuine order-only NCQ mutation removes ready-entry release/acquire synchronization. In the actual source, the ordinary descriptor epoch is read **before** the subsequent acquire of `status_word`. The lost queue handoff therefore creates an ordinary epoch data race; the later status acquire cannot repair an earlier read. Structural mutations separately demonstrate double allocation and invalid publication order. This satisfies the underlying falsification objective without making the incorrect assertion that relaxed means non-atomic.

## 2. Input custody and unchanged production surface

The exact Turn 12 ZIP named by the user was hash-checked and extracted into a new tree. Its original archive integrity and release manifests passed before editing: 5,673 complete entries and 90 source entries. The frozen protocol remains the Turn 4 ABI and Turn 2 NCQ-SC64 variant. [P2, P4, P5, P12]

**Twenty inherited native/Python source and header files remain byte-identical to the parent**, including the queue algorithms, topology/PMC code and hardened buffer exporter. They are checked against the parent; the resulting inventory is `evidence/turn13/CORE_BINDING_CONTINUITY.json`. There is no changed queue memory order, new shared field, alternate token allocator, or new pointer-recovery policy. The library remains version 1.0.1 with wire ABI `0x00010000`.

`formal/SOURCE_BINDING.json` binds the models' manual review to eight complete native source/header hashes and selected critical expressions. `tools/check_formal_binding.py` refuses drift. Full-file identity is stronger than checking that a source file happens to contain the word `acquire`; nevertheless, **a hash is not a compiler, a proof of semantic equivalence, or an automatically generated refinement proof**.

The user's Darwin root-resolution/test-directory/Makefile adjustments have been reconstructed in this derivative and recorded separately. The supplied short Git name `4dd9bbc` was not fetched as a Git object, so this report does not claim to authenticate that commit or every local modification on the Mac.

The receipt's statement “31 runnable cells, three trials, 90 records” has an arithmetic discrepancy: 31×3 is 93. The Apple raw trial inventory was not supplied in this turn. We retain that as a provenance question, not a queue counterexample and not a reason to infer additional completed qualification. No new Apple performance or cache geometry claim is used in these formal conclusions.

## 3. Implementation inventory and abstraction map

| Artifact | Role | What it is not |
|---|---|---|
| `formal/models/spsc.py` | Cursor/snapshot, reservation, abort, epoch, two-word payload and borrowed-alias state transitions | A complete hardware memory model |
| `formal/models/ncq.py` | Both SC index queues, saved observations, strong CAS, helping, phases/epochs, payloads and token ownership | QR-only testing or a replacement queue implementation |
| `formal/explore.py` | Exact breadth-first reachable-state enumeration and nonprogress-edge cycle analysis | Random stress, hash-only visited state or a tool that relabels cutoffs as PASS |
| `formal/handoff.py` | Explicit sb/rf/sw/hb graph construction and ordinary-access conflict checks | A complete ISO C11 or RC11 implementation/frontend |
| `formal/boundaries.py` | Finite scalar ceiling checks, 260-advance wrap illustration, small gate histories and symbolic starvation | Complete 260-lifecycle NCQ exploration or a full authority model |
| `formal/targeted.py` | Stopped-publisher histories over the same NCQ transition model | Exhaustive crash/recovery verification |
| `formal/*.pml` | Handwritten, reviewable SC/control Promela specifications with LTL | Executed Spin verification or implicit weak-memory simulation |
| `formal/c11/*.c` | Genuine C11/pthread handoff and atomicity inputs | The entire production source under GenMC |
| `tools/run_formal.py` | Frozen population, tool invocations, exits, output/source hashes and completion record | A success-only summary that loses failed runs |
| `tools/verify_formal_results.py` | Manifest/schema/plan checking; witness and auxiliary replay; optional full baseline re-execution | Remote attestation or independent trust in a second implementation |
| `tests/test_formal*.py` | Checker self-tests and rehashed semantic negative receipts | More production messages or evidence of target timing |

### 3.1 What a model step contains

An NCQ transition contains at most one shared queue atomic operation, accompanied where needed by process-local decisions and observer-only ghost bookkeeping. Head read, entry read, head CAS, tail read, entry-install CAS and tail help are distinct interleavable steps. CAS losers discard their complete dependent observations. Separate owner steps update the ordinary epoch mirror, phase, and two payload words.

Some private metadata validation and phase work is grouped. For example, the NCQ claim-side model groups validation with the CONSUMED phase/alias start; the source's ordering-sensitive epoch-before-status read is analyzed separately by the handoff graph. No claim is made to have scheduled between every native instruction, every diagnostic validation, or every ordinary descriptor byte access.

The model's `U` frontier and token owner are ghost variables. They track facts at linearization points; they are neither new shared ABI fields nor fences available to executing participants. Updating observer state in the same mathematical transition as a successful CAS does not serialize otherwise independent production operations.

The model initialization assumes complete construction and exposure. The 128-byte spacing has no speed or visibility semantics inside the abstract transition system. It is covered by the separately compiled ABI and the admitted atomic-alignment contract, not simulated caches.

### 3.2 Exact search and explicit reductions

Full immutable states are dictionary keys; equal hash codes still require complete equality. A deliberate collision test confirms unequal states are not merged. No bitstate hashing, symmetry reduction, partial-order reduction, or random successor sampling is used. Dead local ticket/entry values are reset only after they can no longer affect a baseline branch. Mutated stale-retry paths deliberately retain their unsafe values.

The explorer records visited states, transitions, terminal count, maximum shortest-path depth, queue-frontier coverage, pending search frontier, cutoff cause, model configuration and a digest of the deterministic visited-state sequence. A counterexample includes the initial state and each concrete successor on a shortest breadth-first prefix. That is minimal in model transitions, not necessarily minimal in source instructions.

A two-word payload abstracts representative separately written locations. It can expose a mixed-word publication or early reuse. The general proof below, not the number two, extends ownership ordering to every byte written under the same lease discipline.

## 4. SPSC safety: parameterized proof

Let P be fully published messages and C be fully returned messages. Each is written only by its endpoint. Let c-hat and p-hat be acquired peer snapshots. The modeled reservation is private and at most one write and one read lease can be held by their respective endpoints.

### 4.1 Capacity and exclusive physical reuse

Initially P=C=0. A producer admits reservation of ticket P only if P−c-hat<N. With no wrap and monotonic peer observations, c-hat≤C; therefore P−C<N before publication and P+1−C≤N afterward.

A consumer admits ticket C only if C<p-hat. Since p-hat≤P, C<P and incrementing C preserves C≤P. Together:

\[
0\le P-C\le N.
\]

A held read does not advance C. The next use of its physical slot is ticket k+N. Such a write requires acquired reclamation beyond k; while the old lease remains live, that progress has not been published. Therefore the capacity predicate cannot admit a conforming overwrite of the held slot.

An old acquired snapshot can reject usable capacity/data conservatively. It cannot authorize a future publication or a reclamation that has not occurred. A failed observation is not a promise of the latest wall-clock queue size. [P2 §§4.1–4.6]

### 4.2 Publication orders all required writes

For any ordinary descriptor or payload write W(k), choose the release publication S_P(v) actually observed by the consumer's covering acquire L_P(v), where v>k. The sole producer has sequenced every W(k) before S_P(v), including when v covers several earlier messages. The consumer reads only after the acquired covering snapshot:

\[
W(k)\to_{sb}S_P(v)\to_{sw}L_P(v)\to_{sb}R(k).
\]

Transitivity yields W(k)→hb R(k). The release/acquire rule requires the relevant reads-from edge. An unrelated lifecycle acquire or a checksum does not supply it. [P4 §8.1; R1]

A cached snapshot remains a valid covering synchronization event for the entries it covers; the proof does not require a new acquire for every byte or a new SC fence. The finite graph skeleton uses a load per modeled message, while the control model also permits cached-snapshot use. Their scopes are declared rather than silently identified.

### 4.3 Reclamation is the other half of the proof

Let R_last(k) include the last access by every valid view, and let the next writer use the same physical slot for k+N. Reclamation is published only after that last access; the writer must observe a covering release before reusing it:

\[
R_{last}(k)\to_{sb}S_C(u)\to_{sw}L_C(u)\to_{sb}W_{first}(k+N),\qquad u>k.
\]

Thus R_last(k)→hb W_first(k+N). Combined with publication and single-owner serialization, every conflicting ordinary read/write of that physical storage is ordered between adjacent lifecycles and, transitively, between more distant lifecycles.

**Consequence:** data-race freedom and coherent committed payload observation follow within the admitted ownership model. No whole-payload atomic load or 128-byte atomic operation is required. A multi-line payload is not made torn merely because its stores are ordinary; conversely, a checksum/version check after an unauthorized read cannot legalize a data race.

Aborting a private write does not publish P and consumes its epoch. A later reservation overwrites the unpublished bytes under exclusive ownership before any valid publication. This is modeled explicitly.

### 4.4 Executed release/acquire graph fragment

`formal/handoff.py` unrolls fixed SPSC lifecycles, includes ordinary epoch/x/y accesses, enumerates all designated covering publication and reclamation reads-from assignments, and computes sequenced-before, synchronizes-with and transitive happens-before. It checks per-location atomic coherence and excludes causal cycles for this fragment.

The positive N=2, M=4 skeleton has 18 consistent covering graphs; M=5 has 54. No ordinary conflict lacks ordering, and the unique latest happens-before write supplies the expected value. The two release mutations are processed by the same analyzer and produce explicit unordered conflicting accesses.

The graph code deliberately does not implement arbitrary competing RMW modification orders, consume dependencies, all C11 fence forms, general out-of-thin-air rules, or a C parser. These small monotonic handoff patterns have a direct analytical argument; calling this an exhaustive checker for arbitrary C11 programs would be false. The optional external C11 inputs preserve the ordinary accesses so a suitable tool can independently test them.

## 5. NCQ-SC64 dual-queue proof

### 5.1 Three distinct identities

A token is a payload-block identity, not a queue ticket. A queue ticket is a monotonically advancing logical position mapped to physical entry t mod N. A descriptor's phase and epoch are validation data, not an independent proof that it belongs to QF or QR.

For N=2^n, the entry word is:

\[
E(t,b)=N\lfloor t/N\rfloor+b,\qquad 0\le b<N.
\]

A queue entry is one admitted atomic word; padding does not turn several objects into a compound atomic transaction. QF initially contains all tokens with H_F=0,T_F=U_F=N. QR initially has H_R=T_R=U_R=N, while physical entries carry older placeholders.

The exact source uses **CAS, not unconditional ticket fetch-and-add**, for removal, publication and helping. No QR FIFO position is reserved while the caller constructs an arbitrary payload. [P4 §§4.2,8.2]

### 5.2 The frontier invariant and publication order

For each queue:

\[
H\le U,\qquad 0\le U-H\le N,\qquad U-1\le T\le U.
\]

A caller observes t=T. If the current physical entry already has t's cycle, publication occurred and tail advancement can be helped. Exactly one older cycle authorizes an installation attempt. A successful full-word entry CAS publishes the complete index and advances ghost U; tail help follows.

No publication at t+1 can be attempted through the current tail before some actor advances T beyond t, and that advancement requires t's installed generation. Inductively there is no unpublished gap below U. After publication the tail can lag by one; it cannot lag by an arbitrarily long prefix or lead an unfinished reservation.

A consumer can remove the just-installed entry before tail help. Therefore H=T+1 is legal, and the completed finite searches reach it. The common but incorrect assertion H≤T would reject correct executions.

### 5.3 Why installing into a reused entry is safe

The enqueuer owns one unique token outside the destination queue. Consequently that destination cannot already contain all N distinct tokens. At publication ticket t=U, the preceding live occupancy is at most N−1, so t−N<H. The physical entry from the previous cycle has already been removed.

A competing stale observer can still hold the old entry bits locally. That is not ownership of its payload. Its head CAS cannot succeed after H has advanced, provided H never repeats. The model never dereferences a payload on the strength of merely observing an index.

This capacity argument is compositional: duplicating a token elsewhere invalidates it. It is why the formal model includes QF, QR, payload holders and transfer intervals rather than proving QR alone.

### 5.4 Unique claim and FIFO linearization

A successful strong CAS from H=h to h+1 is the removal linearization point. At most one such CAS can succeed for that h before a repetition/reset; neither is allowed. A loser refreshes the entire saved ticket/desired/entry/index set. It cannot reinterpret the updated `expected` scalar as permission to retain stale payload identity.

An observed exactly-one-older entry means ticket h has not yet been installed at that SC observation. If H had already passed h, that ticket's current-or-later entry generation would necessarily have been installed first. Hence in the valid SC history H=U=h at that observation: EMPTY has a legitimate linearization point. Newer mismatches require retry, not an EMPTY invention.

Assign enqueue LPs to entry installation, successful dequeue LPs to head advancement, and empty LPs to the justified entry observation. Contiguous publication and monotonic head claims then give an internal FIFO history consistent with those operation intervals.

This orders QR **publication**, not producer reservation time, numeric application IDs, consumer completion time, or external side effects. Those distinctions are not altered by formal notation.

### 5.5 Exact conservation including invisible transfer intervals

Partition all N unique tokens into F (live QF), Q (live QR) and O_i (private construction/read/transfer responsibility of endpoint i). Within the steady-generation model:

\[
F\uplus Q\uplus\bigcup_i O_i=\{0,\ldots,N-1\},\qquad |O_i|\le1.
\]

All sets are disjoint. Initialization establishes the partition. Each successful head CAS removes exactly one token from the corresponding queue and assigns it to its winner. Each successful entry CAS moves exactly the caller's private token into the destination queue. Aborting returns that same token once. All other steps preserve membership.

A publisher may still have an old block index in local variables after the LP. It no longer belongs to O_i and may not be accessed through that stale ownership. Tail/response steps are legitimate local or queue bookkeeping, not another payload owner.

For an extended failure model, stopped/uncertain/quarantined tokens must remain a separate part of the partition. Conservation does not mean every token is available or recoverable. Neither phase scanning nor timeout reclamation appears in this implementation.

### 5.6 Both ownership directions carry memory ordering

The baseline QR chain is:

\[
W_{b,g}\to_{sb}\text{COMMITTED}_{rel}\to_{sb}\text{QR install}_{SC}
\to_{sw}\text{QR observe}_{SC}\to_{sb}\text{head claim}_{SC}\to_{sb}R_{b,g}.
\]

The consumer has both synchronization and unique claim before its ordinary metadata and payload accesses. For reuse:

\[
R_{b,g,last}\to_{sb}\text{EMPTY}_{rel}\to_{sb}\text{QF install}_{SC}
\to_{sw}\text{QF observe}_{SC}\to_{sb}\text{head claim}_{SC}\to_{sb}W_{b,g+1}.
\]

The descriptor's ordinary mirror and packed atomic phase may temporarily disagree during exclusively owned setup. Only queued/handed-off states require their declared correspondence; a diagnostic reader cannot sample them concurrently as a transaction.

The SC queue proof and handoff argument assume the admitted C11 atomic implementation on shared aliases. They do not independently establish every mmap effective-type or cross-process ABI condition on a new compiler/OS combination.

## 6. Non-ABA theorem: stop before repetition

For a w-bit ticket domain and admitted capacity N, define J=2^w−N−1. Every head/tail desired increment requires its saved t<J. A successful update therefore produces:

\[
0\le t<t+1\le J<2^w.
\]

Cursor values only increase and never reset during that generation. An old expected value cannot become current again. The packed cycle/index word for a physical entry advances to the next cycle; it likewise cannot repeat within the admitted domain. The algebra applies to w=64 without enumerating 2^64 events.

The crucial enqueue check is **before entry installation**: publishing a terminal entry and discovering only later that tail cannot advance would be too late. The source contains the guard in the required place.

Epochs independently satisfy g≤G=2^62−2; a new reservation requires g<G, including after aborts. The four phase values fit in `(g<<2)|phase`. These checks must all hold; a large queue-ticket field cannot compensate for a reused session or untracked old pointer.

The executable boundary checker performs 577,491 scalar checks over small widths, packed entry values, the native-width edge cases and reduced generation boundaries. A separate 260-advance **scalar head-identity** trace demonstrates repeat after 256 increments when wrap is explicitly allowed. With N=4,J=251 and saved head 4, the baseline stops after 247 advances at 251, before the repeat.

This long trace is not mislabeled as exhaustive exploration of all 260-message dual-queue histories, nor as an executable counterexample obtained by deleting only one guard from the production library. Other native cycle comparisons also assume nonwrapping operation. The artifact specifies exactly which abstraction was mutated.

**There is no “non-ABA across 64-bit wrap” promise.** The supported guarantee is no represented identity repetition while old references can remain live. Reaching the ceiling can stop service and retain outstanding responsibility; it does not guarantee a caller receives success before retirement.

## 7. Progress and its limits

### 7.1 Conditional unbounded internal-queue argument

Assume a finite set of endpoints, valid unique-token admission, no wrap/reset, SC queue observations and an admitted strong-CAS primitive that completes its abstract operation.

If no operations complete after some point, only finitely many already pending enqueues can have installed entries without returning. Every such installation leaves at most one helpable tail step and a finite local response tail. Once those finite changes are accounted for, a still-running enqueuer sees either a current installed entry it can help or an eligible older entry it can replace. An infinite sequence of competing successful replacements would itself provide global progress.

A dequeue either returns a justified EMPTY, wins the head CAS, or observes another participant's advancement. After finitely many competing changes, fresh SC observations cannot remain an arbitrary obsolete head forever. Continued valid head-CAS losses imply continuing claims. A stopped producer holds a private token before publication or leaves an already consumable entry afterward; it does not own a uniquely blocking unfinished FIFO position.

This proves the stated internal-queue lock-free property under the abstract assumptions. It is not a bound on OS scheduling or LL/SC retry arbitration. A source-level strong CAS can still require a separately admitted machine primitive implementation. [P2 §6.8; P3 §7; R1]

### 7.2 Executed no-progress graph check

For each fully explored graph, remove edges labeled as queue LPs or completed empty/operation responses. Apply exact topological elimination to the remaining graph. A nonempty cyclic residue witnesses the possibility of indefinitely many modeled nonprogress transitions; every admitted baseline had an empty residue.

The explorer separately rejects an enabled-work model that ends in a nonterminal deadend. All admitted baseline counts for such deadends are zero. The checker also has a negative self-loop control that it rejects, and a completed-EMPTY self-loop control that it does not mislabel as deadlock. There is no implicit scheduler-stutter edge: “the CPU never runs this thread” is not an executed queue instruction.

This finite graph result corroborates the model's progress structure. It does not substitute for a general temporal proof of the actual application. Queue LPs are progress markers, and the analytical finite-participant argument is needed to relate infinitely many LPs to completed operations. Resource exhaustion, retired generations and permanently held application leases remain separate outcomes.

### 7.3 Individual starvation survives global lock-freedom

A symbolic schedule can repeatedly let a victim read h, let a peer successfully claim h and complete, then let the victim fail CAS(h,h+1). Both actors take steps; the peer keeps completing while the victim does not win. Thus no retry bound depends only on participant count.

In the unbounded-ticket mathematical queue this pattern can repeat indefinitely. In the production finite domain the generation eventually retires instead of wrapping; that still supplies no successful-return deadline for the victim. Neither the model nor the report equates weak scheduling fairness with per-operation CAS fairness.

Two targeted stopped-publisher histories run on the same model: one paused private writer leaves capacity for eight healthy peer messages; one paused post-LP publisher permits its original message plus eight peer messages and then resumes only allowed bookkeeping. A third prescribed trace pauses a consumer after its QR entry read, lets a peer complete nine removals (more than two physical N=4 laps), and verifies that the resumed stale head CAS fails without changing ownership or payload. These are explicit prescribed traces, not nanosecond experiments or an exhaustive arbitrary-crash proof.

## 8. Mutation results

| Mutation | First failed property | Shortest model witness steps |
|---|---|---:|
| `mutant_split_head` | `DOUBLE_ALLOCATION` | 8 |
| `mutant_stale_head_retry` | `DOUBLE_ALLOCATION` | 9 |
| `mutant_overwrite_current` | `OUT_OF_ORDER_PUBLICATION` | 25 |
| `mutant_tail_before_entry` | `QUEUE_FRONTIER_ORDER` | 11 |
| `mutant_post_lp_write` | `POST_LP_PAYLOAD_ACCESS` | 13 |
| `mutant_early_return` | `RECLAIM_WITH_LIVE_ALIAS` | 21 |
| `spsc_publish_early` | `SPSC_TORN_OR_UNCOMMITTED` | 4 |
| SPSC publication release → relaxed | Ordinary payload/epoch race; 18/18 consistent covering graphs violate | Separate event graph |
| SPSC reclamation release → relaxed | Old-reader/new-writer race; 18/18 consistent covering graphs violate | Separate event graph |
| NCQ QR install release removed | Epoch read before status acquire lacks HB | First-handoff graph |
| NCQ QR observation acquire removed | Same early epoch-read race | First-handoff graph |
| Head wrap admitted | Stale expected representation repeats after 256 advances | 260-step scalar trace |
| Strong CAS SC → relaxed, atomicity only | **No duplicate claim**; positive control passes | Four modification-order histories |


### 8.1 Why the NCQ order-only witness is specific

In `el_read_metadata`, the code first performs `uint64_t epoch = d->epoch;` and then acquires `d->status_word`. The normal queue-entry synchronization already orders the first ordinary read. If that queue handoff is removed, even an acquire of the correct COMMITTED status afterward cannot retroactively establish a happens-before edge to the earlier epoch read.

The graph witness therefore identifies `P_epoch` versus `C_epoch_BEFORE_status`. It does not claim that the later payload necessarily races in the same skeleton: the retained status acquire can order subsequent payload reads. This distinction prevents a misleading all-or-nothing mutation diagnosis.

The resulting program has a data race under the stated ordinary-access rules. A data race is already a decisive correctness failure; the report does not need to invent one physically observed stale byte or duplicate ticket on x86.

### 8.2 Structural witnesses are not mere memory-order changes

The split-head mutation replaces a conditional atomic claim with an effectively separately validated/unconditional update. The stale-retry mutation changes the expected head while retaining an obsolete entry. Both can grant or attempt to grant a token already owned elsewhere.

The tail-first mutation violates the publication frontier immediately. Overwriting a winner's installed current-cycle entry violates contiguous publication. Post-LP descriptor access is a direct ownership-contract violation even before selecting a later competing write. Early return retains an alias across a transfer that permits reuse. Each trace names its first violated invariant rather than claiming every failure is simultaneously a data race, deadlock and FIFO inversion.

Mutants live only in formal fixtures or opt-in C11 checker inputs. No production unsafe path is enabled or shipped as an alternate runtime mode.

## 9. Exact executed model population

| Model | N | P/C | Reservations per producer | Abort choices | States | Transitions | Terminal states | Max shortest depth |
|---|---:|---:|---:|---|---:|---:|---:|---:|
| spsc-control-ownership | 2 | 1/1 | 8 | Yes | 84,174 | 180,721 | 1,502 | 108 |
| spsc-control-ownership | 4 | 1/1 | 6 | Yes | 41,287 | 87,889 | 328 | 80 |
| ncq-sc64 | 2 | 1/1 | 8 | Yes | 764,188 | 1,523,287 | 3,619 | 220 |
| ncq-sc64 | 4 | 2/2 | 1 | Yes | 166,858 | 613,040 | 38 | 62 |
| ncq-sc64 | 4 | 2/1 | 2 | No | 185,290 | 521,812 | 26 | 113 |
| ncq-sc64 | 4 | 1/2 | 4 | Yes | 749,844 | 2,160,826 | 316 | 115 |
| ncq-sc64 | 8 | 2/2 | 1 | Yes | 166,858 | 613,040 | 38 | 62 |


`reservations` is the per-producer budget. With aborts enabled, some paths deliver fewer messages; eight reservations is not mislabeled as eight committed messages on every path. Consumers drain whatever was published. The N=4,2P/1C two-reservation cell excludes abort branching explicitly; it is not an unnoticed omission from an alleged all-choices population.

All named completed baseline graphs have zero remaining BFS frontier and no detected nonprogress cycle. Their state counts are finite graph sizes, not hardware test messages, repeated probabilistic samples or unique C11 executions. A count summed over separate configurations is not a single combined state space. The N=8 one-reservation cell does not exercise eight-block payload reuse merely because its capacity is eight; its identical graph count to the corresponding N=4 cell is not hidden. Repeated physical reuse is exercised in the N=2 eight-reservation model and in the prescribed N=4 stale/stopped histories.

### 9.1 Retained incomplete and intermediate work

Early exploration used smaller state representations and different local-state normalization. Those results remain historical/intermediate and are not pooled into the final table. Introducing the separate packed-status generation state and clearing dead locals changes graph identity; final hashes bind the final model revision.

The attempted larger N=4/2P2C search was interrupted by an outer execution window; a separate explicit bounded attempt also returned INCOMPLETE. The final-source N=4/2P2C/eight-reservation probe separately reached its 200,000-state cap after 678,844 transitions and returned INCOMPLETE with no terminal state; it is not part of the positive total. A preliminary larger asymmetric-abort campaign reached its resource cap. The admitted contrast matrix was then frozen with a separately labeled no-abort asymmetric cell. This is a declared coverage reduction, not proof that the larger space has no counterexample.

The final default uses a 1,200,000-state and 120-second per-exploration cap with a separate process timeout. An expected five-state cutoff control returns exit 2 and remains INCOMPLETE even when the surrounding test correctly expects that result. `--extended` exposes the N=4/2P2C eight-reservation attempt under the explicit cap; it must not be counted as passed when capped.

No selected baseline was accepted on a partial frontier, timeout, unsupported external tool, or missing output.

## 10. Verifier trust and falsification

`verify_formal_results.py` requires the exact record population, source identity set, model configurations, expected exits, JSON schemas and complete-case status. It rejects a result that changes a bound, deletes a source hash, upgrades itself to external-tool qualification, or leaves a nonempty search frontier while claiming PASS.

It always replays structural witnesses transition-by-transition and recomputes handoff, scalar-boundary and targeted histories from the supplied model. `--replay-baselines` additionally re-executes the completed BFS searches and compares their counts, visited-state digest and cycle analysis.

The verifier is not a wholly independent second formal semantics: witness replay and baseline reruns use the same reviewed transition functions. This is reproducibility and tamper rejection, not independent mathematical validation of that implementation. The explicit analytical proofs, model/source correspondence review, negative controls and optional standard-tool inputs provide separate review surfaces.

Tests deliberately rehash invalid receipt mutations. The verifier must then reject wrong interpretations, not merely detect a stale SHA-256. Manifests cannot protect against a dishonest author who rewrites code, observations and hashes together. Trust in source custody, compiler and model semantics remains explicit.

| Verification lane | Observed result | Scope |
|---|---|---|
| Exact baseline state campaign | Seven completed configurations; all frontiers exhausted | Declared model and reservation bounds only |
| Structural mutant replay | Seven witnesses replayed step by step | First violation in each modeled mutation |
| Handoff graph replay | Two positive SPSC skeletons; positive NCQ first handoff; four order-only negative families | Explicit C11-RA fragment, not a general frontend |
| Boundary and targeted replay | 577,491 scalar checks; six gate schedules; wrap identity; three stopped/stale schedules | Declared scalar and schedule abstractions |
| Formal self-tests | 34 passed | Includes hash collisions, cycles, finite deadends, states, races, bounds and witness rejection |
| Rehashed receipt negatives | 15 passed | Includes removed source identity, changed bounds, fabricated witness and Python -O |
| Release-integrity negatives | Eight passed | Canonical report, source/inventory coverage, mutation and symlink rejection |
| GCC and Clang strict C11/regression | Core 68, mutations 512, IPC 100k SPSC and NCQ4/4, adversarial eight, limits four, chaos three, parser one million | Native Linux; not model checking or Apple execution |
| Expanded Clang release-check | Exit 0; 235 Python discovery tests passed, plus native and formal Make targets | Includes retained 36 baseline binding tests and hardening; formal tests are included in discovery total |
| Three C11 baseline skeletons | Built and executed under GCC and Clang | Native baseline execution only |
| Six C11 mutant syntax/link checks | Both compilers accepted the selected mutation inputs | Deliberately racy variants not executed as native correctness tests |
| GCC ASan/UBSan skeleton lane | Three baseline executables passed with leak detection requested | C processes; no TSan or complete native-library formal claim |
| Spin / GenMC / CDSChecker | NOT_RUN | Optional source inputs / adaptation boundaries explicitly documented |

The first receipt-test harness did not include `OSError` in the expected rejection set for a deliberately missing file. The verifier already rejected the file; the test's exception expectation was corrected and the failing log retained. The first custom-`BUILD` expanded release-check also exposed an inherited build-path omission: topology tests defaulted to `build/native` even though their prerequisite was built under `build/clang`. The Makefile now forwards `ELITE_TOPOLOGY_BINARY` explicitly. All 21 affected fixture tests passed on the corrected rerun. Neither change weakens a production or model invariant. The original failure records remain archived.

## 11. Optional standard-tool models and LTL

`formal/spsc_ownership.pml` supplies capacity and finite termination LTL; `formal/ncq_sc64.pml` supplies queue-frontier and finite termination LTL with owner assertions and bounded dual-queue traffic. These are handwritten SC/control models. Promela's ordinary interleavings are not silently treated as C11 weak memory.

For an installed Spin, compile an assertion verifier separately from a liveness verifier. Do not use `-DSAFETY` and then claim acceptance-cycle checking; do not use BITSTATE/hash compaction and label the visited graph exact. Weak process fairness for the finite termination claim is an explicit selected assumption. Spin documents these controls separately. [R2]

The C11 inputs use actual `_Atomic uint64_t`, ordinary metadata/payload locations, pthread start/join, and selectable release mutations. They compile under both GCC and Clang, with runtime assertions explicitly retained. They are supplied as candidate inputs for an admitted pinned GenMC frontend, but **that tool was not available/executed here**. They are not advertised as drop-in CDSChecker programs: its documented thread/entry/instrumentation interface would require an additional adapter, which is not supplied. [R4] Their exact supported memory model, frontend transformations and bounds must be recorded in a future external run. [R3]

Attempts to obtain Spin in the local environment did not produce an executable. The supplied `.pml` files are therefore reviewable inputs, not claimed syntax-tested or completed Spin results. No third-party verification executable, vendor SDK or sanitizer runtime is redistributed in this archive.

## 12. Scope excluded from this result

The following claims are not supplied by a finite control graph or its handoff proof:

- All ISO C11 executions of the production source, all compiler transformations, all atomic-helper implementations, or a complete machine-checked refinement proof.
- The whole Turn 5 eight-reservation N4 population, all parked-waiter schedules, all admission/grant/authority transitions, or Python cyclic-GC state space. The six gate examples are expressly much smaller.
- Guaranteed completion for each caller, hardware fairness, a per-client service deadline, sub-50-ns latency, or a new Apple/AMD/Intel performance comparison.
- Autonomous restoration of orphaned tokens, volatile power-loss persistence, exactly-once external effects, MPMC parking, or reclamation of memory still reachable through an old pointer.

These limits preserve the project model. SPSC/NCQ correctness assumes conforming clients end all aliases before transfer, construct objects before exposure and do not truncate/reinitialize live backing. A stopped endpoint's token remains counted but may be unavailable. Strong-CAS progress assumes the platform supplies the admitted primitive, not just the right mnemonic.

## 13. Reproduction and release integration

Start in a clean extracted repository:

```sh
python3 tools/verify_release.py --strict
make CC=clang BUILD=build/formal check-formal check-formal-results
python3 tools/run_formal.py --out build/formal-new
python3 tools/verify_formal_results.py build/formal-new --replay-baselines
```

Use a new output directory for each campaign. GCC supplies the independent native syntax/regression lane:

```sh
make CC=gcc BUILD=build/gcc-formal check-formal check check-hooks limits chaos fuzz
```

The potentially incomplete extended search is deliberately separate:

```sh
python3 tools/run_formal.py --extended --out build/formal-extended
```

An exit code 2 from that command is not accepted merely because earlier default cases passed. Retain the extended result and resource frontier.

`SOURCE_MANIFEST.sha256` includes new Python/C/Promela code, model binding metadata and build/tool inputs. `MANIFEST.sha256` covers every shipped member except itself. `tools/verify_release.py` does not regenerate either file during verification. Generated build directories and Python caches are not shipped as claimed tested source.

The detached deposit receipt records literal report/ZIP hashes and any actually completed Drive writes. It is not embedded into the archive it hashes. A fresh extraction is used for post-package checks, whose evidence is saved outside the immutable release to avoid changing its manifest after verification.

## 14. Formal acceptance statement

**Accepted within scope:** both SPSC ownership directions; the selected SC dual-queue control invariants within the completed finite spaces; explicit token conservation and no-double-allocation; the nonwrapping arithmetic argument; reproductions of the designated ordering/structural negative controls; and conditional system-wide progress arguments supported by the completed graphs.

**Still open:** independent standard-tool C11/RC11 execution; complete planned larger-state/lifecycle/wait coverage; a machine-checked source refinement; and the separate target/deployment qualification lanes.

A future valid counterexample takes precedence over every count in this report. This delivery makes the formal evidence runnable, bounded and inspectable; it does not redefine “no counterexample found in these exact graphs” as “all deployments are proved safe forever.”

## 15. Sources and authority

[P12] Authoritative Turn 12 archive, size and hash in the header. Existing source, ABI, audit and historical evidence remain the basis. User's Apple receipt is LAB_REPORTED context; the supplied commit and raw Apple matrix were not independently obtained here.

[P2] `docs/reference/02_Turn_02_Cache_Topologies_and_Barrier_Proofs.md`, preserved with the verified original hash: §§4,6 and8 establish the two-way handoff, exact NCQ variant and wait boundary. Report SHA-256 `9bb8b971b65f7da80bd13053e8027aaa6ede5ae9ea19da8262e24282e16a6977`.

[P3] Governing Turn 3 concurrency audit: SHA-256 `7ad5fbd43134e7d3419c5a1f76d030e25d051510dbc6f20ce173b3d0eccff605`; progress, finite-identity and crash limitations remain unchanged.

[P4] Frozen Turn 4 ABI: SHA-256 `6aaa3d6706c29096cfab56ea1fe569afa561c4cba7caea26ebd3c09cc9812386`; particularly §§8–9,13. The native source, not names in a summary, determines the implemented operation ordering.

[P5] Frozen Turn 5 verification plan: SHA-256 `bc7ae2702cecb8d46820cf5365046fe40dda36c59dde9c80fda98ff7833ecdf8`; §§1,7–8 distinguish V1 from native/sanitizer/latency evidence and require cutoff disclosure.

[R1] GCC 14.1 official atomic built-ins documentation, consulted 24 September 2026. Supports distinct memory-order parameters, indivisible CAS, strong/weak failure behavior and the expected-value update rule. This is compiler contract context, not an automatically verified OS shared-memory ABI.
https://gcc.gnu.org/onlinedocs/gcc-14.1.0/gcc/_005f_005fatomic-Builtins.html

[R2] Spin, official Pan verification options. Supports separate safety/liveness modes, weak fairness selection, maximum-depth handling, no-reduction and bitstate distinctions. No Spin execution is claimed here.
https://spinroot.com/spin/Man/Pan.html

[R3] GenMC author-maintained project and research site, consulted for explicit RC11/IMM/LKMM and frontend/tool boundaries. No locally installed revision or completed run is inferred from the documentation.
https://github.com/MPI-SWS/genmc
https://plv.mpi-sws.org/genmc/

[R4] CDSChecker author-maintained repository, consulted for the required `user_main`, C11 thread and race-instrumentation interfaces. No execution or compatibility of the pthread skeletons is asserted.
https://github.com/computersforpeace/model-checker

The equations, models, tests and measurements of explored-state counts are this project's work. External sources supply semantic/tool contracts; they do not certify these new models or their source correspondence.
