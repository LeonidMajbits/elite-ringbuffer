# Turn 1 — Architecture and Memory Model
## Zero-Copy RingBuffer IPC and SPSC/MPMC Event Fabric

**Project:** Leonid Majbits / Gemini Operator Lab, paired with Leon  
**Date:** September 23, 2026  
**Stage:** 1 of 5 — architecture, proofs, protocol decisions only  
**Implementation status:** Not started; no implementation, executable header, benchmark, or compiler probe is included.  
**Performance status:** The sub-50-nanosecond target has not been measured.  
**Companion documents:** `02_Turn_1_Proof_Obligations_and_Kill_Gates.md`; `03_Turn_1_Primary_Source_Ledger.md`.

Source identifiers [S01]–[S32] resolve in the companion primary-source ledger. Design choices and deductions are explicitly distinguished from source facts. This document does not claim that the complete MPMC composition, crash recovery, or actual compiler output has already been proved or tested.

## 0. Executive architectural decision

Proceed with a **specialized fixed-slot SPSC queue**, an explicitly separate **parkable SPSC profile**, and a separately proved **MPMC descriptor-queue / fixed-payload-pool profile**. Share the bootstrap format, ownership vocabulary, C ABI, and lifecycle rules; do not force every SPSC operation through an MPMC sequencer, allocator, waiter registry, or reservation-recovery mechanism.

The first profile targets the smallest native CPU-to-CPU handoff. The second deliberately pays for notification bookkeeping to allow reliable sleeping. The third must establish its own linearizability, bounded-capacity semantics, descriptor ownership, and lock-free progress proof before it earns a lock-free label.

Four requested properties need precise boundaries:

* **Wait-free** describes a bounded number of a caller's own algorithmic steps, not a wall-clock deadline. An SPSC try-operation can return FULL or EMPTY without waiting for another process. A blocking wrapper or indefinitely retained payload lease cannot inherit that guarantee.
* **No false sharing** is not no communication. Producer publication and consumer reclamation necessarily cause true sharing. Cache-line isolation removes avoidable writer interference, not the coherence cost of IPC.
* **Atomic ticket locks are not lock-free queues.** Fetch-and-add can allocate unique tickets, but it does not make publication holes, abandoned reservations, or capacity accounting disappear. Vyukov's familiar bounded sequence-number queue explicitly disclaims formal lock-free progress. [S03]
* **Less than 50 ns is a hardware-specific acceptance experiment**, not an architectural theorem. It must include a precisely defined payload and timing interval. Sleeping, faulting, scheduling, Python calls, recovery, and MPMC contention are separately measured regimes.

The important MPMC design move is to **reserve payload storage without reserving a globally blocking FIFO position**. Write the payload first; publish its immutable descriptor through a proven ready queue. FIFO order is established by the ready queue's enqueue linearization, not by when a caller first obtains writable storage.

## 1. Scope and platform contract

### 1.1 Native language and dependency boundary

The proposed authoritative implementation language is C11 with a C ABI. C++20 clients may use an opaque wrapper, and Python may call the same ABI. This keeps exactly one implementation responsible for the shared atomic representation. Rust remains an alternative, not an additional independent implementation in the initial proof surface.

The native core uses the language library, OS shared-memory interfaces, and an OS wait adapter. POSIX and Darwin APIs are platform dependencies; therefore this is not a strictly freestanding or ISO-only ANSI program. There are no mandatory external native frameworks, allocators, threading packages, or daemon processes.

Actual Python CFFI is a third-party package. Consequently, the honest distribution claim is **zero third-party native-core dependencies, with an optional CFFI binding dependency**. A literal zero-third-party Python installation would require another binding choice, such as a CPython extension or a standard-library FFI, and is a different contract. [S30, S31]

### 1.2 Shared atomics are a platform commitment

An mmap region does not, by itself, establish all C/C++ object-lifetime and interprocess-atomic guarantees. The C++ specification recommends address-free lock-free atomics, including communication through differently mapped shared memory, but this is not a universal mandate that every atomic specialization on every implementation satisfies. A hidden per-process lock would be fatal here. [S01]

The supported tuple therefore includes OS, architecture, compiler family, native atomic ABI, and alignment profile. Admission requires address-independent, lock-free, naturally aligned 32-bit wait-word and 64-bit cursor operations on the supported target. Querying primitive lock-freedom is necessary, not sufficient: the emitted operations, library fallbacks, and IPC behavior must be audited after the implementation gate. Do not assume 128-bit lock-free atomics.

A C++ client or Rust client must not independently reinterpret a C11 atomic field using its own language's atomic type. Shared layout compatibility is a declared platform ABI, not something inferred from two types both being called “64-bit atomic.” Python never manipulates shared atomics directly.

### 1.3 Initialization and lifetime

One initializer creates and sizes the shared object, maps it, initializes the entire atomic/object state, and only then exposes the attachment information. A release-published READY lifecycle transition and an acquire-validated attach complete the protocol. A process must not race atomic construction merely because a public shm name already exists; bootstrap distribution must prevent that race. A failed pre-READY creation is discarded, not opportunistically adopted.

The object is never truncated, resized, reinitialized, or unmapped while a participant can retain a live lease. Failed generations are not reset in place while old writers remain capable of touching their pages. Shared memory uses relative offsets, so participants need not map it at the same virtual address. MAP_SHARED supplies shared mappings; it is not a durability or recovery protocol. [S15]

## 2. Circular topology and physical layout

### 2.1 Four regions

| Region | Contents | Mutation / access discipline |
|---|---|---|
| Header | Magic, ABI/version, endian, byte length, capacity, offsets, strides, maximum payload, alignment and atomic ABI profile, session identity, queue/wait mode | Immutable after bootstrap; validate before trusting offsets |
| Control block | Lifecycle, SPSC published-write cursor, SPSC reclaimed-read cursor, data and space wait words; MPMC-specific controls and participant records only in the MPMC profile | Each independently written hot control occupies its own coherence-isolation unit |
| Slot ring | Slot descriptors: length/type and optional metadata; per-slot generations only where required by the selected algorithm | Ownership transfers with publication/reclamation; no SPSC sequence array added merely for uniformity |
| Payload buffer | A fixed, contiguous array of aligned payload blocks | One writer or one borrowing consumer per block, never both |

The owner-only working cursor and cached peer cursor belong in **process-local endpoint state**. Making them shared would create extra writable cache traffic without helping correctness.

All offsets and extents are checked using overflow-safe arithmetic before attach. The mapping length, ring capacity, descriptor stride, payload stride, alignment profile, and queue mode must agree. A malformed header is an attach error, not an opportunity to attempt a misaligned atomic access.

### 2.2 Alignment algebra

Let A be the negotiated coherence-isolation alignment, V the queried VM page size, N the power-of-two slot count, B the maximum payload length, D the descriptor stride, and S the payload stride. Let round_up(x,a) mean the least multiple of a not smaller than x.

The proposed layout is:

- H = round_up(header_bytes, V).
- control_offset = H.
- ring_offset = round_up(control_offset + control_bytes, V).
- payload_offset = round_up(ring_offset + N·D, max(V, A, payload_alignment)).
- S = round_up(B, max(A, payload_alignment)).
- segment_bytes = round_up(payload_offset + N·S, V).

Alignment choices must be compatible powers of two in this profile. N·D, N·S, all additions, and the final mapped length are checked for overflow. For SPSC, control_bytes reserves separate A-byte cells for lifecycle, publication, reclamation, data wait state, and space wait state. Future counters must not be inserted into their padding without redoing the ownership analysis.

Candidate profiles are A=64 for admitted x86-64 machines and A=128 for admitted Apple Silicon machines. A uniform 128-byte format is also possible. These are **layout choices, not claims that every x86 or every M-series cache at every level has the same line size**. Apple explicitly provides runtime architectural queries, including page size and `hw.cachelinesize`; profile selection and measurement must use the actual target. [S08]

`alignas(64)` or `alignas(128)` on the first member is not enough. The starting address, occupied extent, next field, and array stride must keep independently written objects in different relevant coherence lines. Likewise, two adjacent payload blocks must not share a line simply because their descriptors were padded correctly.

This gives a conditional, checkable no-false-sharing guarantee for the enumerated hot fields on an admitted coherence profile. It does not eliminate coherence invalidations caused by a peer reading the very cursor that its owner writes, or by transferring ownership of payload lines.

### 2.3 SPSC physical specialization

For a short message, placing descriptor metadata adjacent to its payload may reduce line transfers compared with a physically distant descriptor array. For large messages, descriptor/payload separation may improve scanning and density. These are alternative versioned layouts; the four-region format is the initial specification, and an inline-slot profile must be benchmarked rather than assumed faster.

Every slot's immutable address is derived from its index and the validated header. No raw pointer, `std::string`, dynamically allocated object, Mach port name treated as a transferable capability, or language runtime object is embedded as portable shared state.

The baseline uses ordinary cacheable CPU memory. Apple unified memory does not merge the cores' private caches into one zero-latency cache. It also does not extend this CPU protocol automatically to GPU or DMA access. There is no per-message `msync`, cache-flush instruction, or DRAM writeback requirement in the CPU-to-CPU publication proof. [S09, S15]

## 3. SPSC ownership and memory-order proof

### 3.1 Name the two cursors by meaning

Let P be the number of fully published writes and C the number of fully released reads. Only the producer writes P; only the consumer writes C. Using these names avoids the contradictory “head means writer” and “head means reader” conventions in different libraries.

For the logical unbounded-counter proof:

**0 ≤ P − C ≤ N.**

Message ticket k occupies slot k mod N. The physical implementation uses 64-bit counters with an explicit no-live-wrap lifecycle rule until a modular proof is approved. Masking by N−1 selects a slot; it does not identify its generation.

The initial lease contract allows one outstanding reservation per producer endpoint and one outstanding borrow per consumer endpoint. Batching can be added later as a separately specified contiguous-prefix commit/release operation.

### 3.2 Producer-side transitions

A successful reservation checks capacity against an acquire-obtained reclamation snapshot. It gives the caller the next payload block but **does not advance P**. The reservation is private until commit. Cancelling this uncommitted reservation has no visible queue effect.

After all required payload and metadata writes complete, the producer release-stores the new P. No later payload write is permitted through that lease. The producer may keep its own cursor and the last acquired C snapshot locally; a stale reclamation snapshot is conservative and can cause a transient FULL result, not an overwrite.

### 3.3 Consumer-side transitions

The consumer acquire-loads P when its cached publication snapshot no longer proves availability. Only a snapshot covering the desired ticket authorizes reads of that slot's metadata and payload. The consumer then borrows the payload in place.

Only after every required payload access and every exported borrow has ended may the consumer release-store the new C. The producer must acquire-observe sufficient reclamation before writing the next generation of the same slot. A stale publication snapshot is conservative and may produce a transient EMPTY result.

Failure results are snapshot-based availability observations; the initial API will not promise an instantaneous global queue-size oracle. Exact linearizability semantics for failed operations and endpoint handover belong in Turn 2. Acquire does not promise that a load retrieves the newest value in wall-clock time.

### 3.4 The two happens-before chains

For publication, denote sequencing within an endpoint by sb and release/acquire synchronization by sw:

**write(payload_k) →sb release(P covering k) →sw acquire(P covering k) →sb read(payload_k).**

For recycling:

**read(payload_k) →sb release(C covering k) →sw acquire(C covering k) →sb write(payload_{k+N}).**

A later acquired publication/reclamation value may cover several earlier tickets because the relevant earlier accesses are sequenced before that later release. The proof is about the value observed and the ownership it establishes, not merely about executing an acquire instruction somewhere nearby. [S02, S07]

The second chain is indispensable: a publication-only design can correctly announce new messages while still overwriting storage a slow consumer is reading.

Payload fields may be non-atomic, and a payload may span many cache lines, **provided this exclusive-ownership protocol is obeyed**. The metadata that authorizes access is the atomic synchronization point. A checksum cannot repair a C/C++ data race caused by reading storage concurrently with reuse. Reading first and checking a version afterward is not approved here.

### 3.5 Progress classification

With bounded payload size, local state, and admitted lock-free atomic loads/stores, the polling SPSC try path requires bounded own steps, no CAS retry, no heap allocation, and no syscall. This is the proposed wait-free algorithmic profile. It is not a guarantee that a preempted process, a page fault, or an arbitrary user's serialization routine completes in a fixed elapsed time.

FULL and EMPTY return immediately. A user who holds a lease indefinitely consumes capacity indefinitely. A wait-until-success convenience function is a separate blocking operation. These distinctions are part of the public contract, not footnotes to a universal “wait-free” claim.

## 4. ARM64, x86-64, and exact ordering boundaries

### 4.1 Why volatile fails

C/C++ volatile does not establish release/acquire synchronization or make an ordinary conflicting payload access race-free. Compilers may optimize other non-volatile accesses under the language's data-race rules, and hardware may expose memory operations in orders that a source-code reading did not anticipate. x86's stronger ordering can conceal a bad publication protocol; it does not make the language-level race correct. [S02, S04, S09]

On AArch64, publication must order ordinary payload writes before the release that makes them available. Consumption must acquire that publication before the authorized payload reads. Reclamation requires the matching reverse-direction relationship. Coherence for an individual location is not a cross-location ordering proof.

### 4.2 Expected instruction lowering—not an invented disassembly result

The following is the baseline mapping for admitted naturally aligned 32/64-bit atomics. It describes documented compiler mappings, not code compiled during this planning turn. [S05, S09, S10]

| Language-level operation | Typical AArch64 lowering | Typical x86-64 lowering |
|---|---|---|
| Relaxed atomic load / store | LDR / STR | MOV / MOV |
| Acquire load | LDAR; target/compiler may select LDAPR where supported and valid | MOV with compiler ordering constraints |
| Release store | STLR | MOV with compiler ordering constraints |
| Explicit acquire thread fence | DMB ISHLD | Usually compiler ordering only |
| Explicit release or acquire-release thread fence | DMB ISH | Usually compiler ordering only |
| Explicit sequentially consistent thread fence | DMB ISH | MFENCE or a suitable locked operation |
| FAA or CAS | Appropriate LSE atomic instruction, or exclusive-load/store retry sequence | LOCK XADD / LOCK CMPXCHG, as appropriate |

AArch64 sequentially consistent atomic loads/stores can also map to LDAR/STLR; “seq_cst always emits DMB” is false. Conversely, the presence of LDAR/STLR does not show that an operation was only acquire/release. [S05]

For the simple SPSC handoff, the expected important instructions are STLR for P publication, LDAR or an appropriate acquire alternative for P observation, STLR for C reclamation, and LDAR or an appropriate acquire alternative before reuse. There is no independent requirement to sprinkle DMB ISH between every payload field.

DMB ISHLD is relevant when an explicit acquire-fence construction is chosen. DMB ISH is relevant to stronger explicit fences and some other constructions. ISH refers to the inner-shareable domain; the admitted mapping assumes the OS provides ordinary coherent shared CPU memory. A compile audit must inspect the exact target triple, CPU features, compiler version, optimization/LTO settings, atomic width, and any outlined atomic helpers. [S04, S05, S09]

### 4.3 Speculation is not the same as architectural ordering

Acquire ordering prevents the program from architecturally consuming stale payload as though a correctly observed publication authorized it. It does **not** mean all younger instructions stop executing speculatively until a branch validates the index. LDAR and DMB are not a general Spectre defense or a promise of zero transient access.

The source protocol must place payload access after the successful availability decision. If the mapping ever becomes a hostile-input security boundary requiring transient-execution protection, that is a separate bounds-check/speculation policy with separate instruction requirements. Do not describe a memory-order proof as a security speculation barrier. [S09, S32]

### 4.4 The x86 trap that matters to sleeping

x86 TSO still permits a later load of a different address to observe memory before an earlier buffered store becomes visible to another core. Consequently, a consumer storing “I will sleep” and then reading “queue empty,” while the producer stores “queue nonempty” and reads “no waiter,” is not made safe merely by x86 or by release/acquire operations that fail to read from one another. The wake protocol needs its own ordering argument, not an assumption that the payload protocol solved it. [S05, S09]

## 5. MPMC: algorithm selection and honest progress guarantees

### 5.1 What the common per-slot sequence protocol proves

For explanatory comparison, a bounded sequence-slot design initially assigns seq[i]=i. A producer with ticket p may own its slot only after an exclusive claim and an acquire observation of the appropriate free generation. Publication release-stores seq=p+1. A consumer with ticket c acquires the ready generation c+1; after its payload reads it release-stores seq=c+N to advertise reuse.

In that particular design, the global position-claim CAS can be relaxed because payload ordering travels through per-slot sequence values. This is an algorithm-specific conclusion, not permission to make every FAA in a different queue relaxed. The position counter is a reservation counter, not a contiguous commit frontier. [S03]

Unconditional FAA beyond capacity, abandoned claimed tickets, or a later attempt to decrement a shared allocation counter are not valid bounded admission protocols. Capacity, retries, cancellation, and generations must be specified together.

### 5.2 The publication-hole counterexample

Producer A obtains the earliest unresolved ticket and stops before publication. Producer B publishes a later ticket and returns successfully. A consumer encounters A's unpublished position.

If it waits for A, progress depends on that stopped producer. If it simply reports EMPTY despite a later completed enqueue that must remain observable under a strict FIFO contract, the ordinary empty-result semantics are wrong. A working algorithm must resolve this through its actual ordering, helping, cancellation, or bypass rules; atomicity of the ticket increment alone resolves none of it.

This is why a mutex-free sequence ring must not be advertised as a formally lock-free MPMC FIFO without further argument. Vyukov's own description makes this distinction. [S03]

### 5.3 Proposed MPMC direction: publish handles to completed payloads

The leading candidate is a fixed payload pool plus two bounded descriptor queues: one distributing available block identifiers and one distributing committed descriptors. Each payload block is written before it is enqueued on the ready side. A consumer dequeues a ready descriptor, reads its immutable payload, and returns the block only after all borrows end.

The pool lifecycle is conceptually FREE → WRITING → READY → READING → FREE, with explicit transfer and quarantine states added during the detailed proof. The capacity accounting must include **all** held leases and descriptors temporarily owned by an internal queue operation, not just messages currently visible in the ready queue.

A producer stalled while filling a payload consumes one pool block, not the next global ready-queue position. This isolates user-controlled construction from the sequencer. Queue order is the ready queue's commit linearization order; it is not reservation order. There may be no free payload blocks even when the ready queue is empty because callers are holding write or read leases. FULL therefore means exhausted admitted storage, not necessarily N committed messages.

SCQ is a relevant primary candidate: Nikolaev presents a bounded, linearizable, lock-free FIFO based on single-width primitives, with an author-maintained implementation and discussion of variants. This turn selects it for full proof and compatibility review; it does not claim to have verified the full paper's proof or the correctness of a modified IPC wrapper. [S19, S20]

The composition must prove unique block ownership, ready-queue publication ordering, no descriptor duplication, reclamation ordering, bounded capacity under paused operations, and what happens when a process dies at every internal transfer boundary. A lock-free descriptor queue does not automatically make leaked payload reservations recoverable or make the full application wait-free.

### 5.4 Fallback architecture, not semantic camouflage

A set of SPSC lanes is a valuable event fabric when per-producer ordering is sufficient. A receiver or an application-owned merge component can choose among lanes. One failed producer need not prevent draining healthy lanes. This is not an MPMC FIFO with global total order unless a separate merge protocol supplies that guarantee.

Likewise, work-sharing means one consumer receives each item. Multicast means several consumers independently observe it and storage cannot be reclaimed until their configured gates allow it. LMAX's sequencing and consumer-dependency architecture is relevant to multicast, not proof that work-sharing and broadcast can use the same two cursors. [S23]

### 5.5 Generations and ABA

Slot index equality is not ticket equality. Every reusable MPMC handle must carry or be checked against enough identity to distinguish session, block, and generation. Finite-width generations eventually repeat; “we use 64 bits” is an engineering lifetime argument, not a proof under arbitrary suspension.

The initial lifecycle rule forbids live counter wrap and requires quiescence followed by a new session before exhaustion. N must remain below the half-range used by any approved modular ordering comparison. Signed overflow is prohibited. Deliberately reduced-width counters and very old retained handles are part of the Turn 3 adversarial specification. A future modulo proof must cover every internal queue counter, not only the public ticket.

## 6. Zero-copy fast path and payload lifetime

### 6.1 What zero-copy means here

A native producer obtains a writable span into the mapped payload block, serializes directly into it, and commits it. A native consumer obtains a read-only borrowed span and processes it in place. There is no intermediate transport buffer and no kernel payload copy.

Copying an already-built message into the reserved span using memcpy is still a payload copy. It can be an excellent convenience API, but it is not end-to-end zero-copy. Constructing a Python bytes object on receive is likewise a copy. Reports must distinguish direct construction, one-copy send, borrowed receive, and copied receive.

The producer must stop writing at commit. The consumer must stop reading before release. A generation check on the next API call does not retroactively make an already exported raw pointer safe. Borrow lifetime is a central ownership rule, not a wrapper detail.

### 6.2 Why fixed payload blocks come first

Variable message length up to B is allowed inside each fixed block. A compact variable-extent arena is deferred because it adds a second resource counter, fragmentation, wrap handling, and reclamation coupling.

A future contiguous variable-size profile must specify either two-span messages or a wrap-padding record; it must prevent a producer from consuming a descriptor without obtaining payload space, and vice versa. Reclamation must respect the actual extent ownership order. BipBuffer is useful prior art for contiguous regions; its spatial arrangement is not, by itself, a concurrency or crash-recovery proof. [S26]

Double-mapping an arena to make wrap appear contiguous is an optional OS-specific optimization, not a required portable baseline. It does not remove capacity or ownership constraints.

### 6.3 Fast-path exclusions

Mapping, size validation, page touching, registration, allocation, and timing calibration occur outside the polling fast path. Payload overwrite on full is forbidden. The baseline returns FULL or uses its explicitly selected wait policy; it never silently discards unread records.

Prefaulting and warmup reduce avoidable faults in measurements, but ordinary operating systems still do not provide a universal nanosecond scheduling guarantee. Control diagnostics, heartbeats, and recovery records are placed away from hot ownership fields.

## 7. Blocking without lost wake-ups

### 7.1 Separate queue ordering from wake-up ordering

A release-published payload remains correct whether a notification arrives, is spurious, or is delayed. Notification is a scheduling hint, not the memory fence that authorizes payload reads.

The incorrect sleep sequence is an empty check followed by an unconditional sleep. A producer can publish between them. The correct protocol must enroll the waiter, recheck the queue under the required ordering, and use a kernel compare-and-wait operation whose race with wake-up is defined. A producer must not decide “no wake needed” solely from a stale empty/full snapshot.

### 7.2 Linux adapter

The Linux baseline uses a dedicated aligned 32-bit word and the process-shared futex WAIT/WAKE operations, without FUTEX_PRIVATE_FLAG. Do not alias one half of a 64-bit queue cursor as the wait word. The futex comparison and entry into blocking are atomic with respect to the relevant futex operation; a value mismatch prevents the late sleeper from sleeping on an already changed condition. [S13, S14]

On return, the caller rechecks the actual queue predicate with its required acquire operation. EINTR, timeout, an expected-value mismatch, and spurious wakeups are normal protocol events, not evidence that a message exists. Absolute deadline semantics and timeout clock choice will be fixed in Turn 4.

### 7.3 Darwin adapter: use the public interface first

The preferred baseline requires macOS 14.4 or later and uses Apple's public os_sync_wait_on_address family, including a bounded-deadline/timeout variant, together with os_sync_wake_by_address_any/all. The shared-memory flag must be selected on **both** wait and wake, with matching width and backing location. Apple's header admits aligned 4- or 8-byte values; the cross-platform baseline chooses 4 bytes. These primitives do not provide priority-inversion avoidance. [S11]

Private __ulock interfaces expose both shared and private operations, but an exported/private implementation detail is not the same as a stable public application API. The project should not freeze hard-coded private syscall numbers or private operation assumptions when a public interface satisfies the requirement. [S12]

For older Darwin targets, a Mach semaphore adapter is a possible separately specified fallback. A semaphore is a kernel IPC object; the port name stored in one task is not automatically a valid capability in another. Rights transfer or bootstrap must be explicit, preferably handled by the existing application launcher. A dispatch_semaphore_t is an opaque process-local libdispatch object; copying its pointer or bytes into shared memory does not create a process-shared semaphore. [S16, S17]

Do not substitute std::atomic::wait/notify without an implementation-specific IPC audit. The current libc++ source uses process-private platform mechanisms in its normal implementation; a portable language API is not an interprocess guarantee. [S18]

### 7.4 A deliberately simple SPSC parking proof

Two immutable connection profiles are proposed.

**POLL_ONLY** has the minimal release/acquire queue path and no notification RMW. It is the profile aimed at the tightest native latency. An idle application may elect timed polling, but the profile does not promise immediate kernel wake-up after every publication.

**PARKABLE_SPSC** has one data waiter and a dedicated atomic state with AWAKE=0 and ARMED=1. This is a state-machine specification, not implementation code:

1. The consumer may bounded-spin. Before sleeping it performs an acquire-release RMW that sets ARMED, then acquire-rechecks publication. If data exists, it disarms and consumes. Otherwise it compare-waits for ARMED with a finite deadline.
2. After release-publication, the producer performs an acquire-release RMW setting AWAKE on every commit, even when the old value was already AWAKE. When it observes ARMED, it performs the platform wake.
3. Every wait-state mutation, including disarming, uses the specified acquire-release RMW after initialization. Every return from waiting rechecks the predicate. The sole consumer does not re-arm for another wait cycle without first leaving the preceding wait and observing the queue state.

There are two critical orderings in the wait word's RMW modification order. If publication's notification RMW precedes the consumer's arm RMW, the acquire/release chain carries the publication to the consumer's recheck. If arm precedes notification, the notification clears ARMED; the kernel either observes the changed value before blocking or the wake reaches the already blocked waiter. Any intervening state changes must preserve this chain. A same-value RMW still matters: it cannot be replaced by a load plus “skip if zero.”

This is a proposed proof skeleton to formalize in Turn 2. It deliberately trades an extra shared RMW per commit for an uncomplicated enrollment argument. Its performance is reported separately from POLL_ONLY; on targets where an RMW lowers to an exclusive retry loop, the expanded path must not inherit the load/store-only wait-free claim.

The symmetric space-wait protocol applies when the producer sleeps on a full queue. The single-waiter argument does not extend automatically to MPMC. Multiple waiters need their own enrollment, wake-one versus wake-all, capacity-predicate, fairness, and generation/ABA proof. Optimizing away the unconditional notification requires a new proof, not a branch added after a benchmark.

### 7.5 Notification crash gaps

A producer can die after publishing but before updating the wait word. It can also die after changing the word but before executing wake while a consumer is already blocked. There is no transaction coupling a userspace store and a kernel wake.

Therefore crash-aware blocking uses a finite maximum sleep interval and rechecks publication and lifecycle after timeout. This supplies eventual detection under a scheduling assumption, not sub-50-ns wake latency. Recovery and peer-death notification may shorten the delay, but the queue cannot promise to wake promptly on an instruction the dead producer never executed.

## 8. Crash resilience without unsafe reclamation

### 8.1 Failure model

The initial model includes process termination, descheduling, pause/resume, interrupted operations, and a recovery actor that can itself fail. It does not include persistence across power failure, malicious writers, arbitrary memory corruption, or exactly-once external side effects.

A timeout or heartbeat failure is suspicion, not proof that a writer can never resume. Revoking a generation token can reject a future commit, but it does not revoke the old process's already obtained writable mapping. Reusing those same bytes while that writer might resume permits corruption even if its final commit is rejected.

Safe reclamation requires confirmed process death or other effective fencing/quiescence. Linux pidfds are a useful identity-bound lifecycle mechanism; a PID alone is vulnerable to reuse. The registration protocol must establish identity before failure, not open an unrelated replacement process after noticing a stale numeric PID. An application launcher that owns and reaps children can provide an analogous lifecycle boundary. [S29]

### 8.2 SPSC recovery cases

**Producer dies before commit.** P has not advanced. The consumer cannot observe the partial record and may drain earlier published records. After the original producer is confirmed unable to write, a replacement can overwrite the next unpublished slot and resume from the committed frontier. Private reservation state did not poison shared publication.

**Producer dies after commit but before reporting success.** The record may already be delivered. The sender cannot infer non-delivery from the missing response. Applications requiring deduplication need message identities and an application-level policy.

**Consumer dies while borrowing.** C has not advanced, so the block remains retained. After effective fencing, policy chooses redelivery or explicit discard. Redelivery can duplicate side effects performed before the crash; reclaiming a queue slot does not roll back those side effects.

**Either endpoint remains alive but paused.** No timeout-based slot theft. The queue can report stalled health and preserve safety, or peers can migrate to a different physical generation under a defined lifecycle policy.

### 8.3 MPMC recovery cases

The classic claimed-ticket ring can leave a head-of-line hole. Worse, there may be a crash between obtaining a ticket and publishing an owner record. “Store PID next to the slot and reclaim it later” does not close that gap. Owner identity, generation, claim visibility, and cancellation must form a recoverable protocol, including recovery of a recovery actor.

A tombstone is a possible protocol element: after effective fencing and authoritative identification of the abandoned claim, recovery publishes ABORTED for that generation, and consumers advance over it without reading partial payload. This requires an idempotent recovery claim and a proof that no live writer can still own the bytes. It is not approved merely by naming a tombstone state.

For the proposed descriptor/pool architecture, a stalled builder need not block the ready queue, but a crashed builder can leak a pool block. A crash between dequeuing a free identifier and recording its lease remains a transfer gap to prove. Process-shared lock-free atomics alone do not solve crash-consistent ownership accounting.

### 8.4 Conservative approved recovery direction

Until in-place recovery is proved, the safe boundary is **generation replacement**. Stop admitting work to a failed generation, establish quiescence/fencing where possible, and attach healthy participants to a new, distinct shared-memory object. Do not reset or reuse the original physical pages while an old writer can access them.

The old generation is quarantined until it is safe to reclaim. Repeated failures must have bounded quarantine/resource policies, or admission must fail rather than consume unbounded memory. Migration must report uncertain/lost messages explicitly; it does not silently convert failure into successful delivery.

This recovery path may be coordinated by an existing process or launcher. No dedicated daemon is mandatory. Recovery progress is a separate service property; it is not smuggled into the healthy data-plane lock-free claim.

## 9. Negative boundaries

This is a shared-memory payload transport, not an AF_UNIX or named-pipe payload wrapper. Bootstrap may use OS capabilities or an existing application control plane, but data messages do not secretly travel through a socket fallback.

It is not a ticket-lock or spin-mutex queue, not a universal lock-free claim attached to any atomic increment, and not a full-fence-everywhere design. It has no mandatory external daemon, general allocator, garbage collector, Boost dependency, or framework runtime in the native fast path.

It is not a durable log, exactly-once transaction system, GPU/DMA coherence protocol, multicast bus by default, zero-overhead Python abstraction, or hard-real-time scheduler. A blocked slow path is allowed, but its properties are labeled separately from nonblocking try operations.

## 10. Turns 2–5: deliverables and kill gates

### Turn 2 — Ownership topology, formal ordering, and prior art

Produce byte-range ownership matrices for the admitted 64/128-byte profiles; publication and reclamation proofs; notification-enrollment proofs; a precise linearization/progress contract; and failure histories written as declarative event traces, not executable tests.

Dissect LMAX's sequencers, gating consumers, and wait strategies; DPDK's SP/SC versus MP/MC, RTS/HTS, and zero-copy start/finish restrictions; Aeron's claim/commit/abort and unblocking responsibilities; BipBuffer's contiguous-space guarantee; and SCQ's actual progress, memory-order, and wrap assumptions. Pin source revisions before adopting algorithm details. [S19–S26]

The goal is not five library summaries. It is a matrix showing which proof obligation each design actually solves and which requirement it changes. DPDK's documented zero-copy/peek modes are specifically restricted to SP/SC and HTS; serialized start/finish windows are not evidence of an arbitrary lock-free MPMC borrow API. [S21]

**Exit:** choose the exact MPMC algorithm or explicitly reject the global-FIFO profile; complete the SPSC and single-waiter proofs; leave no unsupported process-shared atomic assumption.

### Turn 3 — Four fatal corner cases and adversarial kill gate

**Split-cache-line tear:** construct malformed and incompatible alignment/stride cases. Atomic controls must be rejected before use if they can straddle forbidden boundaries. Multi-line payloads are not intrinsically defective when the ownership proof holds.

**Producer crash during reservation:** enumerate every claim, owner-record, payload-write, publication, wait-word, and wake boundary. Distinguish stopped from dead. Include consumer and recovery-actor failure, not only producer failure.

**Sequence wrap-around:** specify reduced-width histories, repeated slot laps, stale handles, near-half-range comparisons, and session replacement. No signed-overflow reasoning and no “64 bits makes it impossible” substitute for a lifetime assumption.

**Darwin scheduling / priority inversion:** specify a high-priority spinning consumer and a delayed producer, mixed performance/efficiency cores, competing load, and wake behavior. Public address waits do not supply priority-inversion avoidance; no hard deadline can be inferred from choosing a wait API. Placement controls must be verified on the actual platform, not described as Linux-style pinning by analogy. [S11, S28]

Additional kill histories cover a completed later enqueue behind an unpublished earlier claim, missed wake-up, notification crash gaps, full-capacity write leases, and a resumed old writer after alleged lease expiry.

**Exit:** PASS only for the supported model. Otherwise change the semantics explicitly or kill the candidate. No implementation has yet run; these are proof obligations and future fault-injection specifications.

### Turn 4 — ABI and header specification, without implementation

Produce field-width and byte-offset tables, padding rules, memory-order obligations for every operation, lifecycle and lease state machines, error semantics, platform admission checks, and the C/Python ownership contract. Include length/version validation, no-copy versus copy conveniences, cancellation, timeouts, generation errors, and disconnect behavior.

The deliverable is a normative schema and API specification, **not compilable C/C++/Rust headers or function bodies**. Library authors should be able to implement it after the gate without inventing missing semantics.

### Turn 5 — Benchmark preregistration and CFFI contract

Define the primary native experiment before producing numbers: one message outstanding, a fixed nominated payload such as 64 bytes, already attached/warmed mappings, both processes runnable, explicit topology, polling profile, and no Python in the measured interval.

The end-to-end interval starts before native reservation/direct payload construction and ends after the consumer's required payload reads/check. Publish-to-observe, consumer release, bidirectional request/response, and batch throughput are separately named metrics. An inverse throughput value is not one-way latency. RTT/2 is only a labeled proxy under additional symmetry assumptions, never a substituted direct measurement.

Use validated TSC measurement on x86-64, with compiler and instruction-ordering controls and a calibrated counter frequency. Plain RDTSC is not an automatically ordered interval boundary, and current CPU turbo frequency is not the timestamp-counter conversion. On macOS, use mach_absolute_time with its timebase conversion and measured granularity/overhead; do not equate raw ticks with nanoseconds or core cycles. Exact timestamp sequences are a Turn 5 specification obligation and a post-gate disassembly audit. [S27, S32]

Report p50, p90, p99, p99.9, maximum, sample count, raw observations, timer resolution, calibration uncertainty, and confidence intervals chosen before results. Preserve outliers rather than silently deleting scheduler interruptions. A median below 50 ns does not establish a p99 or worst-case deadline. A timer whose resolution is comparable to the target requires an honest measurement limit, not interpolated one-nanosecond histograms.

Measure payload sizes, ring capacities, polling versus parkable overhead, same/different CPU locality, performance/efficiency-core mixtures, x86 NUMA placement, sleeping wake-up, throughput, and crash recovery separately. Record exact OS/hardware/compiler/features and whether core placement was actually enforced or merely requested. Zero-contention does not mean same-thread loopback masquerading as IPC.

CFFI uses opaque connection and lease handles and the native C ABI. Blocking/batched native work must have a deliberate GIL policy. A borrowed buffer must retain both the mapped region and its slot lease. Retaining the mapping alone does not stop the ring from recycling its contents. Arbitrary exported buffer aliases cannot be revoked by flipping a Python flag; either track their lifetime before release, require an explicit unsafe-borrow discipline, or provide a copied safe API. Python performance is reported independently. [S30, S31]

**Exit:** approve the measurement protocol and binding lifetime model; only a subsequent Turn 6 authorizes implementation.

## 11. Turn 1 disposition

**Provisionally accepted:** fixed-slot, offset-addressed SPSC; two-sided release/acquire ownership; isolated cursor lines; native C ABI; no-copy direct construction/borrowing; explicit POLL_ONLY versus PARKABLE_SPSC modes; public shared-address waits on supported Darwin; shared futexes on Linux.

**Selected for proof, not yet certified:** SCQ-style bounded ready/free descriptor queues with a fixed payload pool; precise failed-operation linearizability; notification optimization; MPMC waiter coordination; crash-consistent local reclamation; modular counter reuse; exact compiler lowering.

**Rejected as unsupported claims:** ticket-lock “lock-free”; FAA alone as publication; timeout alone as safe raw-pointer revocation; volatile synchronization; universal M-series cache-line assertion; copying dispatch semaphore objects across processes; automatic interprocess std::atomic::wait; achieved or platform-independent sub-50-ns end-to-end latency.

The engineering objective is a small, auditable set of strong guarantees, not a larger set of incompatible guarantees hidden behind one fast benchmark.
