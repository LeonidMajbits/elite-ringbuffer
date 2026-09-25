# Turn 1 — Proof Obligations and Kill Gates

**Date:** September 23, 2026  
**Status:** Planning specification; none of the future implementation tests below has been run.  
**Companion:** `01_Turn_1_Architecture_and_Memory_Model.md`.

## 1. What has and has not been established

| Claim | Turn 1 status | Required evidence before release |
|---|---|---|
| Fixed-slot SPSC exclusive ownership | A two-direction happens-before argument is specified | Formal transition review, correct implementation, target disassembly, adversarial histories |
| Poll-only SPSC bounded try path | Accepted as an algorithmic design under admitted bounded load/store primitives | Operation-step accounting; no hidden loops, allocation, syscall, or atomic fallback |
| Cache-line isolation | Layout conditions are specified, not measured on Leon's machine | Validated offsets/strides, runtime target profile, hardware topology record |
| Shared-process atomics | Explicit platform requirement, not universal ISO portability | Supported compiler/OS ABI evidence and process-shared tests after Turn 6 |
| Parkable single-waiter notification | RMW-order proof skeleton proposed | Complete weak-memory proof including disarm, re-arm, timeout and close |
| MPMC global FIFO and lock-free progress | Candidate selected, not proved for this composition | Exact queue algorithm, memory-order derivation, ownership and capacity composition proof |
| Crash-safe local block reclamation | Not approved | Fencing plus recoverable ownership transfer, including every crash gap |
| Conservative recovery by new segment | Safety direction specified | Quiescence/quarantine, bounded resource policy, explicit delivery uncertainty |
| Less than 50 ns native end-to-end | Unmeasured target | Preregistered interval, target hardware, raw timing data and measurement uncertainty |
| Safe Python zero-copy borrow | Contract requirements identified | Exported-alias lifetime model; mapping and slot lease retained together |
| Implementation through Turn 5 | Forbidden | Turn 6 authorization |

## 2. Core invariants

**I01 — Identity.** Every live handle identifies a particular session and queue profile. A numerical slot index alone cannot authenticate ownership.

**I02 — Admission.** Unsupported atomic widths, alignment, byte order, offsets, lengths or mode combinations fail before operations on the untrusted layout.

**I03 — Initialization.** No participant performs an atomic operation on an object concurrently with its construction. READY follows complete initialization; abandoned bootstrap objects are discarded.

**I04 — Capacity.** SPSC published minus reclaimed count stays between zero and N. MPMC accounting includes free, writing, ready, reading, transfer-owned and quarantined resources without double counting.

**I05 — Exclusivity.** A payload block never has both a live writer and an authorized reader, and never has two independent owners of the same generation.

**I06 — Publication.** All required metadata and payload writes happen-before authorized reads. A reserved position is not automatically committed data.

**I07 — Recycling.** All authorized reads happen-before reuse of those bytes by a later writer.

**I08 — Borrow lifetime.** A slot cannot be reclaimed merely because a wrapper function returned while another exported view remains accessible.

**I09 — Sleep enrollment.** A live notifier and waiter cannot both complete their protocol decisions leaving a satisfiable predicate stranded behind an indefinite wait.

**I10 — Crash notification.** Publication and kernel wake are not atomic together. Bounded waiting or an independently proved failure-notification path covers death in that interval.

**I11 — Failure fencing.** Timeout does not authorize raw-memory reuse. An old process that can resume is still a possible writer.

**I12 — Counter identity.** Slot reuse and session replacement never make a stale token appear current. Any modulo comparison has an explicit range and suspension bound, or wrap is prohibited.

**I13 — Progress labeling.** Primitive lock-freedom, queue lock-freedom, individual wait-freedom, finite-capacity backpressure, and recovery liveness are distinct claims.

**I14 — Measurement identity.** The benchmark label states payload size, start/end events, queue/wait mode, topology, native versus Python path, timer and aggregation statistic.

## 3. Mandatory declarative histories

These are event-order specifications, not executable code, implementation tests, or measured results.

### K01 — Publication without payload ordering

Producer writes payload, exposes readiness, and consumer observes readiness. Consider the outcome where the consumer's authorized read still obtains an earlier generation. The correct release/acquire ownership chain must forbid that outcome under the selected language and hardware model. A volatile or relaxed-only readiness protocol fails this obligation.

### K02 — Premature recycling

Consumer observes readiness and begins reading a multi-line payload. Producer wraps around to the same block. The producer must be unable to overwrite the block until it has acquired reclamation covering the old read. This history kills a design that only proves the forward publication edge.

### K03 — Publication hole and completed later enqueue

The earliest producer claim stops indefinitely. A later producer publishes and returns success. A consumer starts after that return. Determine whether the algorithm can return a valid item, help/cancel safely, or return a semantically justified failure without waiting for the stopped process. A FIFO implementation that must await the earliest claimant fails the formal lock-free claim.

### K04 — Lost wake-up on two different words

Consumer observes empty. Producer publishes. Consumer advertises intent to sleep while producer decides that no waiter exists. Both may observe stale values if no synchronization orders the two decisions. The enrollment protocol must exclude indefinite stranding; ordinary payload release/acquire alone is insufficient.

### K05 — Wake-before-block

Consumer has armed and rechecked empty, but has not entered the kernel. Producer changes the wait word and wakes. Consumer then enters compare-and-wait. The kernel must observe the mismatch or an equivalent defined wake relationship; the consumer must not sleep forever because it missed an earlier edge.

### K06 — Death after publication or after wait-word change

Producer commits and dies before notification, or changes the wait word and dies before kernel wake. State the finite detection interval, timeout/predicate recheck, and peer-lifecycle behavior. No proof may assume execution of the producer's next instruction after death.

### K07 — Reservation / owner-record gap

A producer or allocator obtains exclusive shared ownership, then dies before recording the owner in a separate location. Recovery must locate and classify the resource without guessing from a stale PID or reclaiming a live peer's storage. A separate best-effort owner field does not by itself close this gap.

### K08 — Lease timeout and resumed raw writer

Recovery marks a timed-out lease invalid and gives the same payload bytes to another process. The old process resumes and writes through its already-held pointer. Reject the design unless the old writer was effectively fenced or the new generation uses different physical storage.

### K09 — Consumer death after external effect

A consumer reads, performs a non-idempotent external action, and dies before reclaiming the slot. Queue recovery cannot determine exactly-once completion solely from the cursor. Redelivery/discard and application deduplication policy must be explicit.

### K10 — Reduced-width wrap

Use a mathematically reduced counter domain in the specification. Retain an old handle across repeated ring laps and approach the comparison half-range. Prove rejection or enforce a quiescent stop before repetition. Include internal free/ready-queue counters and waiter epochs, not only payload tickets.

### K11 — Misaligned shared controls

Attach a layout whose atomic starts are nominally aligned but whose stride or extent causes two independent controls to share a line, or whose width straddles an unsupported boundary. Either reject attach or select a supported alternative profile. Payloads spanning lines remain legal if ownership is correct.

### K12 — Darwin producer starvation

A high-priority consumer repeatedly polls while the producer is delayed by scheduling, competing load, or unfavorable core placement. Show that the declared blocking policy can relinquish CPU and that the API makes no unsupported priority-inheritance or elapsed-time claim. Run the hardware experiment only after the implementation gate.

### K13 — All storage leased, no messages ready

Every payload block is reserved for writing or borrowed for reading, but the ready queue is empty. The public capacity semantics must permit storage exhaustion without misreporting N committed messages. Repeated FULL results do not prove a faulty queue if the specified resource is genuinely exhausted; they also do not establish useful application-level progress.

### K14 — Recovery actor failure and repeated generations

Recovery is interrupted while marking failure, migrating peers, or reclaiming old storage. Define an idempotent continuation and a bounded quarantine policy. A design that remains safe only by leaking an unbounded number of generations is not an accepted resilient bounded system.

## 4. Turn-by-turn evidence package

**Turn 2:** exact topology tables; acquire/release proofs in both directions; wait-state modification-order proof; linearization and failure-result definitions; algorithm/ABI assumptions; primary-source dissection with pinned revisions.

**Turn 3:** adversarial histories and their allowed/forbidden outcomes; counter and lifecycle assumptions; explicit pass/kill decision for the selected MPMC design. A model-checking plan may be specified, but no executable implementation is produced during the gate.

**Turn 4:** normative field and API schema; complete state transitions; operation memory orders; platform adapters; initialization and attachment ordering; lease and exported-buffer rules. No compilable headers.

**Turn 5:** benchmark protocol and statistical reporting; timestamp ordering and calibration specification; raw-data format; native/Python separation; deployment and compiler metadata requirements. No fabricated latency results.

## 5. Immediate design decisions

Proceed with fixed maximum payload blocks and relative offsets. Keep owner cursors and cached peer observations local. Separate control lines and slot extents. Do not include a notification RMW in a benchmark labeled as the minimal polling profile unless that cost is actually measured there.

Use public Darwin shared-address wait operations on admitted OS versions and shared futex operations on Linux. Do not copy opaque dispatch semaphore objects between processes, and do not assume a standard-library atomic-wait implementation is process-shared.

Reject ticket locks as the MPMC solution. Investigate a proven bounded descriptor queue with payload construction outside its FIFO publication window. Do not award the composed system a lock-free or crash-safe label until the queue, pool, leases, and recovery protocol have been analyzed together.

Counter wrap, raw-pointer revocation, sleeping, and Python alias lifetime are not cleanup work after the fast loop. They are gate conditions before implementation.
