# Turn 03 — Concurrency Kill Gate and Failure Modes
## Zero-Copy Lock-Free RingBuffer IPC / NCQ-SC64

**Project:** Leonid Majbits / Gemini Operator Lab, paired with Leon  
**Date:** 2026-09-23  
**Stage:** 3 of 5 — architecture and proof gate  
**Status:** Adversarial paper audit. No implementation, executable model, compiler probe, fault-injection run, or latency measurement has been produced.  
**Authoritative predecessor:** `02_Turn_02_Cache_Topologies_and_Barrier_Proofs.md`, 91,463 bytes, SHA-256 `9bb8b971b65f7da80bd13053e8027aaa6ede5ae9ea19da8262e24282e16a6977`. The mounted predecessor matches this digest.  
**Destination:** Deployment Research / Elite_Systems_LockFree_RingBuffer  
**Evidence convention:** [P2] means the audited predecessor; [P1] means the original Turn 1 packet. [R01]–[R16] resolve in the source ledger. “Derivation,” “counterexample,” and “policy” identify this turn's analysis, not a source's assertion. Numerical acceptance thresholds below are proposed engineering policy, not measurements or architectural timing guarantees.

---

## 0. Gate verdict

**Keep NCQ-SC64 as a conditional, publish-first correctness reference. Reject the current composition as a self-recovering, leak-free, bounded-memory transport under arbitrary process failure.** These are different verdicts on different properties, not a contradiction.

The four requested attacks produce the following results:

| Attack | Result for the specified reference | Mandatory consequence |
|---|---|---|
| Producer dies while holding a block | No unpublished FIFO reservation hole; a block can nevertheless become an orphan | No autonomous in-place reclamation claim; retire/rebuild or introduce a separately proved recoverable ownership protocol |
| Atomic crosses a line/page or is silently widened | Outside the admitted ABI; not a legal execution of the reference | Reject before atomic access; do not use padding as a substitute for atomicity |
| Low-QoS producer or borrower is starved | A published descriptor remains helpable; missing work and held capacity do not become available by scheduling a reader | No hard deadline or individual fairness claim; qualify the deployment and idle policy |
| CAS contention or exclusive-store failure | Strong CAS excludes a returned spurious mismatch, but individual retry count and elapsed latency are unbounded | Retain the primitive progress assumption, audit lowering, and apply empirical tail/throughput gates |

Two decisive limits are now explicit:

**Recovery impossibility threshold:** one ownership transfer whose owner/outcome cannot be distinguished safely is enough to reject *guaranteed autonomous same-generation recovery of every token*. This does not mean one crash blocks every consumer or exhausts a 1,024-block pool.

**Proposed latency cliff:** for the nominated native 64-byte, warmed, sub-saturation experiment, reject the NCQ deployment profile when end-to-end **p99.9 exceeds 1,000 ns** reproducibly under the rule in §9. A saturation experiment has separate throughput and contention criteria. The original sub-50-ns uncontended aspiration remains separate and unmeasured.

The adopted fallback direction is SPSC fault-isolation lanes when their ordering and routing semantics fit the application. They are not relabeled as a globally ordered MPMC FIFO, and they cannot guarantee nanosecond scheduling or recover volatile memory after a host power failure.

### 0.1 What this turn changes

This audit does not silently revise Turn 2's queue operations. It tightens four surrounding contracts: process identity and mapping inheritance; retirement versus completed quiescence; resource leakage versus queue progress; and measurable rather than imagined timing limits. It also identifies an exact implementation trap involving the compare/exchange `expected` argument, and specifies the forbidden post-publication accesses that a future implementation must exclude.

The user's acceptance summary overstates one predecessor result: XNU's 64-byte constants coexisting with a 128-byte system query do **not** prove a universal cross-cluster coherence geometry or that 128 is a uniquely required hardware size. [P2, §2] established conditional isolation for admitted granularities dividing the chosen alignment. That is the theorem retained here.

---

## 1. Subject of the audit and assumptions

### 1.1 The exact subject

NCQ-SC64 is the Turn 2 reference refinement of the publish-first NCQ scheme, not an arbitrary sequence-number ring. It has a fixed pool of N payload blocks and two internal queues: **QF**, containing available block identifiers, and **QR**, containing completed-message identifiers. Each internal queue entry is one admitted atomic 64-bit cycle/index word. Head, tail, and entry operations are sequentially consistent. Compare/exchange is strong in the reference proof. There is no live counter wrap, no in-place reset, and no 128-bit atomic requirement. [P2, §6.3]

The pool token and the queue ticket are different identities. A producer can hold a payload block while constructing it without owning an indispensable QR publication position. Enqueue linearizes when the complete cycle/index word is installed. Tail advancement afterward can be helped. Dequeue linearizes when a consumer wins the head increment; only that winner may inspect the payload. [P2, §§6.4–6.8]

The four block phases—EMPTY, RESERVED, COMMITTED, CONSUMED—do not uniquely identify queue membership. In particular, COMMITTED can precede QR insertion, and EMPTY can precede QF return. Neither is an independent recovery oracle. [P2, §§6.9–6.12]

### 1.2 Admitted execution model

The proof covers nonmalicious participants on one live host, using coherent, normal cacheable CPU mappings of the same backing object, with a compatible address-free atomic ABI. All construction precedes exposure. Extents and alignments are validated. No participant writes through a released borrow. Registered processes may stop between operations, including indefinitely. A hardware primitive is assumed to supply the progress required by the abstract operation it implements. The last assumption is not proved by an alignment assertion or the word “atomic.” [P2, §1; R01, R02]

A process may contain several endpoints. **The outstanding-lease bound is per endpoint, not per PID.** Let K be the total admitted endpoint count, and let endpoint i hold at most L_i tokens across its active calls, transfers, write leases, and read borrows. A reentrant callback must not create a second hidden operation outside that bound. Failed endpoints are not silently replaced forever while their resource losses go unaccounted.

A SIGSEGV that terminates an otherwise conforming process is a stop-failure scenario. A bug that has already overwritten other participants' control words is arbitrary memory corruption and lies outside that scenario. The signal name alone does not distinguish the two. An observed integrity violation stops admission; it does not authorize best-effort repair of untrusted cursors.

A complete host power loss is not merely a stopped producer. The protocol has no durable state or recovery transaction. Hardware persistence, storage logging, and exactly-once external effects were expressly outside Turn 2's model. [P2, §§1.2, 6.12]

### 1.3 Five different meanings of progress

| Property | Meaning here | What it does not imply |
|---|---|---|
| Atomic safety | An admitted atomic object is not observed as a torn intermediate update | Fair memory arbitration or persistent storage |
| Queue lock-freedom | Under the primitive model, system-wide queue operations keep completing despite stopped peers | Every caller completes, or useful messages always exist |
| Resource availability | Sufficient payload tokens remain available for the workload | A ready queue has data just because it has capacity |
| Recovery liveness | A surviving lifecycle actor can restore an admitted service configuration | That recovery is kernel-free, wait-free, or daemon-free coordination with no actor |
| Deadline compliance | The deployment meets a specified distribution of elapsed times | A proof from source-level memory ordering alone |

Returning EMPTY forever on an empty queue is compatible with a correct nonblocking try API. Returning NO_CAPACITY forever after every block is stranded is not useful application throughput. Reporting that distinction is mandatory.

---

## 2. Retained core invariants and the adversarial boundary

For either internal queue, let H be the removal frontier, T the published-tail hint, and U a proof-only frontier one beyond the last contiguously installed entry. Turn 2 establishes:

\[
H\le U,\qquad 0\le U-H\le N,\qquad U-1\le T\le U.
\]

Tail does not get ahead of an unfinished payload. It can trail one already installed entry. A consumer can briefly have H=T+1 after consuming that installed entry before tail bookkeeping finishes. Any later implementation that asserts H≤T unconditionally changes the reference and can misclassify this legal state.

**Publication witness.** If an enqueue's entry CAS has not succeeded, that producer has not published its block. If it has succeeded, the entry contains the whole identifier and generation needed for consumption. No later producer must finish the stopped producer's payload. A helper modifies only queue metadata, never another owner's payload or block-phase word.

**Reuse witness.** An internal enqueuer holds a unique pool token outside the destination queue. The destination cannot contain all N distinct tokens while that caller also owns one outside it. Thus the physical queue entry being replaced has already been claimed. This argument is invalid as soon as recovery duplicates a token or a caller returns it twice.

**Borrow witness.** A saved index is not ownership. A failed head CAS discards that saved index. The winning consumer's required payload reads are sequenced after the acquire observation and successful claim. The last such read precedes the release path through QF. The two directions are:

\[
W_{b,g}\to_{hb}R_{b,g},\qquad R_{b,g,\mathrm{last}}\to_{hb}W_{b,g+1}.
\]

These are inherited protocol proofs, not hardware test results. This audit found no counterexample to these invariants within their stated nonwrapping, single-word, SC, unique-token model. The failures below either expose missing surrounding guarantees or kill proposed modifications that would violate the model.

---

## 3. Failure Mode 1 — Process death and abandoned leases

### 3.1 Crash cut-point ledger

A kill can occur on either side of every listed atomic transition. “Atomic” means a transition has one outcome in the admitted model; it does not mean the caller necessarily receives that outcome before dying.

| Cut point | Shared-state consequence | Healthy queue behavior | Recovery hazard |
|---|---|---|---|
| Before winning QF head CAS | No free-token ownership acquired | QF/QR unchanged by this attempt | No token to recover from this attempt |
| After QF head CAS, before owner/RESERVED record | Token removed; descriptor can still look EMPTY | QR is not blocked by this unpublished work | Owner-record gap; absent token not safely attributable from phase |
| During RESERVED payload construction | Partial bytes in a privately owned block | Other QR entries remain consumable | Bytes cannot be exposed; token can be orphaned |
| After COMMITTED, before QR entry CAS | Payload frozen but not necessarily enqueued | No QR reservation hole | COMMITTED alone does not authorize replay or reclamation |
| After QR entry CAS, before tail assistance | Message is already published; T may lag one | Consumers can claim it; producers can help T | Never treat missing function return as nonpublication |
| After QR insertion, before caller returns | Message may already have been consumed and reused | Ordinary progress continues | A retried send may duplicate delivery; producer must not touch descriptor |
| After QR head CAS, before CONSUMED record | Consumer owns token; old phase may remain COMMITTED | Other queue tickets remain claimable | Symmetric ownership gap |
| During consumer borrow | Payload still retained | Other blocks can cycle | Reclaiming while an alias lives violates the reverse ownership edge |
| After external effect, before QF return | Effect may exist; token remains outside free queue | Queue cannot roll back external world | Redelivery versus loss is ambiguous without application protocol |
| After EMPTY, before QF entry CAS | No borrow should remain, but token is not yet free-queued | Other free tokens remain usable | EMPTY is not evidence of completed return |
| After QF insertion, before release function returns | Token can already be reallocated | New owner may write immediately | Old release path cannot touch that block again |
| After publication, before notification/wake | Predicate can be true while a waiter remains asleep | Polling reader can observe it | Finite wait/recheck or lifecycle notification remains required |

This is the first direct answer to “how does it avoid leaking slots?”: **the present schema does not guarantee that it does.** It prevents one important class of FIFO blocking while admitting orphaned storage after a crash. That limit was already explicit in Turn 2 and must not be erased by the phrase “lock-free recovery.” [P2, §§6.9–6.12]

### 3.2 Counterexample C1 — Atomic claim, nonatomic attribution

Take two free identifiers x and y, and two registered producers A and B. Both descriptors initially appear EMPTY. Consider two executions:

**Execution E1:** A wins the QF removal of x and dies before recording ownership. B wins the removal of y and is paused before recording ownership.

**Execution E2:** B wins x and pauses at the same boundary. A wins y and dies at the same boundary.

The shared QF head advances by two in both. Its historical entry words are unchanged by these removals. The block phase/owner records remain unchanged. The recovery actor knows A is dead and B is alive but cannot read B's unrecorded local result through this protocol. Its available shared observations are identical.

Recovering x is unsafe in E2; recovering y is unsafe in E1. Recovering both steals the live B token in either execution. Recovering neither preserves safety but fails a requirement to recover A's token within a fixed deadline while B can remain paused.

**Conclusion:** the existing state cannot support wait-free or bounded autonomous reclamation of the unknown dead owner's block while arbitrary live claimants remain in the attribution gap. Waiting for B to cooperate, globally quiescing participants, or adding a recoverable operation record changes the available information and can resolve the gap. A best-effort PID store after the claim does not.

This is a small indistinguishability argument about this schema, not a universal impossibility result for every recoverable queue.

### 3.3 Counterexample C2 — A timeout is not a fence

A owns a RESERVED block and a writable raw pointer. The recovery actor sees no progress for Δ. There are two indistinguishable histories up to that observation: A is dead, or A is alive but delayed.

Suppose the actor changes the generation and gives the same bytes to B by time Δ without establishing that A can no longer access them. In the delayed history, A resumes and executes an ordinary payload store through its old pointer while B owns those bytes. Rejecting A's later commit does not undo that store. The ownership invariant is already violated.

Consequently, for any finite Δ, the following combination is impossible under the present assumptions:

\[
\text{arbitrary pause/resume}
+\text{unrevoked writable pointer}
+\text{mandatory reuse of the same bytes by }\Delta
+\text{zero corruption}.
\]

Safe choices are to retain the bytes, establish effective fencing/quiescence, or move the service to physically distinct backing while retaining the old generation. A larger generation field, checksum, hazard-pointer name, or extra memory fence cannot revoke the write capability in this history.

### 3.4 Counterexample C3 — Phase scanning duplicates ownership

A marks b COMMITTED and is delayed around QR publication. A recovery scanner observes COMMITTED and inserts b into QR “to finish the send.” If A's original insertion has already linearized, the scanner has duplicated the same token. The two queue entries can then give two consumers the same payload. Even without a simultaneous consumer, duplicate return through QF invalidates the token-admission theorem and can eventually overwrite a live queue member.

Changing COMMITTED to a recovery-specific state requires an atomic relation to the insertion outcome, not just another phase bit written in a separate location. Likewise, a scanner must not read ordinary, mutable descriptor fields or payload bytes without ownership or a separately proved stable snapshot. “Only diagnostics” does not exempt a racy read.

### 3.5 Death verification is wider than one PID

SIGKILL does not run cleanup handlers. Sending a signal and observing final process exit are different events. On Linux, establish the process handle as part of attachment before failure, and use a process-level pidfd/owned-child lifecycle protocol rather than opening a potentially recycled numerical PID afterward. A thread-only death indication is insufficient to fence every thread sharing that process's mappings. [R03, R04]

There is a second identity boundary: mappings may be inherited by a child. A dead parent does not prove an inherited writer is dead. The admission policy must prohibit unregistered descendants and unregistered transfers of mappings/leases, or account for and fence them all. Linux's documented mapping inheritance and noninheritance options make this a real lifecycle obligation, not an abstract exotic case. [R05]

Closing a shared-memory file descriptor or unlinking its name does not revoke mappings already held by another process. Recreating the same name can create a distinct object, but that is not evidence that an old mapping disappeared. [R06]

Darwin requires the corresponding supported process-lifecycle registration and ownership policy. An existing launcher that owns its child processes is a suitable place to provide it. This report does not pretend that a stale PID plus a failed heartbeat is a Darwin capability-revocation API. Exact adapter details remain a Turn 4 requirement.

### 3.6 Capacity loss is quantitative even when safe reclamation is unavailable

Let X be the number of distinct tokens retained by failed/uncertain owners and unavailable for useful service. Let B_live be the remaining non-free tokens: healthy write/read leases, transfer-owned tokens, and ready messages. Then:

\[
N_{\mathrm{effective}}=N-X,\qquad |F|=N-X-B_{\mathrm{live}}.
\]

These equations are proof-state accounting; asynchronously sampling separate counters does not automatically produce an exact runtime value of X or B_live.

With per-endpoint outstanding bound L_i and a known set D of failed endpoints, an upper bound on newly orphaned tokens is:

\[
\Delta X\le\min\left(N-X,\sum_{i\in D}L_i\right).
\]

This bound includes invisible transfers and aliases. With equal L, an adversary can exhaust the pool after as few as ceil(N/L) suitably placed endpoint crashes, provided replacements are repeatedly admitted into that same generation without recovering prior losses. Already published messages are not automatically orphaned just because their originating producer died.

If the required remaining service capacity is N_min, the capacity contract is lost when:

\[
X>N-N_{\min}.
\]

For N=1024 and L=1, one lost token leaves 1023 potentially usable tokens. After 1024 such losses, QF and QR can both be empty forever. This is not evidence that the queue head is blocked by an unpublished ticket. It is total resource exhaustion.

**Policy:** a confirmed failure with an unresolved transfer triggers retirement of the generation rather than indefinite replacement and silent capacity erosion. The threshold for rejecting a *zero-leak guarantee* is one unresolved transfer, not 1024 failures.

### 3.7 Can recovery occur without an auxiliary supervisor process?

**Yes, coordination can run in an existing application process or launcher. No, the current data-plane algorithm does not thereby acquire lock-free recovery.** A surviving peer can perform setup and failover; a dedicated daemon is not technically necessary. The recovery operation still needs an authority, lifecycle evidence, mapping management, and a bounded resource policy.

A different design can support detectable recovery using operation descriptors and additional state. Published research explicitly studies such techniques, including persistent-memory settings. That demonstrates why the rejection above is schema-specific; it does not certify a drop-in extension for this volatile IPC pool. [R07]

The admitted options are: service restart/new generation with explicit uncertainty; coordinated quiescence and reconstruction; or a new recoverable-transfer algorithm subjected to its own proof gate. Only the first two are conservative directions in this campaign. A bare phase scanner or lease-timeout thief is rejected.

---

## 4. Recovery hardening — distinguish retirement, quiescence, and reuse

### 4.1 Counterexample C4 — CLOSED does not seal an in-flight operation

A producer validates RUNNING and is paused immediately before its valid QR publication CAS. A lifecycle actor writes CLOSED and exposes a successor segment. The producer resumes and publishes to the old segment.

Nothing in a separate lifecycle store invalidates that already prepared CAS. Even checking the lifecycle again immediately before the CAS leaves a race between the check and CAS. Therefore a simple CLOSED flag cannot mean “no further old-generation message can linearize.”

**Amendment:** call the first transition RETIRE_REQUESTED. It prevents cooperating endpoints from beginning new work after observing it; it does not prove all older work has stopped. SEALED means a separately established absence of operations and aliases that could still affect that generation. A combined admission protocol could establish a stronger cutover, but it is not silently supplied by the current cursors.

### 4.2 Conservative lifecycle states

| State | Authority and meaning | Permitted consequence |
|---|---|---|
| RUNNING | Current admitted generation | Normal data-plane calls |
| RETIRE_REQUESTED | Existing lifecycle authority announces withdrawal | No new work after observation; existing calls accounted for |
| DRAINING / QUIESCING | Cooperative endpoints end calls and borrows, then acknowledge | May reconcile already published work under the chosen delivery policy |
| QUARANTINED | Some old access or ownership outcome remains uncertain | Old mapping retained; no reuse, no new admission |
| STAGED | One new object is being initialized and validated | Not yet distributed as current |
| READY_SUCCESSOR | New object completely initialized | Distribution to healthy participants allowed |
| SEALED_OLD | All old accesses ended or were effectively fenced | Stable inspection or reclamation may proceed |
| RECLAIMED | No old mapped/reference capability can be used by participants | Release old object resources |

These are lifecycle predicates and actions, not a claim that writing their names atomically supplies the predicates. The authority must be able to justify SEALED_OLD from endpoint acknowledgements and process/mapping lifecycle evidence. Acknowledgement covers active native calls, delayed queue helpers, write aliases, read aliases, and exported Python views—not only the caller's last message.

### 4.3 Safety proof for a distinct successor

Let E_old and E_new denote mappings of genuinely different backing objects. Require that no address/offset interpretation from an old handle is redirected into the new object, and that old participants are not issued a new writable capability as a side effect of an old operation.

Then an old valid access addresses E_old, while every new-generation token belongs to E_new. Retaining E_old until safe release prevents virtual-address reuse from turning a stale local pointer into access to unrelated new storage. No old operation can corrupt a new token through the normal old mapping. This isolates memory safety; it does not identify whether an old message was delivered.

If old and new work overlap, report a session discontinuity. There is no global FIFO or exactly-once theorem across that cutover. Late old publications and uncertain external effects must be classified explicitly as old-generation uncertainty, not silently merged into the successor's success stream.

### 4.4 Failure of the lifecycle actor

Choose one existing lifecycle authority in the initial recovery profile. Allow at most one staged successor and serialize publication of the successor identity through that authority. No dedicated external process is required, but a stopped authority can delay recovery. Takeover requires confirmed authority failure/fencing and a separately specified ownership handoff; expiration of an unfenced authority lease is not sufficient.

Before a successor is published, an abandoned staged object can be discarded after its creator is fenced. After publication, a replacement authority must rediscover the published generation and not create another competing successor. If authority identity or the published outcome cannot be established, stop recovery rather than guess. This is intentionally **not** advertised as a lock-free distributed management protocol.

A process-local creation quota is insufficient if repeated authority failures forget staged objects. The deployment must retain a bounded namespace/manifest of active, staged, and quarantined generations through authority handoff, or stop on that handoff. Turn 4 must specify this control record and its registration-before-exposure discipline. This remains a release blocker, not an implementation detail to improvise later.

### 4.5 A bounded quarantine policy

Propose at most **two quarantined generations**, one active generation, and one staged generation. Count an abandoned staged object against the same resource budget until safely disposed. If M_max is the admitted maximum object size, data-object reservations are bounded by:

\[
M_{\mathrm{objects}}\le 4M_{\max}.
\]

For Turn 2's strict Apple worked profile, M=557,056 bytes, the four-object envelope is 2,228,224 bytes = 2,176 KiB = 2.125 MiB. For its strict x86 example, M=270,336 bytes, it is 1,081,344 bytes = 1,056 KiB. These are mapping-size arithmetic, not full RSS or kernel-memory measurements; control records and OS overhead need separate caps.

If two quarantined generations cannot be reclaimed and another active generation must be retired, do not publish a successor that would leave three unbounded quarantines. Halt new admission or obtain effective fencing. No finite-memory system can promise unlimited replacements while indefinitely preserving every still-accessible old allocation.

**Result:** safety can be bounded; availability under unlimited unfenceable failures cannot. SPSC lanes reduce the affected region but do not repeal this resource limit.

---

## 5. Failure Mode 2 — Split-line tearing and asymmetric clusters

### 5.1 Three questions that must not be collapsed

**Atomicity:** can an observer see a torn intermediate value of one synchronization object?

**False sharing:** do logically independent objects occupy the same relevant coherence line?

**Compound consistency:** can individually valid observations of several objects describe different logical versions?

An answer to one is not an answer to the others. A 128-byte-aligned structure can contain two separately atomic words that do not form an atomic pair. A naturally aligned eight-byte word can be atomic while neighboring independent words cause false sharing. A multi-line ordinary payload can be safe because exclusive ownership, not a giant atomic load, protects it.

The admitted ABI contains naturally aligned four-byte wait words and eight-byte cursors/entries/state words. A proposed atomic aggregate larger than those widths is a new primitive requiring its own admission and proof. There is no automatic promotion to 128-bit atomics because the profile uses 128-byte cells. [P2, §§1, 2, 6.3]

### 5.2 Exact no-split placement proof

Let w∈{4,8} be an admitted object's actual access width. Let a be its physical starting address. Assume a is naturally aligned, and every relevant line size g under this profile is divisible by w. For the admitted 64- and 128-byte cases:

\[
a\equiv0\pmod w,\qquad w\mid g.
\]

Then its residue is one of:

\[
a\bmod g\in\{0,w,2w,\ldots,g-w\}.
\]

Therefore:

\[
(a\bmod g)+w\le g.
\]

The half-open byte range [a,a+w) cannot straddle a g-byte boundary. The same reasoning applies to a page size V divisible by w. Mapping at an admitted page-aligned base preserves the required within-page physical alignment.

**Important:** natural eight-byte alignment already excludes crossing a 64- or 128-byte boundary for an eight-byte object. The stronger A=128 cell policy separates independent objects; it is not the fundamental reason an eight-byte operation is indivisible.

For isolation, let every admitted coherence granularity divide A and give independent hot objects disjoint complete A-cells. Every cell boundary is also a coherence-line boundary, so their line sets are disjoint. This retains Turn 2's conditional theorem without turning software constants into hardware measurements.

Migration changes which core executes the operation, not its physical byte range. If every eligible core admits the same width/alignment semantics and all relevant granularities divide A, the placement proof continues to hold on either core. If that compatibility is not established for a target, admission fails; a thread's current P-core placement is not a permanent substitute for a migration-safe ABI.

### 5.3 What actually happens for an invalid atomic?

The language/ABI contract is violated before there is a portable answer. Different instructions and feature sets can reject misalignment, support only particular nonaligned accesses, or perform an expensive compound hardware transaction. This turn does not invent one deterministic outcome for every M-series instruction and generation. Target verification must inspect the selected primitive, not infer support from an ordinary unaligned load that happened to work.

The x86 case illustrates why “a split atomic necessarily tears” is wrong. Intel-authored Linux documentation explains that a split locked operation can preserve atomicity through bus locking; supported detection policies may instead trap, throttle, or terminate the task. That is unacceptable latency and system interference, even when no torn value is produced. [R09]

For ARM64, the deployment contract remains the conservative naturally aligned single-word atomic mapping, with the actual compiler's atomic/exclusive sequence audited later. Legal acquire/release instruction choices are not a license to operate on a misaligned C atomic object. Documentation access in this turn did not establish a complete target-specific table for unsupported M-series accesses; those accesses are rejected, not guessed safe. [P2, §5; R01, R08]

### 5.4 Counterexample C5 — An aligned pair is still two observations

Suppose generation and block index are stored as two independent words in a perfectly aligned cell. A writer updates the index and then the generation; a reader obtains one field before and one field after that transition. Without an ownership/order protocol preventing overlap, the reader can assemble a generation/index pair that no single atomic publication installed.

NCQ-SC64 avoids this particular problem by packing the entire queue-entry identity into one admitted 64-bit atomic word. If the desired index range no longer fits, the design must change; widening the entry without changing the primitive proof is forbidden.

Block length, message identity, and payload bytes are different. They are accessed only under their token's ownership, with the two happens-before edges through QR and QF. They do not require whole-structure atomicity. Conversely, reading them concurrently with possible reuse and validating a generation afterward is not a replacement for ownership.

### 5.5 Admission is before access, not after a failed experiment

Turn 4 must require the following checks before operations on the proposed shared controls:

| Check | Rejection condition |
|---|---|
| Bootstrap | Initializer incomplete, metadata still mutable, or incompatible backing/lifecycle |
| Bounds | Region, count×stride, offset+width, or rounding exceeds the mapped extent or arithmetic domain |
| Natural width/alignment | Any four/eight-byte control violates the selected primitive contract |
| Profile isolation | A claimed independent cell overlaps another at an admitted granularity |
| Array stride | First element is aligned but later entries lose alignment/isolation |
| Atomic representation | Foreign layout, unsupported width, mixed atomic/non-atomic access, or per-process lock fallback |
| Mapping lifetime | Another actor can truncate/reinitialize the object or redirect a still-live local pointer |

Reading arbitrary header bytes safely also needs the existing initialization/exposure contract. An attacker or buggy peer racing the immutable header cannot be made safe by validating it once and then trusting different bytes. Malicious shared writers are not isolated by this in-process-style ABI.

**Kill threshold:** one legal-path torn atomic, one admitted malformed placement, or one unauthorized mixed-generation payload access kills that implementation/profile immediately. The remedy is not to average the event into a low error rate.

---

## 6. Failure Mode 3 — Darwin QoS, inversion, and starvation

### 6.1 What publish-first fixes

If a background producer stops before QR entry installation, its work is not ready. A high-priority consumer can consume other completed entries and may truthfully observe EMPTY when none exists. It does not wait for that producer to fill a preclaimed FIFO head.

If the producer stops after entry installation, the descriptor is already available. Consumers need not wait for the tail hint, and a later enqueuer can advance a lagging tail. If a low-priority consumer has already won its claim, it retains a block rather than an unclaimed QR position. These are algorithmic properties of the reference, not scheduler promises.

### 6.2 What publish-first cannot fix

A consumer requiring the *result* of an unscheduled producer still depends on that producer. If low-priority borrowers retain every block, new producers have no capacity. If high-priority polling work consumes available CPU budget, it can aggravate the dependency. If a particular contender repeatedly loses a CAS, QoS alone does not supply an individual queue fairness theorem.

Apple's QoS guidance describes promotion for particular synchronous dispatch and mutex dependencies. It does not imply that the kernel recognizes arbitrary raw-memory producer/consumer edges. More directly, the public address-wait interface expressly disclaims priority-inversion avoidance. Do not import mutex donation into a ring by analogy. [R10, R11]

### 6.3 Timing impossibility under arbitrary descheduling

Let a required participant be delayed by D after the last prerequisite event but before its required action. If the model permits any D, then for any finite target X choose D>X. The operation misses X regardless of how few instructions the queue requires.

Thus:

\[
\sup T_{\mathrm{wall}}=\infty
\]

in the arbitrary-scheduling model, even for an otherwise bounded-step SPSC operation. This is a direct consequence of the execution model, not a claim that every practical workload experiences indefinite starvation.

A bounded spin phase limits its own busy work. It does not prove that the needed producer will run next. A processor spin hint is not an OS scheduling guarantee; sleeping or yielding also does not establish a fixed resumption time.

### 6.4 Required scheduling policy

Align producer, consumer, and control-plane QoS with the actual end-to-end dependency. A critical producer should not be designated background merely because it lacks a UI. Conversely, raising every worker to user-interactive can worsen competition and is not a universal solution. Record the actual QoS configuration rather than relying on queue names. [R10]

Specify bounded idle polling for application-facing profiles, followed by the admitted waiting policy. PARKABLE_SPSC has a single-waiter proof; MPMC kernel parking does not inherit it. Until a multi-waiter protocol is independently approved, use the polling reference in its declared measurements and expose application-managed timed rechecks as a different service mode, with their added delay. Do not add a wait-on-head shortcut and call it proved.

Separate ready-work delay from no-input time. A high-priority consumer returning EMPTY while no producer has committed a message is not an algorithmic starvation counterexample. A ready descriptor left undelivered while a scheduled, correctly admitted reader keeps taking valid steps would be.

For fault/scheduling experiments, include same-QoS and inverted-QoS cases, runnable oversubscription, low-priority held read leases, a producer delayed before publication, and a publisher delayed after publication. Save scheduling evidence where available. P/E placement on macOS may be requested or observed rather than strictly pinned; do not relabel a QoS request as verified core affinity.

### 6.5 A service-level starvation tripwire

Propose a **100-ms per-request no-completion tripwire** for a continuously requesting endpoint while peers complete at least 100,000 comparable operations and the trace establishes that resources were available. This is a coarse service-health policy, not a claimed upper bound and not a replacement for the 1,000-ns tail gate.

If the affected thread did not run, classify the failure as scheduling/deployment. If it ran but repeatedly lost while peers progressed, classify it as individual unfairness. Both can make the deployment unacceptable for a per-client service contract; neither by itself disproves global lock-freedom. Any timeout remains a pending/failed request in the latency ledger rather than disappearing from the successful-sample histogram.

**Immediate logical kill:** a requirement for a finite worst-case deadline under arbitrary descheduling is incompatible with this platform model. SPSC reduces synchronization work but does not repair that contradiction.

---

## 7. Failure Mode 4 — CAS failure, contention, and livelock

### 7.1 Strong versus weak is not cosmetic

The admitted queue uses strong compare/exchange. For its exact integral representation, a returned strong-CAS failure means the compared value differed; it is not a permitted caller-visible spurious failure. Weak compare/exchange may fail with the expected value unchanged. Compiler documentation and the language operation contract explicitly distinguish them. [R01, R12]

A strong CAS implemented with an exclusive-load/store retry sequence can internally retry because an exclusive store failed. That internal event is not automatically a returned strong-CAS failure, and it need not indicate that another *queue operation* completed. The implementation must preserve the source contract.

The Linux atomic-primitives documentation warns that abstract progress expectations do not automatically survive a compare/exchange implementation using LL/SC and compiler-generated loops. This is supporting evidence for auditing the primitive; Linux kernel APIs themselves are not substituted for C11 IPC semantics. [R02]

Accordingly, the future assembly audit must distinguish native LSE CAS, LL/SC retry sequences, outlined helpers, and any unsupported lock-based fallback. A single LSE instruction is not a constant-wall-time operation. Cache isolation must not be used to claim that exclusive-monitor interference or scheduling interruption is impossible.

### 7.2 Counterexample C6 — The updated expected argument creates a false success

A common future implementation error would defeat the proof even with strong CAS and perfect alignment.

Consumer A reads H=h and the entry containing x. Consumer B successfully advances H to h+1 and owns x. A's CAS from h to h+1 fails. The compare/exchange interface updates A's expected value to h+1.

If A blindly repeats the same desired value h+1 and keeps its old saved x, its next CAS can succeed as a same-value update from h+1 to h+1. A can then falsely declare itself owner of x even though it removed no ticket and B already owns it.

**Required invariant:** every head-CAS attempt is tied to the exact saved ticket and entry observed for that attempt; desired is that saved ticket plus one. A failure invalidates the saved entry and all derived desired values. Retry re-reads and re-derives them. The same discipline applies to tail assistance and entry-generation comparisons.

This counterexample kills a malformed retry wrapper, not the Turn 2 protocol, which already requires discarding the losing observation. It is important because “every successful CAS advances progress” is false for a wrapper that accidentally permits same-value successes.

### 7.3 Individual retry latency has no K-only bound

In the unbounded-ticket abstract model, consider A and B repeatedly competing. Before every A CAS, schedule B to publish the next eligible entry or claim the next head. A's saved expectation is stale and it loses. B can complete arbitrarily many operations while A remains pending. The queue is making progress, but A has no individual bound based only on the fact that two participants exist.

For sixteen contenders, it is therefore wrong to assume “at most fifteen failures” or independent success probability 1/16. Arrivals, cache ownership, scheduling, and repeated winners can be correlated. A geometric-tail estimate would require additional probabilistic assumptions that this protocol does not provide.

With fixed-width, nonwrapping counters, a generation ceiling ultimately limits the number of distinct logical advances before retirement. That does not guarantee A's successful completion before retirement and does not bound its elapsed time. Even one primitive or one scheduler gap can have an unbounded duration in the admitted abstract timing model. No useful worst-case nanosecond CAS-retry bound is derived here.

A useful decomposition is:

\[
T_i=T_{\mathrm{off\ CPU}}+T_{\mathrm{faults}}+T_{\mathrm{payload}}
+\sum_{j=1}^{A_i}T_{\mathrm{atomic},j}+T_{\mathrm{backoff}}+T_{\mathrm{other}}.
\]

The memory-order proof does not assign finite maxima to these terms. A finite retry budget, if added, changes the API: it may return CONTENDED/RETRY while retaining any already acquired token. It must never return ordinary FULL or EMPTY merely because contention exhausted a budget, nor abandon a token without a specified continuation/abort path.

### 7.4 Global progress and true livelock

Under the strong primitive model and no repeated identity, each failed publication/head CAS in the proper loop witnesses a conflicting change. Successful installations and claims have finite completion tails or helpable bookkeeping. A finite set of stopped participants cannot leave an unpublished ready-queue reservation that only one of them can finish. This preserves the core global progress argument. [P2, §6.8]

A trace with many failures while other messages complete is not global livelock. A trace with infinitely many valid active steps, no completed queue operation, no legitimate empty/capacity termination, no retirement, and no violated primitive assumption would contradict the admitted lock-free proof and kill the implementation or the proof. A finite long stall can expose a practical failure, but cannot by itself establish an infinite-execution theorem.

Replacing strong CAS by weak CAS and counting every returned failure as evidence of another participant's progress invalidates the argument. A weak-CAS variant could be proved under suitable primitive progress conditions, but it is a different admitted variant. The language's recommended practice about avoiding persistent spurious failure is not a measured fairness bound. [R12]

### 7.5 Where coherence thrashing comes from

A pool-token lifecycle traverses QF removal, QR insertion, QR removal, and QF return. In the uncontended reference trace these involve six RMW attempts, plus loads and phase work. Under contention, producers compete for QF head and QR publication/tail positions; consumers compete for QR head and QF publication/tail positions. Helpers add accesses to those same controls. [P2, §10.2]

Padding separates different controls. It cannot stop contenders from requiring exclusive access to the same logical control or publication entry. More contenders can increase failed attempts and ownership transfers without increasing useful messages. In this context “bus thrashing” usually means coherence/interconnect contention, not a literal legacy bus lock; actual x86 split bus locks are the separate invalid-placement case in §5.

Define the measured diagnostic:

\[
A_{\mathrm{RMW}}=\frac{\text{all queue RMW attempts, including helpers and failures}}{\text{completed payload-token lifecycles}}.
\]

Account for work still in flight at interval boundaries. Per-thread local counters or sampled diagnostic runs must not create a new shared counter on the hot path. The theoretical uncontended value six is not an assertion that every instrumented run will observe exactly six.

A simple capacity model for a hot coherence resource j is rho_j=λD_j, where D_j is the measured serial service demand per useful lifecycle and λ the offered useful rate. Sustained rho_j≥1 cannot support a stationary backlog-free workload. Real service demands change with contention; this relation is a necessary-load sanity check, not a formula for p99.9 or an assumed hardware line-transfer latency.

### 7.6 Mitigations that preserve their costs and semantics

A bounded local delay before retry can reduce simultaneous ownership requests, but must not become an unbounded sleep inside an operation advertised as a continuously active lock-free try call. Randomized backoff is a heuristic unless its adversary/probability model is specified. If a backoff policy changes primitive progress assumptions, repeat that proof.

Use producer admission/sharding to reduce contenders per queue where the application allows it. Keep helper operations limited to the proven metadata steps. Batching is a new publication/retention contract when it changes how long tokens or frontiers are held; do not treat it as a free six-RMW-to-zero optimization.

SCQ remains a separately gated candidate when NCQ's CAS hotspot fails the performance criteria. It has its own ticket-invalidation, threshold, weak-memory, and linearization proof obligations; changing CAS to fetch-and-add in this reference does not turn it into SCQ. [P2, §7]

Do not add a ticket lock as a contention remedy and keep the lock-free label. A stopped owner would reintroduce the exact dependency this campaign removed.

---

## 8. Cross-cutting hardening: finite identities and post-LP access

### 8.1 Counterexample C7 — Physical reuse is not identity reuse

Reduce the mathematical ticket width to eight bits and let N=4. A consumer saves a head ticket and index, then pauses. If the implementation allows 256 logical head advances with modulo wrap, the head representation can equal the old expected value again. A CAS based on that stale observation can now succeed against a different logical ticket. Its saved payload index need not identify the new item.

This counterexample is excluded only by a real no-live-repeat rule or a separately proved modular/suspension protocol. It is not excluded by saying a production-sized counter takes a long time to wrap. Block-generation bits, queue-ticket bits, participant identities, and external session handles must each satisfy their own lifetime rule.

### 8.2 A conservative nonwrapping ceiling

For a w-bit queue ticket representation and capacity N, propose a conservative ceiling:

\[
J=2^w-N-1.
\]

Every increment/helper update must have a desired value no greater than J, and every arithmetic intermediate used for packing/comparison must be representable. A generation is retired before normal operations need to cross this ceiling. A stale prepared update below J cannot wrap a current value: it either fails the comparison or installs its originally checked bounded successor. No entry comparison may overflow in the attempt to decide that it is safe.

This is a specification of rejected states and checked transitions, not implementation code or a completed lifecycle refinement. Operations that have already acquired a token retain responsibility for it when retirement is observed; a RETIRED return does not make it free. The ceiling can halt an operation before success and lead to quarantined/uncertain old work. It is a safety boundary, not a wait-free completion guarantee.

For a two-bit phase plus 62-bit block-generation field, also stop before generation repetition; an independent conservative generation ceiling is 2^62−2. A session label alone is insufficient if an old raw pointer has been redirected by unmapping and reusing its address. No local address reuse while live aliases remain is part of the lifecycle contract.

### 8.3 Counterexample C8 — Logging after publication corrupts the next owner

A installs b in QR and pauses before its enqueue function returns. B consumes b, finishes all reads, returns it to QF, and C acquires b for a new generation. A resumes and writes a diagnostic timestamp, phase, owner marker, or “send complete” flag into b's descriptor.

That write now conflicts with C's generation. The fact that A's original function has not returned does not preserve A's ownership.

**Required invariant:** after the publication LP, only permitted queue bookkeeping and process-local bookkeeping may use the old operation. No mutable block field, payload byte, or borrow-liveness flag in the transferred block may be updated. The symmetric rule applies after a consumer publishes b into QF.

A delayed helper also holds a reference to the old queue mapping, even when it no longer owns a payload block. Quiescence therefore counts active metadata calls as well as outstanding payload leases.

### 8.4 Notification failure stays separate

The single-waiter parking proof uses an acquire-release RMW chain on its wait word, including same-value operations. It does not make a user-space publication atomic with a kernel wake. A producer can die in either intervening gap. A bounded wait followed by an acquire predicate/lifecycle recheck is still necessary for crash-aware detection. [P2, §8; R11, R13]

Propose a maximum requested wait interval of **1 ms** for that crash-aware profile. The actual observation bound is not 1 ms: it includes scheduler delay and local recheck time. A timeout requests a chance to recheck; it neither proves process death nor authorizes reclamation. MPMC wait coordination remains unapproved.

---

## 9. Quantitative kill gates and the measurement contract

### 9.1 No measured threshold crossing is claimed

There is no implementation and no target-machine measurement. This section freezes **proposed acceptance policy** so a later result can reject the architecture rather than motivate moving the goalposts. A safety counterexample can reject a property now. A latency hypothesis cannot be declared passed or failed until a correctly defined experiment supplies observations.

The primary latency cliff is:

\[
\boxed{X=1{,}000\ \mathrm{ns};\quad T_{p99.9}>X\ \Rightarrow\ \text{reject the qualified NCQ deployment profile under the repetition rule}.}
\]

This is a chosen one-microsecond service budget. It is not a number derived from M-series cache geometry, an estimated coherence round trip, or a claim that an unscheduled thread must resume within it.

### 9.2 Reference workload and exact denominator

Use a native CPU-only workload with N=1024, B=64, direct construction in mapped storage, no Python calls, warm attached mappings, and the strict-isolation NCQ-SC64 format. Work per payload and read validation are fixed. No deliberate long user borrow is inserted into the normal latency profile; separate fault/retention profiles exercise those cases.

Each scheduled offer has an intended time t_offer. End-to-end latency is the time until a winning consumer finishes the required reads of that same message, minus t_offer. Preserve an additional interval beginning immediately before the actual native reservation call so generator delay and library/handoff delay remain distinguishable. A generator delay must not vanish by moving t_offer forward after a stall.

Every scheduled offer stays in the denominator. Retried reserves, contention, backpressure, pending operations, and missed deadlines are not deleted. A request that cannot be shown to complete by X is a deadline miss; it does not become a fast successful observation because its eventual retry was quick. Any bounded drain/end-of-run censoring is reported, and unresolved requests count as deadline misses. Full distributions retain their censored status rather than inventing finite latency values.

Define:

\[
p_X=\Pr(T_{\mathrm{end-to-end}}>X\ \text{or no successful completion by }X).
\]

The tail objective is p_X≤10^-3, with the quantile and equality convention fixed in the final harness specification. The raw elapsed distribution includes scheduling delays. An additional scheduler-attributed diagnostic may explain a failure but cannot replace the primary observations.

Use two complementary workloads. An uncontended run has one producer and one consumer, one message outstanding, and no artificial per-message kernel wake. A contention run uses a fixed open-loop arrival schedule that is strictly below the calibrated sustainable service rate of every compared candidate. Let the common load be 0.70 times the smaller of the candidates' separately calibrated sustainable rates for that topology. This avoids treating overload as an intrinsic queue defect. It also deliberately cannot establish the capacity objective; capacity is measured independently so a slow candidate cannot pass merely by lowering the common load.

If an application-specified arrival rate exists, it takes precedence as an additional required workload. A candidate unable to sustain it fails capacity even if it passes the easier common-load comparison.

### 9.3 Participant and topology matrix

Specify producer count P, consumer count C, and total registered endpoints K separately. The required sweep is 1P/1C, 2P/2C, 4P/4C, 8P/8C, plus 16P/1C and 16P/16C. The last two have K=17 and K=32, not K=16. Admitting “sixteen producers” while leaving readers outside the registration/capacity accounting is a contract error.

For K=32 the Turn 2 example's one control page still fits its proposed 32 participant cells plus seven control cells: 2,496 bytes for A=64, or 4,992 bytes for A=128. The example total mapping sizes therefore remain unchanged for the stated V=4096/16384. This is layout arithmetic, not permission to add unknown fields later without checking them.

Separate observed/enforced same-cluster P/P, cross-cluster P/P, P/E, E/E, and migration regimes where the target exposes meaningful evidence. On x86 separate same locality domain from cross-domain cases. A failing hostile topology is not silently removed from a product that claims to support it. Equally, an intentionally unqualified topology must not be used to falsify a narrower declared service level.

### 9.4 Repetition and uncertainty policy

For each required condition, propose five independently restarted runs with at least 10^7 scheduled offers per run. This yields approximately 10,000 observations in the upper 0.1% when outcomes are uncensored; it does not make correlated observations independent.

**Operational rejection rule:** if the empirical p99.9 exceeds X, or the empirical deadline-miss fraction exceeds 10^-3, in at least three of those five runs, reject that deployment profile. A safety failure requires only one valid witness and is not subject to this majority rule.

**Acceptance rule:** all five runs must meet the empirical objective, measurement uncertainty must be small enough not to straddle the threshold, and a preregistered dependence-aware uncertainty analysis must support the claimed tail rate. Mixed runs or inadequate timing resolution are inconclusive, not a pass. Turn 5 must freeze the dependence/block treatment and interval construction before qualification data are inspected. A naive IID binomial interval on an autocorrelated trace is not sufficient by assertion.

No constant timer-overhead subtraction is allowed to manufacture a pass. Report raw and calibrated intervals, quantization, timestamp ordering, and cross-core comparability. If the timing system cannot resolve the sub-50-ns aspiration, state that limit rather than interpolate nanoseconds that were not observed.

### 9.5 Complete kill register

| ID | Criterion | Consequence and scope |
|---|---|---|
| K-SAFETY | One legal execution yields torn control, two owners, unreadied payload, overwrite-before-release, invalid FIFO result, or token duplication | Immediate rejection of the implementation/protocol variant; no statistical tolerance |
| K-ABI | One supported attach path permits incompatible atomic width, alignment, stride, construction, or hidden process-local atomic lock | Reject the target/profile before operation |
| K-RECOVERY | One admitted crash leaves an owner/outcome gap while the specification requires autonomous leak-free same-generation recovery without quiescence or a new protocol | Reject that recovery requirement for NCQ-SC64 now |
| K-REVOKE | One resumable old access can touch newly assigned bytes | Reject reclamation/cutover; quarantine or fence |
| K-POWER | Durable recovery is required after one host power loss using only this volatile transport | Reject the persistence requirement for this design; SPSC is not a remedy |
| K-FAST | Proposed uncontended p50 is not below 50 ns in the nominated direct-construction experiment | Original performance aspiration not met; do not label the system sub-50-ns based on throughput |
| K-TAIL | Native end-to-end p99.9 exceeds 1,000 ns under §9.4 at common sub-saturation load | Abandon NCQ for that latency-qualified deployment, or reject the host/service configuration if alternatives also fail |
| K-CAPACITY | Sustained goodput is below 75% of a semantically admitted SPSC-lane alternative under equal total CPU/resources, reproducibly | Prefer lanes for that service; do not invoke the comparison when global FIFO semantics differ |
| K-THRASH | Mean A_RMW exceeds 24 attempts/lifecycle and goodput at 16P/16C is below 90% of 8P/8C, in at least three of five matched trials | Reject the central NCQ scaling profile; evaluate sharding or the separately proved SCQ candidate |
| K-STARVE | A request crosses the 100-ms tripwire while peers and resource evidence meet §6.5 | Reject the per-client service policy; classify scheduling versus individual CAS unfairness |
| K-RESOURCE | A required retirement would exceed two retained quarantines or the global staged/object cap | Stop new admission; never reclaim an unfenced generation to hide the failure |
| K-ROLLOVER | One live queue/generation identity can repeat, or arithmetic crosses its admitted ceiling | Reject the lifetime/refinement design |
| K-WAIT | A live single-waiter execution strands a satisfiable predicate after both sides finish the specified protocol | Reject the adapter/protocol; a crash-gap timeout is a separate case |

The 75%, 24-attempt, 90%, and 100-ms values are engineering stop-loss choices, not thresholds measured in prior art. The 24-attempt criterion is four times the six-RMW uncontended reference cost. It is combined with a throughput regression so an operation count alone is not confused with a latency theorem.

The p50 quantile for the original 50-ns aspiration is also an explicit proposal resolving its previously unspecified statistic. It does **not** satisfy a requirement that every message, the worst case, or p99.9 be below 50 ns. Such a stricter requirement must keep its own threshold and may fail even when K-FAST passes.

### 9.6 Crash recovery timing and resource thresholds

The formal recovery impossibility threshold is one unresolved ownership outcome under the constraints of §3. It is independent of elapsed time. A finite timeout cannot add missing ownership information.

For the managed-generation alternative, propose a separate **100-ms p99 recovery-time objective**, measured from confirmed failure notification to a validated successor becoming available to the surviving endpoints. This applies only under a live, schedulable authority, available reserved resources, and the declared cutover policy. It is neither a universal upper bound nor evidence that no old messages are uncertain. Record detection delay separately from recovery work; counting only after failure confirmation must not hide a slow failure detector.

Failure to provide the authority handoff, object-budget enforcement, or all-reference quiescence protocol blocks release even before timing is attempted. The reference queue's accepted core proof cannot waive these lifecycle obligations.

---

## 10. Mandatory declarative adversarial histories

These are proof/test specifications for the later authorized implementation, not executed tests. Each has a decisive allowed/forbidden outcome rather than a vague requirement to “stress it.”

| History | Controlled cut/interleaving | Required result |
|---|---|---|
| H01 | Kill producer after QF head CAS, before owner record | Healthy QR delivery continues; orphan uncertainty recorded; no guessed token return |
| H02 | Kill during multi-line payload construction | Partial bytes never become a ready message |
| H03 | Pause COMMITTED producer before QR insertion | Other completed messages can publish; no phase-scanner duplicate |
| H04 | Kill after QR entry CAS before tail update | Reader can claim published message; other enqueuer can help T |
| H05 | Consumer takes last installed entry before T advances | Legal H=T+1 accepted; no false corruption/error from an H≤T assertion |
| H06 | Pause old consumer after entry read; recycle slot through other participants | Old head CAS cannot confer ownership; no payload read before successful refreshed claim |
| H07 | Strong CAS fails and updates expected | Old desired/index discarded; no same-value head-CAS false ownership |
| H08 | Pause publisher after LP; fully cycle same block through peers | Publisher resumes with queue/local bookkeeping only |
| H09 | Kill consumer after claim before phase update | Unknown borrowed token not reclaimed from COMMITTED alone |
| H10 | Kill after external effect, before return | No fabricated exactly-once result; uncertainty explicitly reported |
| H11 | Kill after EMPTY before QF insertion | No assumption that EMPTY implies available storage |
| H12 | Return b to QF, pause releaser, reallocate b | Releaser never writes b again |
| H13 | False timeout followed by resumed writer | Old writer cannot access successor bytes; same-byte reassignment prohibited |
| H14 | Parent exits but child retains inherited mapping | Parent-only exit evidence insufficient for reclamation |
| H15 | Unlink/close object while old mapping exists | No claim of pointer revocation; old allocation remains isolated |
| H16 | RETIRE_REQUESTED races a prepared QR CAS | Late old publication allowed only as declared old-generation outcome; no false sealed-cutover claim |
| H17 | Recovery authority dies before/after successor exposure | No duplicate current generation; bounded retained/staged objects or safe stop |
| H18 | Third unfreeable retirement with two quarantines retained | Admission stops before violating object budget |
| H19 | Malformed member offset, width, array stride, or page boundary | Attach rejects before invalid atomic execution |
| H20 | Aligned multiword descriptor observed across update | No authorization from torn logical identity; single-word packing/ownership rule enforced |
| H21 | Repeated P/E migration while using admitted primitives | No change to object identity/placement; correctness preserved, retries and latency reported |
| H22 | Background writer delayed, interactive readers busy | Ready independent work progresses when readers run; no promise of producing absent input |
| H23 | Low-QoS borrowers retain every token | NO_CAPACITY truthful; no token stealing or false count of committed messages |
| H24 | One strong-CAS contender loses repeatedly to peers | Global progress distinguished from individual starvation; request remains in tail ledger |
| H25 | Hypothetical weak-CAS substitution with spurious failures | Existing strong-CAS progress proof not reused unchanged |
| H26 | Eight-bit ticket domain with stale expected across wrap | Retirement prevents repeated identity; prohibited wrapped history is recognized |
| H27 | Counter/helper reaches ceiling with token in flight | No overflowing desired value; token remains accounted during retirement |
| H28 | Notify before arm, arm before notify, and same-value notification | Existing single-waiter proof preserved in every ordering |
| H29 | Kill after publication or after wait-word update, before wake | Bounded requested wait plus recheck detects when scheduled; no same-byte recovery on timeout |
| H30 | Repeated crashes with replacement endpoints | Retire on unresolved loss; no unbounded same-generation capacity erosion |
| H31 | Global power loss or detected prior control corruption | Failure of out-of-scope service requirement reported; no volatile exactly-once recovery claim |
| H32 | Saturating offered load or a stalled load generator | Pending/offered work retained in denominator; no inverse-throughput latency substitution |

For finite-model work after authorization, begin with N=2 and at most two participants, or N=4 with three or four participants, and small identity domains. Keep the admitted K≤N bound; test K>N separately as a required admission rejection, not as a valid-history counterexample. Include both queues and token conservation; testing QR alone misses the free-pool ownership gaps. Distinguish the abstract SC model from hardware/compiler refinement and from elapsed-time measurements. A bounded exploration without a counterexample is evidence for the explored bound, not a proof for every history.

No code for this exploration is supplied at this gate.

---

## 11. Alternative selection without semantic camouflage

### 11.1 When to select SPSC lanes

Select isolated SPSC lanes as the preferred service architecture when the application needs per-producer order, bounded fault domains, and short native handoffs more than one globally linearized MPMC queue. One failed writer then affects its lane's unpublished slot and capacity rather than removing untraceable tokens from a shared pool. Its own lane may still require fencing and reset; healthy lanes do not need to wait for it.

With P producers and one reader/multiplexer, each producer-to-reader lane can remain genuinely SPSC. With multiple consumers, choose explicit static routing or a dispatcher with separately specified output lanes. A P×C lane matrix trades shared arbitration for O(PC) lane resources and routing policy. A multiplexing reader trades centralized CAS for scan/scheduling cost and may itself become a single point of service delay.

A fair bounded scan among ready lanes does not establish reservation-time global FIFO. Adding timestamps and waiting for absent earlier work can recreate head-of-line blocking. Changing that ordering is a product decision, not an invisible optimization.

### 11.2 Zero-copy across a dispatcher is not free

Forwarding a borrowed pointer to another consumer prolongs the original lane's borrow. That lane's producer cannot reuse the storage until the downstream borrower finishes. The dispatcher must not publish reclamation merely because it forwarded a descriptor. Copying into another lane simplifies lifetime but is a copy. A shared payload pool reintroduced under the lanes brings its own ownership/recovery obligations back.

Accordingly, benchmark the actual routed architecture, including any extra hop, held lease, copy, and wake. Do not compare a one-hop NCQ end-to-end path against a raw isolated SPSC loop while omitting the multiplexer and call that a deployment result.

### 11.3 When global FIFO is mandatory

Do not replace NCQ-SC64 with lanes unless the global ordering contract is explicitly changed or a valid merge protocol is added and measured. If strict FIFO is mandatory and NCQ meets the empirical gates, retain it with a managed recovery envelope and the stated failure uncertainty. If contention kills it, investigate the exact single-width SCQ candidate, preserving its separate proof obligations. If neither qualified FIFO profile meets the requirement, report the unsatisfied requirement rather than relabeling a weaker service.

SPSC is not a fix for untrusted shared writers, indefinite scheduling pauses, finite-memory exhaustion under unlimited unfenced failures, or volatile power-loss recovery. It primarily changes synchronization cost, ownership locality, and fault containment.

---

## 12. Final admission record for Turns 4 and 5

| Property | Turn 3 decision | Remaining evidence |
|---|---|---|
| NCQ publish-first FIFO and helpable tail | Retained within the exact abstract model | Faithful finite-width/atomic refinement and later executable validation |
| SPSC two-direction payload ownership | Retained | Native ABI and implementation audit |
| No split placement for admitted words | Proved by natural-alignment arithmetic | Actual member offsets, mapping, target primitive contract |
| No false sharing of designated controls | Conditional on every relevant granularity dividing A | Target qualification; not a universal M-series cache assertion |
| Strong-CAS queue progress | Conditional on admitted primitive progress and correct retries | LL/SC/LSE/helper audit; no per-call fairness guarantee |
| Automatic leak-free same-generation crash recovery | Rejected for the current schema | New recoverable transfer design or coordinated quiescence |
| Bounded managed failover | Safety direction and resource policy specified | Authoritative generation record, registration, handoff, and all-reference quiescence protocol |
| MPMC parking | Not admitted | Independent multi-waiter proof and adapter validation |
| Crash-aware PARKABLE_SPSC | Retained normal-enrollment proof; finite requested waits required | OS behavior and fault-gap validation after implementation authorization |
| Sub-50-ns / p99.9≤1,000-ns performance | Unmeasured policy gates | Turn 5 preregistration and later hardware measurements |
| Hard realtime / exactly-once / power-loss persistence | Not supplied by this design | Different execution/storage/application protocol |

**Proceed to Turn 4 for the native ABI and explicit lifecycle contract. Do not promote NCQ-SC64 to a production “self-healing” fabric on the strength of its queue proof.** The schema must express unresolved ownership, retirement, active-call/alias lifetime, and error semantics rather than leaving those states outside the API.

If the required product contract insists on all of the following simultaneously—arbitrary crashes and pauses, raw zero-copy pointers, no effective fencing, fixed finite memory, no loss, no quiescence, and indefinitely available recovery—the campaign must reject that contract now. Swapping CAS for FAA, or MPMC for SPSC, does not satisfy it.

The meaningful result of this gate is narrower and strong: **publish-first removes the stopped-publisher FIFO hole; natural alignment removes the admitted split-word placements; neither recovers missing ownership information nor bounds a scheduler.** Those boundaries are now explicit enough to falsify.

---

## 13. Primary-source ledger and evidence limits

**Access date for new web research:** 2026-09-23. Source descriptions below distinguish this turn's retrieval from facts carried in the hashed predecessor. No external implementation is copied into the deliverables. Browser-generated citation tokens are replaced here by durable source references.

### Project records

**[P2] Audited Turn 2 report.**

`02_Turn_02_Cache_Topologies_and_Barrier_Proofs.md`  
91,463 bytes; SHA-256 `9bb8b971b65f7da80bd13053e8027aaa6ede5ae9ea19da8262e24282e16a6977`.

See repository reference file: `docs/reference/02_Turn_02_Cache_Topologies_and_Barrier_Proofs.md`.

Full supplied text and mounted bytes form the audit basis. Relevant sections: §1 model; §2 conditional alignment; §6 exact NCQ-SC64 operations, proof, and crash limit; §8 single-waiter protocol; §§10–12 cost model and admission. The cumulative bundle preserves these bytes unchanged. None of its pen-and-paper conclusions is reclassified as a measured result.

**[P1] Original Turn 1 packet.**

`01_Turn_1_Architecture_and_Memory_Model.md`, `02_Turn_1_Proof_Obligations_and_Kill_Gates.md`, `03_Turn_1_Primary_Source_Ledger.md`, and its original deposit receipt/manifest. The original ZIP and manifest establish the inherited artifact set. The similarly named shorter lab-side Turn 1 document in Drive is a separate variant, not silently substituted for this packet.

### Language and primitive contracts

**[R01] GCC, Atomic Built-ins.**

https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html

Official live compiler documentation, read this turn. Supports strong/weak distinction, updating the expected argument on failure, width/fallback concerns, and lock-free queries. It does not certify an unbuilt Apple Clang or Linux binary. Used for the exact retry-wrapper counterexample and primitive admission, not as a hardware latency table.

**[R02] Linux kernel, Atomic types — Forward progress.**

https://docs.kernel.org/core-api/wrappers/atomic_t.html

Official kernel documentation, read this turn. The forward-progress discussion warns about compare/exchange implemented using LL/SC and compiler-generated loops. Used as a primary implementation warning. Kernel atomic API ordering rules are not imported as the userspace C11 API.

**[R08] Cambridge memory-model researchers, C/C++11 mappings to processors.**

https://www.cl.cam.ac.uk/~pes20/cpp/cpp0xmappings.html

Author-maintained mapping reference, read this turn. Supports expected x86/AArch64 mappings and the distinction between a source operation and its instruction sequence. It is not actual disassembly or a full M-series misalignment specification.

**[R12] C++ working draft, operations on atomic types.**

https://eel.is/c++draft/atomics.types.operations

Live working draft, read this turn. Supports compare/exchange value/expected semantics, permitted weak spurious failures, and recommended practice concerning progress. It is cited as a live working draft, not misidentified as a frozen C++20 edition. The authoritative implementation remains the C11 ABI proposed in the predecessor.

**[R14] C++ working draft, ordering and lock-free properties.**

https://eel.is/c++draft/atomics.order

https://eel.is/c++draft/atomics.lockfree

Ordering/address-free background carried from the predecessor's R02. This turn preserves the predecessor's release/acquire and admitted interprocess-ABI model; it does not infer a universal ISO guarantee for arbitrary mmap atomic layouts.

### Process, mapping, and wait lifecycle

**[R03] Linux man-pages, signal(7).**

https://man7.org/linux/man-pages/man7/signal.7.html

Read this turn. Supports the uncatchable SIGKILL boundary and signal/process behavior. A signal request is not used as a proof that all relevant address-space writers are fenced.

**[R04] Linux man-pages, pidfd_open(2).**

https://man7.org/linux/man-pages/man2/pidfd_open.2.html

Read this turn. Supports identity-bound task handles, registration caveats, and process-versus-thread termination notification distinctions. Registration-before-use and fencing inherited mappings are this report's additional requirements.

**[R05] Linux man-pages, fork(2) and mmap(2).**

https://man7.org/linux/man-pages/man2/fork.2.html

https://man7.org/linux/man-pages/man2/mmap.2.html

Read this turn. Supports inherited mappings and shared mapping behavior. The no-unregistered-descendant admission policy is this report's deduction. The text does not claim private copy-on-write mappings and MAP_SHARED mappings have the same write-sharing semantics.

**[R06] Linux man-pages, shm_open(3) / shm_unlink.**

https://man7.org/linux/man-pages/man3/shm_open.3.html

Read this turn. Supports closing descriptors without removing mappings, unlinking names without revoking extant mappings, and creating a new distinct object after unlink. Used to rule out unlink-as-fencing.

**[R11] Apple libplatform, public address-wait header.**

https://raw.githubusercontent.com/apple-oss-distributions/libplatform/main/include/os/os_sync_wait_on_address.h

Official live source, read this turn. Supports four/eight-byte alignment, consistent shared flags, compare-and-wait, deadlines, and explicit lack of priority-inversion avoidance. Exact deployment SDK and binary remain unqualified. Only documentation is referenced; no header code is included.

**[R13] Linux man-pages, FUTEX_WAIT.**

https://man7.org/linux/man-pages/man2/FUTEX_WAIT.2const.html

Underlying OS contract carried from the predecessor's R28: expected-value comparison/enrollment, timeout and recheck behavior. The userspace single-waiter proof remains the predecessor's proof, not something supplied by the syscall alone.

### Scheduling, hardware failures, and alternative recovery research

**[R09] Linux kernel v6.16, Bus lock detection and handling.**

https://www.kernel.org/doc/html/v6.16/arch/x86/buslock.html

Versioned primary documentation authored by Intel engineers, read this turn. Supports the distinction between split locked operations, atomicity-preserving bus locks, large disruption, and trap/throttling policies. It does not establish the exact behavior of every x86 deployment or any Apple processor.

**[R10] Apple, Energy Efficiency Guide for Mac Apps — Prioritize Work at the Task Level.**

https://developer.apple.com/library/archive/documentation/Performance/Conceptual/power_efficiency_guidelines_osx/PrioritizeWorkAtTheTaskLevel.html

Official archived guidance, read this turn. Supports QoS distinctions and identified synchronous dependency-promotion cases. It does not promise hard timing, cross-process raw-ring priority donation, or core pinning.

**[R07] Attiya, Ben-Baruch, Fatourou, Hendler, Kosmas — Tracking in Order to Recover: Detectable Recovery of Lock-Free Data Structures.**

https://arxiv.org/abs/1905.13600

Author-submitted publication record and abstract read this turn; version 2 dated 2021-07-27. Supports that detectable recovery is studied using additional tracking descriptors and persistence assumptions. No theorem from the full paper is claimed adopted or proved for this volatile IPC pool. No PDF was analyzed or benchmark result reproduced in this turn.

**[R15] Nikolaev, NCQ original source anchor.**

https://github.com/rusnikola/lfqueue/blob/708c0052872950dcb15b487fa7a5dd77ce2a2746/lfring_naive.h

Primary artifact audited in Turn 2; pinned commit and source blob are recorded in [P2, R15]. This turn audits the fully specified NCQ-SC64 refinement in [P2], not an independently recompiled source library. No claim that unmodified author code has this project's SC metadata, layout, recovery, or API contract.

**[R16] Apple XNU cache geometry anchors.**

https://github.com/apple-oss-distributions/xnu/blob/f6217f891ac0bb64f3d375211650a4c1ff8ca1ea/osfmk/arm64/proc_reg.h

https://github.com/apple-oss-distributions/xnu/blob/f6217f891ac0bb64f3d375211650a4c1ff8ca1ea/osfmk/arm/cpuid.c

Primary source interpretation carried from Turn 2's R03–R04. Its constant-versus-geometry caveat is retained. This turn did not obtain machine register values or establish a universal P/E L1/L2 line-size table.

### Retrieval limitations and final evidence boundary

Arm's current A-profile learning index was readable and identified its synchronization and memory-attributes guides. The guide links redirected to support endpoints that did not return usable content through the browser. This report therefore does not pretend to have established exact unsupported-access behavior or exclusive-monitor geometry for every M-series target from those guides. The proof instead rejects unadmitted accesses and uses the explicit primitive contract; exact hardware/compiler qualification remains required.

The queue, lifecycle, and timing results have three different statuses: the supplied reference was audited mathematically; missing recovery guarantees were rejected through explicit counterexamples; performance numbers are proposed future gates. No result is labeled as a completed executable stress test, a successful recovery trial, or an achieved latency measurement.
