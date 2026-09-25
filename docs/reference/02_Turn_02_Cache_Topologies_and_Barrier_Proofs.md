# Turn 02 — Cache Topologies, Barrier Proofs, and Prior Art

## Zero-Copy Lock-Free RingBuffer IPC — Leonid Majbits / Gemini Operator Lab

**Date:** 2026-09-23  
**Stage:** 2 of 5, architecture and proof gate  
**Status:** Normative protocol proposal and pen-and-paper arguments; no implementation, compiler probe, model-checking execution, or benchmark has been produced.  
**Destination:** Deployment Research / Elite_Systems_LockFree_RingBuffer  
**Predecessors:** Turn 1 architecture, proof obligations, and primary-source ledger supplied in this conversation.  
**Evidence convention:** [R01]–[R31] identify primary sources in §14. “Decision,” “derivation,” and “proof” identify this report’s engineering analysis rather than a source’s claims. Hardware measurements on Leon’s machines remain outstanding.

---

## 0. Decisions and corrections

The SPSC fast path remains a two-direction ownership transfer over ordinary shared CPU memory. For MPMC, select a precise **publish-first NCQ reference protocol with sequentially consistent queue metadata**, named **NCQ-SC64** in this document. Its FIFO publication is one atomic cycle/index replacement; advancing the tail afterward is helpable bookkeeping. Arbitrarily long payload construction happens before this publication operation, outside the FIFO.

This deliberately refines Turn 1’s provisional SCQ selection. SCQ remains the more elaborate scalability candidate, not an automatically certified optimization. The reference protocol provides a smaller, explicit proof surface and an independently useful correctness baseline. It is not a claim that NCQ beats SCQ under contention, nor a decision to abandon SCQ permanently. Both come from Nikolaev’s original work; the free/allocated-queue indirection is prior art, not a novelty claim for this project. [R14–R17]

Four premises in the directive must not enter the specification unchanged:

| Premise | Disposition |
|---|---|
| Every Apple M-series P-core has 128-byte L1/L2 lines while E-cores have 64-byte lines | Not established as a universal hardware statement. Apple’s pinned XNU header defines a 64-byte cache-line constant for several named P/E configurations, while other software interfaces report 128 bytes. Neither fact alone supplies a complete physical L1/L2/coherence geometry. A 128-byte software isolation profile is retained conditionally. |
| STLR/LDAR implement the RCpc pair | Incorrect terminology. LDAR is the stronger RCsc acquire; LDAPR is the RCpc acquire. Either can appear for a language-level acquire when the target/compiler mapping permits it. |
| EMPTY → RESERVED → COMMITTED → CONSUMED eliminates publication holes | False without a queue algorithm. These states describe payload ownership; they must not create unhelpable FIFO reservations. |
| VM_FLAGS_SUPERPAGE_SIZE_2MB is an Apple Silicon shared-mmap optimization | Rejected for the pinned XNU path: accepted superpage cases are inside an x86-64 conditional, and a caller-supplied VM object is rejected there. The symbol’s presence in a userspace header is not support for this use. |

These corrections are supported by Apple/Arm sources and the algorithms examined below. [R03–R06, R11–R16]

**Turn 2 outcome:** SPSC publication and recycling proofs are established under the declared platform model. The single-waiter parking argument is completed. A specific, conditional MPMC reference protocol is selected and proved at the abstract-operation level. Exact hardware geometry, actual compiler output, empirical latency, and in-place crash reclamation are not upgraded to verified facts.

---

## 1. Model and admitted guarantees

### 1.1 Language and operating-system boundary

The shared ABI has one authoritative native implementation, proposed in C11. C++20 clients call its C ABI; Python does not load, store, or reinterpret shared atomics itself. C++ memory-order names in this report are explanatory equivalents, not permission to overlay unrelated atomic representations on a C object.

The language documents describe release/acquire synchronization and recommend address-free lock-free atomics, including differently mapped shared memory. They do not make every compiler’s atomic representation, fallback library, construction semantics, or platform mapping suitable for IPC. C11 also explicitly disallows racing initialization with atomic access. [R01, R02]

**Required admission conditions:** ordinary coherent cacheable CPU mappings of the same backing object; compatible byte order and atomic ABI; suitable naturally aligned 32-bit and 64-bit address-free atomics; no hidden process-local lock fallback; one completed initializer before attachment; validated offsets and sizes; no concurrent truncation, reinitialization, or unmapping. No 128-bit atomic is required by NCQ-SC64.

The proof treats an admitted interprocess atomic operation like its corresponding operation in the language model. Proving that a particular compiler/OS tuple actually realizes this contract remains a deployment gate. A successful primitive lock-free query alone does not prove every part of the contract.

### 1.2 Failure and execution model

Participants may be preempted, paused, or terminated between operations. They are not malicious, do not write through released leases, and do not concurrently operate one endpoint in violation of its declared single-owner contract. Payload serialization can take arbitrary time; therefore serialization is not silently included in the queue’s lock-free progress theorem.

The fast-path generation has a finite, declared lifetime with no live counter wrap. The mathematical queue proof first uses nonnegative, unbounded tickets, then restricts execution to a representable, nonwrapping interval. Lifecycle retirement is outside steady-state queue progress. Ordinary OS scheduling, page faults, and finite-capacity backpressure do not have nanosecond deadlines supplied by this proof.

Only CPU-to-CPU ownership is covered. GPU, DMA, persistent-memory durability, power failure, and exactly-once application side effects require separate protocols.

### 1.3 Notation

- `sb`: sequenced-before inside an endpoint operation stream.
- `rf`: reads-from a particular atomic modification.
- `sw`: synchronizes-with.
- `hb`: happens-before, including the required transitive closure of sequencing and synchronization.
- `mo(X)`: modification order of atomic object X.
- `S`: the total order used for the reference queue’s sequentially consistent metadata operations.
- `LP`: linearization point, not the instant the caller receives its return value.

The central release/acquire rule is: a release modification and an acquire observation synchronize when the latter reads the appropriate released value or release sequence. Executing an acquire instruction somewhere in a function does not synchronize with an unrelated release. [R01, R02]

---

## 2. Cache-line topology: physical facts versus the ABI

### 2.1 The 128-byte claim needs qualification, not repetition

In XNU commit `f6217f891ac0bb64f3d375211650a4c1ff8ca1ea`, `osfmk/arm64/proc_reg.h` defines `MMU_CLINE` as 6, with comments identifying 64-byte lines, for Firestorm/Icestorm, Avalanche/Blizzard, and Everest/Sawtooth configurations. Its `cpuid.c` derives cache geometry from architectural registers separately by cluster type and retains boot-CPU values for a compatibility path. These are primary implementation facts, not a complete vendor microarchitecture specification for every shipping M-series part. A cache-maintenance step or compile-time constant can be smaller than a physical allocation/coherence granule; the constant alone does not prove that the silicon’s L1 line is 64 bytes. Conversely, a system-wide 128-byte query alone does not prove that every core’s L1 and L2 use that size. The exact P/E/L1/L2 table is therefore not certified by these sources. [R03, R04]

There is also a useful compiler-ABI warning: LLVM issue 182951 reports macOS’s 128-byte cache-line query alongside 64-byte compiler interference-size macros. That issue is closed in the retrieved record. Its historical mismatch is enough to reject a compiler-dependent interference constant as an unversioned shared-memory ABI; this report does not claim the old compiler behavior remains unfixed. [R05]

| Target or property | Established basis | Layout decision |
|---|---|---|
| Named Apple P/E families in pinned XNU | XNU defines a 64-byte cache-line constant; actual per-level physical geometry is not established by that constant | A=128 is conservative isolation for admitted 64/128-byte granularities |
| Apple L2/system-level coherence domains | Not completely enumerated by those L1 constants or one system query | Do not infer exact L2 geometry, inclusion policy, or ownership granule from A |
| Apple efficiency cores | Not automatically a separate A=64 IPC ABI when peers can run on P-cores or migrate | Use the same negotiated A=128 segment profile |
| Admitted x86-64 machines | A=64 is the proposed profile, requiring target validation rather than an ISA-wide cache-size theorem | A=64, or A=128 when a deliberately larger shared ABI is selected |
| Compiler alignment/interference constants | Compiler configuration and version can differ from OS reports | Explicit serialized layout identity; never silently rebuild an incompatible segment |

A boot log or microbenchmark from one M1 is evidence about that machine, not a universal M1-to-future-M-series specification. Record CPU model, OS build, relevant cluster topology, base page size, compiler target, and available cache information at admission and benchmarking.

### 2.2 Anatomy of the communication cost

A producer writes payload lines while it owns the slot. Publication transfers permission to read them; reclamation later transfers permission to overwrite them. The same coherence line may be in different cores’ caches over the lifecycle. Unified physical memory does not eliminate this ownership communication.

Distinguish three kinds of traffic:

**False sharing:** independent fields or slots share a coherence unit, so one participant’s writes disrupt another participant’s unrelated access.

**True sharing:** the consumer reads the producer’s publication cursor, producers contend on an MPMC tail or slot, or a payload is handed from writer to reader. Padding cannot remove the information exchange itself.

**Other locality effects:** prefetching, cache-set conflicts, shared-cache capacity, TLB misses, and core migration can affect latency even when the specified fields never occupy the same coherence line. The no-false-sharing proof does not claim to eliminate these effects.

P/P within a cluster, P/P across clusters, P/E, and E/E are different measurement conditions. x86 same-NUMA-node and cross-NUMA-node are likewise separate conditions. No exact coherence latency is assigned to any of them without measurement.

### 2.3 A checkable isolation theorem

Let the admitted coherence/invalidation granularities be the set G. Let A be a power of two such that every relevant granularity ℓ in G divides A. Let mapped pages preserve A-aligned physical offsets, and let each protected object occupy an integral number of A-byte cells starting at an A-aligned offset.

For a physical byte range X, define its line set at granularity ℓ as:

\[
\mathcal L_\ell(X)=\{\lfloor x/\ell\rfloor:x\in X\}.
\]

If two protected objects X and Y occupy disjoint A-cells, then:

\[
\forall\ell\in G:\quad\mathcal L_\ell(X)\cap\mathcal L_\ell(Y)=\varnothing.
\]

**Proof.** Every A-cell boundary is also an ℓ boundary because ℓ divides A. No ℓ-line crosses an A-cell boundary. Disjoint sets of complete A-cells therefore contain disjoint ℓ-lines. Mapping aliases do not change physical within-page offsets under the admitted page-alignment conditions. ∎

This is the precise guarantee. It applies only to the enumerated objects and admitted G. If the target has an incompatible or unknown larger relevant granule, the advertised guarantee is not established by a static assertion alone.

### 2.4 Future compile-time assertions, without writing a header

`alignas(128)` is a future declaration requirement for the Apple profile, not a substitute for checking the structure. The C11 implementation uses the corresponding alignment facility. Turn 4 must encode the following obligations as actual layout assertions after the planning gate:

| Object/property | Required assertion or admission rule |
|---|---|
| Cursor isolation cell | Alignment at least A; size exactly A in this profile |
| Publication and reclamation | Each offset is a multiple of A; occupied cell sets are disjoint |
| 64-bit atomic field | Exact admitted width/representation; natural alignment; no unsupported split-line/page placement |
| 32-bit wait word | Separate A-cell, matching width on wait/wake; never an aliased half of a cursor |
| Slot descriptor | Stride multiple of A; every indexed start preserves A alignment |
| Payload block | Start aligned to both A and payload requirements; stride multiple of both |
| Queue entry array | For strict isolation, each packed 64-bit entry occupies a distinct A-cell—not merely an aligned array base |
| Control/participant records | Independently written records separated; no counters inserted into another field’s padding |
| Segment bounds | Every offset, multiplication, extent, and rounding operation checked for overflow before dereference |
| ABI identity | Header version, offsets, strides, atomic ABI, A, and page assumptions match the attaching binary |

Packing a structure to remove padding is forbidden for atomic control layouts. `sizeof` alone is insufficient; member offsets and array stride matter. A compile-time assertion cannot certify an arbitrary runtime mapping or another process built with a different ABI.

### 2.5 Ownership-by-range matrix

| Range | SPSC writer | MPMC writer | Access and contention |
|---|---|---|---|
| Immutable header | Initializer only | Initializer only | Read after bootstrap; not a statistics area |
| Lifecycle cell | Designated lifecycle actor | Designated lifecycle actor | Atomic, separate from data-plane controls |
| Published P | Producer only | Not used as a universal MPMC commit cursor | Consumer read is true sharing |
| Reclaimed C | Consumer only | Not used as a universal MPMC reclaim cursor | Producer read is true sharing |
| QF/QR head | Not present | Competing dequeuers | True sharing among consumers of that internal queue |
| QF/QR tail | Not present | Enqueuers and helpers | True sharing; help must not touch another owner’s payload |
| Queue entry cell | Not present | Competing publication CAS operations across generations | Distinct cells isolated in strict profile |
| Block descriptor and payload | Current lease owner | Current unique token owner | Immutable to producer after publication; not reusable until last borrow ends |
| Wait state | Waiter/notifier RMWs in parkable mode | Separate MPMC proof required | Deliberate true sharing, not part of minimal polling cost |

Process-local working cursors, cached peer snapshots, and temporary ticket/index values remain local. Moving them into shared memory adds traffic without adding authority.

---

## 3. Exact proposed footprint and mapping policy

### 3.1 Four regions and the arithmetic

Use the Turn 1 logical organization: header, control block, slot/descriptor region, payload buffer. Let V be the actual base page size, A the negotiated isolation size, N the power-of-two payload-block count, and B the maximum payload length. V and A are compatible powers of two in this profile.

For the worked profiles, reserve one V-byte header page and one V-byte control page. The header’s proposed logical contents fit within 512 bytes. The control page must fit all mode-specific cells and admitted participant records; if it does not, expand it by whole pages and recompute all offsets. This is an exact footprint for the stated proposal, not a claim that Turn 4 field offsets are already frozen.

The proposed ordinary block descriptor contains a 64-bit generation/state word, 32-bit length, 32-bit type, 64-bit message identity, and 64 bits reserved for versioned metadata: 32 logical bytes, rounded to stride A. Payload stride is:

\[
S=\operatorname{roundup}(B,\max(A,\text{payload alignment})).
\]

For SPSC, no per-block atomic state machine is necessary: its descriptor can use the same space budget without adding MPMC atomics. Length and other metadata are published by P.

For strict-isolation NCQ-SC64, the slot region consists of N QF entries, N QR entries, and N block descriptors, each with stride A. The packed queue entry itself is only eight logical bytes. The rest of its cell is deliberate isolation, not payload capacity.

The control-page example admits K=16 participant records of A bytes. Five SPSC control cells or seven NCQ control cells—lifecycle, four queue cursors, and two reserved wait-mode cells—fit with these records in the assumed pages. The reserved MPMC wait cells do not authorize an unproved MPMC parking mode.

### 3.2 Worked example: N=1024, B=64

Assume payload alignment does not exceed A. All array extents below are page multiples, so no additional region-rounding term is needed.

| Profile | Exact shared mapping formula | x86 example: A=64, V=4096 | Apple example: A=128, V=16384 |
|---|---|---:|---:|
| SPSC, separate descriptor/payload | 2V + 2NA | 139,264 bytes = 136 KiB; 34 pages | 294,912 bytes = 288 KiB; 18 pages |
| NCQ-SC64 strict per-entry isolation | 2V + 4NA | 270,336 bytes = 264 KiB; 66 pages | 557,056 bytes = 544 KiB; 34 pages |
| SCQ candidate, two physical 2N-entry queues, strict isolation | 2V + 6NA | 401,408 bytes = 392 KiB; 98 pages | 819,200 bytes = 800 KiB; 50 pages |
| NCQ compact queue arrays, isolated block descriptors/payloads | 2V + 16N + 2NA | 155,648 bytes = 152 KiB; 38 pages | 311,296 bytes = 304 KiB; 19 pages |
| SCQ compact queue arrays, isolated block descriptors/payloads | 2V + 32N + 2NA | 172,032 bytes = 168 KiB; 42 pages | 327,680 bytes = 320 KiB; 20 pages |

**These numbers are arithmetic, not measurements.** They exclude process-local state, kernel page tables, mapping objects, and recovery quarantine. The Apple 16-KiB and x86 4-KiB values are explicit example inputs, not assumptions to bake into all deployments.

The compact rows do not provide the strict no-false-sharing guarantee for distinct queue entries. A permutation that spreads adjacent ticket indices across lines can reduce contention, but cannot prove that arbitrary concurrently active indices never share a dense line. The original NCQ/SCQ cache remapping is such a mitigation, not universal isolation. [R14–R16]

The costs are visible: an eight-byte queue word consumes 64 or 128 bytes under strict isolation. That can be rational for a small control ring and wasteful for a large high-throughput array. Do not claim maximum density and complete per-entry isolation simultaneously.

### 3.3 Inline metadata is a separate experiment

For small SPSC messages, a 32-byte descriptor and a 64-byte payload could occupy a single 128-byte slot. An inline-slot experiment has mapping size 2V + 128N for this example: 136 KiB with V=4096 and 160 KiB with V=16384.

This may reduce distinct metadata/payload line accesses relative to the separate Apple-profile arrays. It changes the physical format and needs a distinct ABI/layout identifier. No measured advantage is claimed. The initial four-region proposal remains the reference format for this turn.

### 3.4 Mapping, page residency, and TLB reach

The normal baseline is one shared backing object, sized and initialized once, mapped with MAP_SHARED in each process. Relative offsets let virtual bases differ. Two independently created anonymous mappings are not the same shared backing object merely because both callers used MAP_SHARED. File offsets must satisfy the platform’s page-alignment rules. [R10]

For hot virtual extent W and page size V, the basic page count is approximately ceiling(W/V), with boundary effects determined by layout. Each process still has its own address translation context. Fewer mapped pages does not mean shared TLB entries across processes.

Initialization may pre-touch pages and establish the intended NUMA placement on Linux before timing begins. That is setup work, not a per-message operation. Prefaulting is not a hard guarantee against all later scheduling or memory-management interruptions. Neither msync nor a cache clean belongs in the CPU publication path.

### 3.5 Linux huge-page option

Explicit HugeTLB requires a compatible backing mechanism, provisioned pages and permissions. A hugetlbfs object or `memfd_create` with the HugeTLB option is a relevant route; attaching MAP_HUGETLB to an arbitrary ordinary POSIX-shm object is not a general conversion operation. The requested huge-page size must be supported. [R07, R08]

Transparent Huge Pages are a different mechanism with different policies, including shmem-specific policy. A hint or a large aligned virtual allocation is not proof of the effective page size. Record the mapping evidence, such as the relevant process mapping information, rather than declaring success from a requested flag. [R09]

Huge pages are optional and separately benchmarked. A small warmed ring may already fit its translation working set. Allocating megabytes to reduce a few dozen translations can increase waste or startup failure without lowering the critical coherence cost. Failure to obtain huge pages must either select a declared base-page profile or fail admission—not silently produce mislabeled results.

### 3.6 Darwin: kill the proposed 2-MiB shortcut

Pinned XNU `vm_map.c` rejects a superpage request when the caller supplies a VM object. Its recognized ANY/2MB cases are guarded by `__x86_64__`; the other path returns KERN_INVALID_ARGUMENT. It also assigns non-inheritance to that superpage mapping path. [R06]

Therefore this project does not specify VM_FLAGS_SUPERPAGE_SIZE_2MB as an Apple Silicon POSIX-shm optimization. Use normal shared mappings with the actual page size. This conclusion is about the examined public request path, not a claim that Apple hardware can never use larger translations internally.

---

## 4. SPSC: complete ownership and happens-before proof

### 4.1 State and invariant

P counts fully published messages; C counts fully reclaimed messages. Only the producer modifies P and only the consumer modifies C. Ticket k uses physical slot k mod N. A reservation is private: it does not increment P. A read borrow retains its slot until its final access has ended.

The central invariant is:

\[
0\le P-C\le N.
\]

Let ĉ be a producer’s previously acquired reclamation snapshot and p̂ a consumer’s previously acquired publication snapshot. Without wrap and without role handover, these values cannot authorize future work beyond the corresponding actual progress. They can be conservative.

### 4.2 Capacity and authorization induction

Initially P=C=0 and no slot is owned by a consumer.

A producer may publish ticket P only after its reservation was admitted with P−ĉ<N. Since ĉ≤C, P−C≤P−ĉ<N, so incrementing P preserves P−C≤N. It never relies on a speculative future reclamation value.

A consumer may borrow ticket C only when C<p̂. Since p̂≤P, C<P, so releasing that ticket cannot make C exceed P. It does not read an unpublished ticket.

Induction over publication and reclamation events preserves the invariant. Borrowing without reclaiming does not change C and therefore does not free capacity prematurely. The same argument covers a slow reader and a producer that has lapped around the physical array.

### 4.3 Forward publication theorem

For message k, let Wk denote any required payload or descriptor write. Let SP(v) be a producer release-store of P=v, where v>k, and LP(v) a consumer acquire-load that reads that modification. The consumer authorizes Rk only from such a covering acquired snapshot.

Because the single producer finished all writes for k before the covering publication:

\[
W_k\to_{sb}S_P(v).
\]

The observed release/acquire pair supplies:

\[
S_P(v)\to_{sw}L_P(v).
\]

The authorized consumer access follows the acquire:

\[
L_P(v)\to_{sb}R_k.
\]

Thus, by transitivity:

\[
\boxed{W_k\to_{hb}R_k.}
\]

A later acquired P can cover earlier messages because those writes were sequenced before that later release. No release-sequence trick across different cursor objects is needed. A load of the initial P value does not authorize ticket zero. [R01, R02]

### 4.4 Reverse recycling theorem

Let Rk,last be the last access through every valid consumer borrow of message k. Let SC(u) be a release-store of C=u, u>k, and LC(u) a producer acquire observation covering reclamation of k. The next physical reuse is ticket k+N.

\[
R_{k,\mathrm{last}}\to_{sb}S_C(u)
\to_{sw}L_C(u)
\to_{sb}W_{k+N,\mathrm{first}}.
\]

Therefore:

\[
\boxed{R_{k,\mathrm{last}}\to_{hb}W_{k+N,\mathrm{first}}.}
\]

Together with the forward theorem, this orders every conflicting payload access across ownership generations. Ordinary multi-line payloads need not be atomic. A generation check performed after an unauthorized read cannot substitute for this reverse edge.

### 4.5 API linearization and failed observations

Successful publication linearizes at its release-store of P. Reclamation linearizes as a capacity transfer at its release-store of C. Borrow acquisition is a single-consumer ownership event; it does not imply that physical capacity has been released.

The minimal polling API’s failed observations are explicitly **NO_DATA_OBSERVED** and **NO_CAPACITY_OBSERVED**, or equivalently documented transient EMPTY/FULL outcomes. Acquire loads do not promise a wall-clock-newest snapshot. A strict failed-operation oracle with no transient failure must be specified and proved separately; it must not be inferred from the successful publication proof.

This distinction is particularly useful when comparing against a fully linearizable internal MPMC queue: payload visibility, successful FIFO delivery, physical lease occupancy, and the semantics of a failed try-call are different obligations.

### 4.6 Progress and batching

The minimal SPSC try-operation has bounded own algorithmic steps when its ordinary loads/stores and admitted atomic operations have the required bounded-step behavior. It has no CAS retry loop, allocation, or syscall. User serialization is bounded only when the caller’s payload-work contract is bounded. Waiting until success is a different operation.

A batch may publish a completely constructed contiguous prefix or reclaim a completely finished contiguous prefix. The same covering-release proof applies. Batching can amortize cursor communication, but holding the first finished record until the batch is complete can increase its latency. Neither throughput nor inverse throughput is a substitute for that message’s handoff latency.

---

## 5. ARM64 versus x86: exact semantic distinctions

### 5.1 Volatile is not a synchronization protocol

Volatile does not establish the inter-thread/interprocess release/acquire relation required here. A cache-coherent machine can still expose cross-location observations in an order that violates a naive payload-then-flag assumption; the language also requires a valid synchronization relation for conflicting ordinary accesses. x86’s stronger hardware behavior can hide a bug without repairing the language-level program. [R01, R11–R13]

### 5.2 RCsc, RCpc, and language acquire

LDAR is the stronger RCsc load-acquire. LDAPR implements RCpc acquire. Arm documents eligible compiler mappings from C/C++ acquire to LDAPR, including target-dependent support in GCC 13.1 and LLVM 16. In particular, LDAPR can pass an earlier STLR to an unrelated address where LDAR imposes a stronger constraint. That extra freedom is legal for the corresponding language operations. [R11]

Consequences for this design:

- A correct SPSC release/acquire handoff must remain correct with a legal LDAPR acquire mapping. It cannot depend on LDAR accidentally ordering unrelated metadata.
- RCsc in the hardware instruction’s name does not turn a source `memory_order_acquire` operation into a C++ sequentially consistent operation.
- The NCQ-SC64 reference deliberately uses source-level sequential consistency for its queue metadata. It does not obtain a total order by inspecting mnemonics and guessing what the programmer intended.

### 5.3 Documented mapping, not fabricated disassembly

| Source operation | Typical admitted AArch64 mapping | Typical admitted x86-64 mapping |
|---|---|---|
| Relaxed load / store | LDR / STR | MOV / MOV |
| Acquire load | LDAR, or eligible LDAPR family lowering | MOV with compiler ordering |
| Release store | STLR | MOV with compiler ordering |
| Sequentially consistent load / store | Baseline LDAR / STLR mapping | MOV load; commonly locked XCHG or equivalent ordered store mapping |
| Acquire thread fence | DMB ISHLD | Usually compiler ordering, no fence instruction |
| Release or acquire-release thread fence | DMB ISH | Usually compiler ordering, no fence instruction |
| Sequentially consistent thread fence | DMB ISH | MFENCE or an appropriate locked operation |
| Ordered compare/exchange | LSE CASAL-family or appropriate exclusive-load/store sequence | Locked CMPXCHG |
| Ordered exchange/fetch-add | Suitable LSE operation or exclusive retry sequence | XCHG / locked XADD as appropriate |

The table is a mapping expectation, not a binary audit. Exact instruction selection depends on width, target features, compiler version, optimization, LTO, and outlined atomic helpers. The compiler’s permitted lowering of compare/exchange failure paths matters too. [R11, R13, R30]

Do not call DMB ISH a full-system fence. ISH names the inner-shareable domain; SY is the full-system domain. DMB orders relevant memory observations; it is not a promise that payloads have been written back to DRAM. Nor is an acquire a general speculative-execution security barrier. [R31]

No additional DMB is needed merely because the SPSC payload occupies multiple cache lines. The release/acquire handoff is the synchronization mechanism. Adding DMB around every field would not repair a wrong ownership protocol and would obscure the intended cost model.

### 5.4 A real prior-art warning: DPDK and RCpc

Arm’s analysis of a DPDK ring failure shows why a weaker legal acquire mapping can expose an invalid cross-variable assumption previously hidden by LDAR. The issue involves inconsistent cursor observations and capacity arithmetic; acquire fences without a relevant released value do not create the missing synchronization. The article discusses stronger consistent ordering, proper synchronization chains, and validation of inconsistent observations as distinct remedies. [R12]

This report does not label every current DPDK release broken. The lesson is architectural: a proof must cover the source memory model, not just a fortuitously stronger instruction sequence produced by an older compiler.

### 5.5 Post-gate assembly admission checklist

The later binary audit must record the native target, architecture features, every shared atomic width/alignment, actual load/store/RMW lowering, compare/exchange failure order, and any out-of-line helper. Reject per-process lock fallbacks. Check that payload stores precede publication and payload reads precede reclamation in the compiled contract. Audit the polling and parkable profiles separately.

No compilation or probe is performed in Turn 2. The proof target is now specific enough that the future audit can falsify it rather than merely print reassuring assembly.

---

## 6. MPMC: state names are insufficient; publish-first is the mechanism

### 6.1 Counterexample to the requested state-only proof

Suppose producer A reserves FIFO ticket t and marks its descriptor RESERVED, then stops. Producer B reserves t+1, fully writes it, marks COMMITTED, and returns success. A consumer starts afterward.

A consumer that insists on ticket t cannot read A’s incomplete payload. Waiting for A violates the intended nonblocking progress property. Returning an ordinary strict EMPTY despite B’s completed operation is not a correct empty result for the intended FIFO. Changing B’s state from COMMITTED to some more emphatic name changes none of this.

Therefore the proposition “four atomic descriptor states eliminate the publication hole” is false. This remains a counterexample even when every state access is sequentially consistent. Ordering prevents some visibility bugs; it does not manufacture a missing completed message.

### 6.2 Separate three identities

A correct design distinguishes:

**Payload block identity:** a stable index in the fixed pool, plus segment/session and lease generation.

**Queue ticket:** a logical FIFO position used only by an internal index queue. A block’s index is not its queue ticket.

**Payload phase:** EMPTY, RESERVED, COMMITTED, or CONSUMED for the current generation. This phase is not by itself proof of membership in QF or QR.

A writer may own a block for a long time without owning a ready-queue ticket. Only after payload construction ends does it attempt the atomic ready-queue publication. This indirection/two-queue architecture is explicitly present in Nikolaev’s original work. [R14, R17]

### 6.3 Selected exact reference: NCQ-SC64

The reference is the NCQ algorithm of §4/Figure 5 of Nikolaev’s paper, with the following explicit project choices. The author’s pinned `lfring_naive.h` is the source comparison, not blindly adopted binary code. [R14, R15]

| Property | NCQ-SC64 requirement |
|---|---|
| Internal queues | QF distributes free block indices; QR distributes completed-message block indices |
| Queue capacity | N≥2, with N a power of two; active participant bound K≤N |
| Entry | One atomic 64-bit word packing a cycle and a block index; no pointer or double-width CAS |
| Atomics | All queue head, tail, and entry operations use sequential consistency in this reference |
| CAS | Strong compare/exchange for the abstract progress proof; target primitive progress is an admission assumption |
| Array mapping | Identity index mapping with strict A-byte entry stride for the reference; a fixed bijection could preserve correctness but is not needed here |
| Enqueue admission | The caller owns one unique pool token not currently in that queue; enqueue is never invoked into a genuinely full internal queue |
| Publication order | Entry installation first; tail advancement afterward and helpable |
| Consumer ownership | Successful head CAS transfers exactly one index; payload access occurs only after that success |
| Lifetime | No live ticket/cycle wrap; no in-place reset during operation |

The pinned source uses weaker acquire/acquire-release operations and its own cache remapping. NCQ-SC64 deliberately strengthens the queue-metadata orders and specifies strict entry isolation. This is a named reference variant, not a claim that the unmodified source was formally proved for our IPC ABI.

### 6.4 Mathematical queue state

Let H and T be an internal queue’s head and tail hints. Define cycle(t)=floor(t/N), position(t)=t mod N. Each entry atomically stores a pair (cycle, block index).

For an initially empty QR: H=T=N and every entry has cycle zero. For an initially full QF: H=0, T=N, and entry i has cycle zero and block index i.

Introduce a ghost quantity U: one beyond the last contiguously published ticket. Ghost means mathematical proof state; it is not another hot shared atomic. Initially U=N for both queues, with H distinguishing empty from full.

The central invariants are:

\[
H\le U,\qquad 0\le U-H\le N,\qquad U-1\le T\le U.
\]

The last inequality says that tail may lag one published entry, but does not reserve a future unpublished entry. H can briefly be one greater than T if a consumer claims the newly published entry before tail bookkeeping finishes. The incorrect invariant H≤T must not be introduced into this proof.

### 6.5 Publication transitions and the enqueue LP

An enqueuer examines ticket t obtained from T and the entry at position(t).

If the entry already has cycle(t), that ticket has already been published. Any enqueuer may attempt to advance T from t to t+1, then reconsider the current tail. No payload construction is needed for that helping step.

If the entry is exactly one cycle older, the caller may attempt to atomically replace the complete entry word with (cycle(t), its block index). The successful replacement is the enqueue LP. Its payload was completed before this attempt. The enqueuer subsequently attempts to advance T; failure of that final tail CAS is harmless when another participant has already helped.

Other cycle mismatches indicate a stale observation and cause reconsideration. They do not authorize overwriting an arbitrary newer entry.

**Lemma M1 — No unpublished reservation gap.** T advances past t only after ticket t has an installed entry. Publication at t+1 therefore cannot precede installation at t. Immediately after installation at t, the entry itself is already consumable even if its publisher stops before changing T. Thus published tickets are contiguous and tail assistance never requires the stopped publisher to resume. ∎

**Lemma M2 — At most one lagging tail step.** A publication at T=t raises U from t to t+1. Further publication cannot occur at t+1 until some enqueuer advances T. Once T reaches t+1 it again equals U. Hence T∈{U−1,U}. ∎

### 6.6 Safe physical-slot reuse

The queue entries are index records, not the payload blocks themselves. Replacing an old queue entry is safe only after that ticket has been claimed from the queue.

A caller entering an internal enqueue owns a unique token outside that queue. Until its own publication LP, that token cannot be counted among the queue’s live members. Consequently that queue cannot already contain all N distinct pool tokens. Multiple concurrent enqueuers each hold a distinct outside token, so the same argument applies collectively.

At a publication LP t=U, at most N−1 live members precede the insertion. Thus U−H≤N−1, and t−N<H. The prior use of position(t) has already been claimed. Replacing its old atomic index record cannot overwrite a live unclaimed queue item.

A stale consumer may still retain a locally read old index, but it cannot obtain ownership with its old head expectation after another consumer has advanced H. It must not access the payload before winning the head CAS. No hazard-pointer reclamation is required for the fixed index array because the array itself remains mapped and alive.

### 6.7 Consumer transition, empty result, and FIFO proof

A consumer reads H=h and then the complete atomic entry at position(h).

If the entry’s cycle equals cycle(h), the consumer attempts to advance H from h to h+1. The successful head CAS is the dequeue LP; only its winner owns the locally captured block index. A loser discards its observation and retries without reading the payload.

If the observed entry is exactly one cycle older, the queue is empty at that entry observation in the reference sequentially consistent history. A future-cycle mismatch means the saved head is stale and requires a retry, not EMPTY.

**Lemma M3 — Unique dequeue.** For each h, at most one CAS changes H=h to h+1. No second consumer can claim that ticket. ∎

**Lemma M4 — Empty is justified.** Under the SC metadata order, if the entry is one cycle older than h, publication of ticket h has not occurred. H cannot already have advanced past h while that entry is still one cycle old, since advancing past h requires observing its installed generation. By M1 no later ticket is published without h. Thus U=H=h at the observation, and it is a valid EMPTY LP. ∎

**Theorem M5 — Internal FIFO linearizability.** Successful entry installations are ordered by increasing ticket, by M1. Successful removals are ordered by increasing head, by M3. M4 justifies failed empty removals. Assign enqueue LPs to entry installation and successful dequeue LPs to head advancement; the resulting sequential history is a FIFO history consistent with the operations’ intervals under the admitted atomic execution model. ∎

This FIFO orders message publication, not the times at which producers obtained their payload blocks. Consumers may finish processing in a different order from dequeue LPs. Applications requiring reservation-order processing or ordered external side effects need additional semantics.

### 6.8 Progress proof, including a stopped publisher

Assume a finite participant set, no wrap/reset, valid token admission, and the stated atomic progress model.

If an enqueue’s publication CAS fails, a conflicting modification has occurred. With generation repetition excluded, it cannot fail forever against an unchanged expected word. Repeated successful competing publications constitute progress. If the enqueuer observes a matching current generation but a lagging tail, it can help T advance itself; no unique owner holds that right.

If no operations completed after some point, the finite set of pending operations could perform only finitely many successful publication LPs before exhausting those pending enqueues. Each such LP leaves at most one helpable tail step. After those finitely many steps, a still-running properly admitted enqueuer encounters either a successful publication opportunity or evidence of another publication. It cannot be blocked by an uncompleted payload associated with the tail, because no such reservation state exists in this queue.

A dequeue similarly either observes a valid empty state, succeeds at the head CAS, or encounters evidence of competing head advancement or newer entry generations. Infinite head-CAS failures require continuing removals; repeated stale generations require queue progress. A stopped consumer that already advanced H holds a payload token, not an unclaimed FIFO head position.

Therefore the internal queues are **lock-free under these assumptions**. Individual starvation is possible, so they are not wait-free. Nothing in this proof bounds elapsed OS scheduling time or provides a 50-ns MPMC deadline.

### 6.9 The four payload states, precisely interpreted

Use a single admitted 64-bit generation/state word per block. Two phase bits and a nonwrapping generation field are sufficient for this reference’s phase representation; session identity remains in the segment/handle. Generation must not be inferred from a bare block index.

| Phase | Meaning | Who may access payload? |
|---|---|---|
| EMPTY(g) | No payload borrow remains for generation g; token may still be in the free-return transfer before QF publication | No reader; next writer only after successful QF dequeue |
| RESERVED(g+1) | One producer owns the token and may construct the next payload | That producer only |
| COMMITTED(g+1) | Payload frozen; it may be awaiting QR insertion or already published | No producer writes; a consumer only after successful QR dequeue |
| CONSUMED(g+1) | One consumer has claimed the message and may still hold its borrow | That consumer’s admitted views; not the next writer |

Normal lifecycle is EMPTY → RESERVED → COMMITTED → CONSUMED → EMPTY. A producer may abort from RESERVED, end every writable alias, mark EMPTY, and return the token through QF. Concurrent cancellation during a commit is not supported. Once COMMITTED has begun the publication operation, the caller cannot independently recycle the token because it has not yet received success.

These state labels are validation and ownership metadata. They are not a standalone recovery oracle. In particular, COMMITTED does not mean “definitely in QR,” and EMPTY does not mean “definitely already available in QF.”

### 6.10 Composition: publication and recycling happens-before

Let b,g identify a payload block and generation. All writes finish before its COMMITTED release transition and before QR’s publication CAS. A successful consumer obtains b through an acquire observation of that queue entry followed by its successful head CAS.

\[
W_{b,g}\to_{sb}\mathrm{COMMITTED}_{rel}(b,g)
\to_{sb}\mathrm{publish}_{QR,SC}(b)
\to_{sw}\mathrm{observe}_{QR,SC}(b)
\to_{sb}\mathrm{claim}_{QR,SC}(b)
\to_{sb}R_{b,g}.
\]

The SC queue publication is a release operation, and its observed SC load is an acquire. Therefore all required writes happen-before the authorized payload reads.

For reuse, the last consumer access occurs before marking EMPTY and before publishing b into QF. The next producer acquires b through QF before writing its next generation:

\[
R_{b,g,\mathrm{last}}\to_{sb}\mathrm{EMPTY}_{rel}(b,g)
\to_{sb}\mathrm{publish}_{QF,SC}(b)
\to_{sw}\mathrm{observe}_{QF,SC}(b)
\to_{sb}\mathrm{claim}_{QF,SC}(b)
\to_{sb}W_{b,g+1,\mathrm{first}}.
\]

Hence both directions of payload ownership are synchronized. State-word checks must agree with that ownership; a mismatch is an integrity/lifecycle error, not a reason to spin on another producer’s RESERVED block at the FIFO head.

**Important lifetime rule:** after a queue publication LP, a caller may update only the permitted queue bookkeeping. It must not touch transferred payload or mutable descriptor fields, even if its own enqueue function has not returned yet. Another process can already claim, process, return, and reuse that block. This applies symmetrically to returning a block to QF.

### 6.11 Conservation and bounded capacity

Each of the N unique block tokens belongs to exactly one disjoint logical category: in QF; owned for construction; in QR; owned by a consumer; transfer-owned around an operation; or quarantined. Define transfers at the appropriate queue LP, not by reading a best-effort owner field.

Initialization places all tokens in QF. A successful dequeue transfers one token out of a queue; a publication transfers the caller’s one token into a queue. Aborting returns that same token once. No transition duplicates a token, and an outstanding read/write lease retains its token. Induction establishes:

\[
N=|F|+|W|+|Q|+|R|+|T|+|X|,
\]

with disjoint sets and no double counting. Physical entry words left behind from old cycles are historical records, not additional live tokens.

As a result, a ready queue can be empty while the payload pool has no available token. A failed reserve means no free storage token at its internal QF observation, not “N committed messages exist.” A consumer holding a Python buffer still owns its token until every admitted alias ends.

The finite native commit/release sequences compose the lock-free index queues with bounded local phase work. An application that reserves every payload and then stops constructing has exhausted a finite resource; the ready queue does not acquire a hidden head-of-line lock, but useful production cannot continue without storage. This resource boundary must remain visible in the public contract.

### 6.12 Crash boundary: what is proved and what is not

A stopped producer before QR publication holds a block outside QR. Healthy ready messages can proceed. A stopped producer after the publication CAS leaves a readable descriptor and a helpable tail step. These histories no longer create the original publication hole.

A crashed process can nevertheless leak a token, including death after QF dequeue but before separately recording an owner. A consumer can die after an external side effect but before returning its block. Queue lock-freedom does not resolve either uncertainty.

In-place timeout reclamation is still rejected. A timed-out but resumable writer can corrupt reused bytes through an existing pointer even when a later API generation check rejects its commit. Retain the Turn 1 recovery direction: effective fencing or a physically distinct new segment, bounded quarantine, and explicit delivery uncertainty. The current proof does not certify a crash-consistent ownership registry, exactly-once recovery, or MPMC wake-up coordination.

---

## 7. SCQ: why it remains valuable, and why it is not interchangeable

### 7.1 The actual mechanism

SCQ’s single-width bounded form uses twice as many physical queue entries as the nominal n-token capacity, cycle/safety/index information, FAA ticket allocation, invalidation of unfilled positions, tail catch-up, and a bounded failed-dequeue budget. The paper derives the 3n−1 threshold for its relevant form. These are coupled elements of the algorithm; selecting FAA and omitting the rest is not SCQ. The presentation assumes an SC model and k≤n participants. [R14, R16]

The mechanism differs from NCQ: SCQ may retire an unfilled ticket so consumers need not wait for its claimant; the delayed producer then retries at another admissible position. The threshold is important because indiscriminate empty-ticket invalidation can otherwise sustain a livelock against active producers.

### 7.2 A subtle linearization trap

Consider overlapping enqueues A and B. A obtains an earlier SCQ ticket and pauses. B obtains a later ticket, inserts, and completes. Before any consumer invalidates A’s earlier ticket, A inserts there. A subsequent consumer can take A before B.

This history can be linearizable because the two enqueues overlapped. However, assigning **every enqueue LP to its eventual slot-insertion CAS** would incorrectly place B before A. The necessary linearization reasoning is history-dependent; it cannot be copied from the simpler publish-first NCQ argument.

Conversely, if a consumer invalidates A’s unfilled ticket before A arrives, A must not resurrect it. It retries, and the order can differ. That invalidation-versus-insertion race and the associated cycle conditions are central proof obligations.

This is why “completed payload plus a sequence number” is not a complete SCQ proof. Completed payload removes user construction from the queue’s publication interval; it does not remove SCQ’s own internal ticket races.

### 7.3 Source-versus-paper findings

The pinned author implementation has additional engineering choices beyond the paper presentation: transformed packed indices, acquire/acquire-release atomics, threshold accesses with their specified orders, and a bounded retry-before-invalidation path allowing 10,000 repeated observations in a particular dequeue case. That last policy does not make the algorithm a mutex, but it is highly relevant to latency and preemption experiments. It cannot be ignored in a claim about a tiny fast-path instruction count. [R16]

The repository distinguishes SCQ, SCQD indirection, SCQ2 double-width storage, and wCQ. Its wCQ implementation uses double-width CAS, so it is not a drop-in solution to the present single-width atomic admission contract. Code is dual-licensed under BSD-2-Clause/MIT according to the author’s README; preserve attribution and inspect the actual selected files before reuse. [R17]

### 7.4 Decision

Retain the exact §5/Figure 8 single-width SCQ form as the scalability research candidate. Do not substitute SCQ2, wCQ, an unbounded linked extension, or an ad hoc FAA loop under the same name.

NCQ-SC64 is the Turn 2 reference because its publication LP and helping rule admit the explicit proof above. To promote an optimized SCQ profile, later gates must independently accept its linearization argument, weak-memory refinement, threshold/livelock bound, finite-width comparison rules, IPC composition, and actual compiled primitive behavior. This is an evidence gate, not a claim that the published SCQ algorithm is invalid.

---

## 8. Completing the single-waiter parking proof

The minimal POLL_ONLY profile never performs notification RMWs. PARKABLE_SPSC is a separate immutable connection mode. Each enabled wait direction has one waiter and one dedicated aligned 32-bit state: AWAKE=0, ARMED=1.

All state mutations after initialization—including same-value notifications, arming, and disarming—are acquire-release RMW operations. The notifier performs its state-to-AWAKE RMW after publishing the predicate-changing cursor, on every relevant publication. If it observed ARMED, it issues the platform wake. The waiter arms, acquire-rechecks the cursor and lifecycle, then compare-waits for ARMED only if waiting is still justified.

Linux supplies a process-shared expected-value futex wait; Darwin’s admitted public address-wait API requires the matching shared flags and word size. The wait itself does not replace the payload acquire. [R28, R29]

### 8.1 Modification-order lemma

An RMW reads the immediately preceding modification in the atomic object’s modification order. Since every mutation here is acquire-release, adjacent RMWs synchronize, including same-value transitions. Transitivity carries preceding cursor/lifecycle publication through any finite intervening RMW chain. [R01, R02]

### 8.2 Case A: notification precedes arm

Let N be the notifier’s RMW and A the waiter’s arm RMW, with N earlier than A in wait-state modification order. The RMW chain establishes:

\[
\mathrm{publish}\to_{hb}N\to_{hb}A\to_{sb}\mathrm{predicate\ recheck}.
\]

For the monotonic single-writer predicate cursor, its acquire recheck cannot authorize a state older than the relevant happens-before publication. If the message has not already been consumed by the sole consumer, the recheck discovers availability and does not sleep on a stale empty state. A previously processed message need not keep the waiter awake.

### 8.3 Case B: arm precedes notification

If A precedes N and the waiter remains in this wait attempt, N changes ARMED to AWAKE. If this occurs before the expected-value kernel wait, the value mismatch prevents that late wait from sleeping on the old state. If blocking has already enrolled the waiter, the subsequent wake makes it eligible to run.

Only the sole waiter can set ARMED. It cannot re-arm another attempt while it remains blocked in this one. Consequently no other participant can restore the expected value and conceal this notification from the current attempt. Other notifications only write AWAKE.

### 8.4 Timeout, disarm, re-arm, and close

After a timeout, interruption, or spurious wake, the waiter disarms through the same RMW discipline and rechecks the actual predicate and lifecycle. It re-arms only for a new attempt. A delayed old wake can become spurious; predicate rechecking makes that harmless.

Close release-publishes lifecycle before issuing the same notification protocol to each enabled direction. An active waiter checks lifecycle as well as data/space availability. This prevents a normal live closer from leaving it asleep merely because no payload was added. Closing does not authorize unmapping storage still referenced by a lease.

The producer can still die between cursor publication and wait-state modification, or between wait-state modification and the kernel wake. No ordering primitive turns those steps into a transaction. Finite wait deadlines and lifecycle rechecks remain required for crash-aware eventual detection, subject to the waiter actually being scheduled.

**Result:** the normal single-waiter lost-wake histories are excluded under the declared protocol and OS compare-and-wait contract. This is not an MPMC waiter proof, a priority-inheritance mechanism, or a bound on wake-up latency. Optimizing away the same-value notification RMW invalidates this particular argument and requires a replacement proof.

---

## 9. Prior-art dissection: what each design actually buys

### 9.1 LMAX Disruptor

**Source-derived mechanism.** Disruptor separates the preallocated event ring, sequencer, sequence barriers, consumer gating, and wait strategy. It is primarily an inter-thread event-processing design with multicast/dependency capabilities. Its Sequence abstraction addresses false sharing in its own runtime. These are not identical to a C shared-memory work-sharing queue. [R18]

In the pinned multi-producer sequencer, availability is release-published per sequence and acquire-observed. `getHighestPublishedSequence` walks the requested range and stops at the first unavailable sequence. [R19]

**Our deduction.** A producer stopped after claiming an early sequence can therefore prevent that contiguous consumer frontier from passing it even when later sequences are available. That behavior supports ordered event processing; it does not satisfy our strict stopped-producer progress requirement by virtue of having per-sequence availability flags.

The consumer gating rule also changes capacity semantics: a slow required subscriber can retain storage for every producer. In work-sharing, one successful claimant owns an item; in multicast, every configured gate must permit reclamation. These are distinct protocols.

**Borrow:** preallocation, local cached gates, explicit wait strategies, batch/prefix publication, and clear separation of sequencing from storage.

**Do not transplant:** Java padding as a portable C ABI; a multi-producer claim window as proof of crash-independent FIFO progress; a consumer graph as though it were one reclamation cursor. Busy-spin, yielding, sleeping, and blocking choices remain CPU/latency policies, not memory-order proofs.

### 9.2 DPDK rte_ring

**Source-derived mechanism.** The documented classic MP/MC path separates reservation heads from published tails. Producers claim head space using CAS, write entries, then publish in tail order. RTS changes tail-update coordination; HTS serializes same-side operations. Bulk requests are all-or-nothing, while burst operations can process an available subset. [R20, R21]

The current documented start/finish and zero-copy/peek facilities are restricted to SP/SC or HTS. In that split window, other operations on the relevant side cannot simply proceed independently. [R20]

**Our deduction.** Classic tail-order publication fails the paused-earliest-producer requirement: later completed copying cannot make the earlier missing payload complete. RTS avoids some tail-spinning/preemption costs, but a relaxed completion counter does not itself supply abandoned payload bytes. HTS is explicit serialization, not the MPMC lock-free borrow protocol we require.

**Watermark correction.** Built-in `rte_ring_set_water_mark` support was removed in the 17.05 release notes. The replacement API information allows applications to implement their own watermark policy. Do not design against an obsolete API merely because an old ring tutorial describes it. Application occupancy thresholds and admission correctness must be kept separate. [R22]

**Borrow:** bulk/burst semantics, explicit synchronization profiles, modulo-index reasoning with bounded-distance assumptions, and published occupancy information whose snapshot meaning is documented.

**Do not transplant:** hidden same-address pointer assumptions into offset-based IPC, EAL/framework dependencies into the zero-dependency core, or the word “lockless” as a formal stalled-thread progress theorem. Arm’s RCpc case study is a further reason to audit the exact metadata orders rather than trusting source-level acquire labels. [R12]

### 9.3 Aeron / Real Logic lineage

**Source-derived mechanism.** Aeron log buffers contain three rotating terms plus metadata. Term identity and offset distinguish stream positions and reuse; publication and subscription/image semantics preserve stream ordering. A buffer claim can expose writable log space directly, then commit or abort it. [R23, R24]

The pinned BufferClaim implementation release-publishes frame length on commit. Abort marks a padding frame and release-publishes its length. The pinned TermUnblocker distinguishes positive, negative, and zero frame-length conditions, and can install padding over an unfinished region after the surrounding protocol decides to unblock it. [R25, R26]

**Our deduction.** Aeron makes the abandoned-claim problem explicit and has runtime machinery to restore stream progress. That is useful engineering precedent, not proof that a timeout revokes a pointer in our shared-memory pool. Its documented contract requires completing or aborting a claim before the unblock timeout. Arbitrarily paused writers violate that deadline assumption; our safety model cannot silently import it.

An Aeron transport log is also not automatically durable transactional storage. Archive/replication and application semantics are additional layers. Similarly, an embedded driver can avoid a separate process while still providing services that a bare ring does not contain. “No mandatory external daemon” must not be confused with “no coordination work exists.”

**Borrow:** explicit claim/commit/abort, type-tagged padding, generation-aware stream positions, separate completed frontier versus observed high-water position, and honest backpressure/admin outcomes.

**Do not transplant:** runtime-dependent unblocking as autonomous kernel-free recovery, a claim timeout as effective fencing, or publication/subscription stream semantics as interchangeable with work-sharing MPMC. This analysis uses the currently resolved official repository `aeron-io/aeron`; the older `real-logic/aeron` reference did not resolve through the connector in this turn.

### 9.4 Simon Cooke’s BipBuffer

**Source boundary.** The original CodeProject article did not return readable content in this research session. Cooke’s own historical article establishes authorship and describes the two-region representation, including an older region A and a newer prefix region B. The following is an explicit geometric reconstruction for this project, not a claim to have audited every line of the unavailable original implementation. [R27]

Let the backing array be [0,L). Region A=[a₀,a₁) contains the older readable bytes. When B exists, it is [0,b₁), with:

\[
0\le b_1\le a_0\le a_1\le L.
\]

The consumer reads A first. When A is exhausted, B can be promoted by changing metadata; no payload compaction is required. A producer reserves a contiguous free interval, fills it, and commits only the filled extent. Any concurrent adaptation must synchronize the boundary information that authorizes reads and future overwrites.

**Spatial proof.** Appending to an available end interval without crossing another live region preserves disjointness. Advancing a₀ consumes only the older prefix. Promoting B after A becomes empty preserves FIFO byte order because every byte in B was written after every remaining byte in A. No existing payload needs to move.

**“Zero fragmentation” is too strong.** With L=1024 and A=[256,900), free prefix and suffix lengths are 256 and 124. There are 380 free bytes in total, but neither interval admits one contiguous 300-byte reservation without moving data or changing the request. When B has been opened, a tail gap can also remain temporarily unusable under an ordered contiguous-allocation policy.

Thus the useful promise is contiguous reservations and no compaction copying, not universal success whenever aggregate free bytes exceed request length. An API may return a smaller reservation; that is different from satisfying an all-or-nothing 300-byte request.

**Borrow:** reserve/commit semantics and explicit contiguous-region geometry for a later variable-size profile.

**Do not transplant:** two-region arithmetic as an MPMC ownership or crash-recovery proof. Fixed-size blocks avoid this second allocator and its coupled reclamation rules in the current low-latency reference. No original BipBuffer source code is copied into this deliverable, and no unverified reuse licence is assumed.

### 9.5 The less-prominent primary artifacts worth carrying forward

The highest-value additional artifacts are Nikolaev’s **NCQ Figure 5**, the **SCQ threshold/livelock construction**, and the author’s sharply separated implementation variants. NCQ is easy to miss because it is called “naive,” yet its publish-before-tail helping property fits our proof problem exceptionally well. SCQ is valuable because it explains how FAA allocation becomes genuinely nonblocking rather than merely syntactically atomic. [R14–R17]

This is a relevance judgment, not a fabricated current repository-star ranking. No claim of novel invention is attached to using these mechanisms.

---

## 10. Theoretical comparison, not a counterfeit benchmark

### 10.1 Contract comparison

| Design | Natural publication/consumption model | Stopped early claimant | Primary mismatch with our target |
|---|---|---|---|
| Polling SPSC proposal | One writer/reader, publication and recycling cursors | Uncommitted reservation remains private | Only one active producer and consumer; retained lease bounds capacity |
| NCQ-SC64 + payload pool | Publish completed index first; help tail; claim by head CAS | Before LP, no FIFO position owned; after LP, descriptor readable | CAS contention, finite pool leakage after crash, no individual wait-freedom |
| SCQ candidate + pool | FAA positions, safe invalidation/retry and threshold logic | Internal empty positions can be retired safely by the exact algorithm | More complex proof, weak-order refinement and finite-width assumptions |
| LMAX multi-producer sequencer | Preclaimed sequence and availability frontier | Contiguous reader frontier stops at an unpublished sequence | Multicast/gating and inter-thread runtime, not our stopped-writer contract |
| Classic DPDK MP/MC | Claim head, fill entries, publish tail in order | Earlier completion dependency can retain tail frontier | Overcommit/preemption assumption; not arbitrary-duration zero-copy leasing |
| DPDK HTS zero-copy | Serialized same-side start/finish | Retained split operation excludes same-side progress | Serialization is part of its contract |
| Aeron claim | Reserve log position, commit frame or abort as padding | Runtime unblocking under its claim-time contract | Driver/lifecycle services and stream semantics; no timeout-based pointer revocation theorem |
| BipBuffer geometry | Contiguous byte reservations in two regions | Depends on a separately specified concurrency protocol | Spatial scheme alone supplies no MPMC progress guarantee |

The rows are architectural comparisons derived above, not a speed ranking.

### 10.2 Operation-count ledger

For an uncontended complete SPSC message lifecycle, the synchronization core is a producer release publication and consumer release reclamation, with covering peer acquires. Cached snapshots and batch prefixes can amortize some peer loads. There is no queue RMW in the minimal polling profile.

For one uncontended NCQ-SC64 payload lifecycle, ignoring setup and phase validation:

| Internal operation | Successful-path RMW work |
|---|---|
| Dequeue block from QF | One head CAS |
| Enqueue completed block into QR | One entry CAS and one tail-CAS attempt |
| Dequeue block from QR | One head CAS |
| Return block to QF | One entry CAS and one tail-CAS attempt |
| Complete lifecycle | Six RMW attempts in this uncontended trace, plus loads and phase operations |

Helping, contention, and retries increase that work. These counts do not translate directly into nanoseconds. Some operations are on one core’s recently used line; others acquire ownership from a peer. The distribution of these cases matters.

Each enabled parkable notification direction adds its notification RMW to the relevant publication or reclamation. Waiter enrollment and syscalls are additional slow-path costs. Do not compare the parkable implementation’s measured numbers with a polling-only count while omitting those operations.

### 10.3 Why published numbers cannot settle the target

The SCQ paper evaluates its algorithms using specific x86/Power systems and benchmark workloads. Those observations are not measurements of our offset-based IPC ABI, Apple core topology, payload construction, Python binding, or wake protocol. Likewise, throughput demonstrations for Disruptor or DPDK are not end-to-end one-message latency observations for this design. [R14, R18, R20]

No estimated “18 ns,” “35 ns,” or “2× faster” result is assigned to our unbuilt system. The <50-ns target remains an acceptance experiment with a nominated payload, start/end events, quantile, topology, and uncertainty budget.

### 10.4 Measurements to preregister in Turn 5

Separate at least: native SPSC polling; native parkable notification overhead while awake; actual sleeping wake-up; native MPMC by participant count; direct payload construction versus copying from an existing buffer; and Python/native-boundary cost.

Measure nominated 8-, 64-, and 256-byte payloads, several capacities and occupancies, same-cluster/cross-cluster and P/E conditions where placement is actually known, plus x86 locality variants. Include a stalled producer before publication, after publication, and while holding a payload lease. Keep one-way latency, round-trip latency, throughput, and recovery-detection time as separate quantities.

Record raw observations, timestamp ordering, calibration and resolution, sample count, latency quantiles and tails, page backing actually obtained, compiler/OS/CPU identity, and whether scheduling placement was enforced or merely requested. Source papers provide comparison hypotheses; the later harness supplies evidence.

---

## 11. The four fatal histories, sharpened for Turn 3

### 11.1 Split-line controls and false isolation

Reject an atomic whose width/alignment is outside the admitted hardware ABI before operating on it. Reject a claimed isolated profile whose member offsets or strides cause protected cells to overlap at an admitted coherence granule. A descriptor array aligned to 128 with eight-byte stride is a mandatory negative case.

A multi-line ordinary payload is not itself a failure; it is safe when its ownership chain holds. The test must distinguish atomic-control tearing risk, false sharing, and legal multi-line payload transfer.

### 11.2 Producer crash at every boundary

Enumerate QF claim, owner-record update, RESERVED transition, each payload write, COMMITTED transition, QR publication, tail assistance, return-to-caller, notification state change, and wake syscall. Repeat symmetric boundaries for consumers and token return.

Before QR publication, a stopped builder must not prevent healthy ready descriptors from being consumed. After publication, the payload must remain readable and tail assistance must work without that publisher. No test may treat “caller did not receive success” as “message was not published.” Owner-record gaps and leaked tokens remain recovery questions, not proof that the ready queue is poisoned.

### 11.3 Sequence wrap and stale observation

Reduce counter widths in the mathematical adversarial histories. Retain a saved head/tail expectation, entry word, and user lease across many cycles. Confirm that the no-live-wrap rule retires the session before any represented identity can repeat.

The production-sized width is not the proof. Every internal queue cycle, block generation, and arithmetic comparison needs its own representable bound. Masking a ticket to N−1 obtains a position, never a generation identity. If a future modular scheme is adopted, its suspension and half-range assumptions must replace—not accompany and contradict—the no-wrap theorem.

### 11.4 Darwin scheduling and priority inversion

A high-priority busy consumer can coexist with a delayed producer. Padding and acquire/release ordering do not schedule that producer. The admitted Darwin address waits expressly do not provide priority-inversion avoidance. [R29]

The architecture therefore reports polling progress in own steps and parkable behavior under OS scheduling assumptions. It does not promise a real-time deadline from selecting a QoS class, nor infer Linux-style affinity enforcement on Darwin. A preempted NCQ publisher no longer owns an indispensable uncommitted FIFO position, but exhausting the CPU or all payload tokens is still a real resource problem.

---

## 12. Admission and kill register

| ID | Decision at Turn 2 | Evidence still required before release |
|---|---|---|
| T2-01 | Retain 128-byte Apple software isolation; reject universal P/E cache claim | Actual target geometry/locality and ABI manifest |
| T2-02 | Accept layout theorem for admitted granularities and strict strides | Compile-time offsets and runtime attach validation after the gate |
| T2-03 | Accept SPSC forward/reverse ownership proofs | Faithful implementation, target atomic ABI, disassembly and stress histories |
| T2-04 | Accept single-waiter RMW parking proof under stated live-notifier model | OS adapter validation, timeout/close fault histories |
| T2-05 | Select NCQ-SC64 as explicit MPMC proof/reference profile | Independent adversarial review in Turn 3; implementation/refinement validation later |
| T2-06 | Retain exact single-width SCQ as optimization candidate | History-dependent linearization, weak-memory refinement, threshold/wrap review |
| T2-07 | Reject four-state-only or ticket-lock MPMC proof | Requires a genuinely different algorithm, not additional state names |
| T2-08 | Reject Apple Silicon 2-MiB superpage shortcut via examined path | Any alternative must have its own documented supported mapping mechanism |
| T2-09 | Linux HugeTLB/THP remain optional profiles | Provisioning, actual mapping evidence, comparative measurements |
| T2-10 | In-place crash reclamation not accepted | Effective fencing and crash-consistent ownership transfer including gaps |
| T2-11 | MPMC kernel parking not accepted by SPSC proof | Independent multi-waiter protocol and fairness/wake analysis |
| T2-12 | Python raw borrowing remains lifetime-constrained | Both mapping and token held until all exported aliases end |
| T2-13 | <50-ns target remains unmeasured | Preregistered interval, quantile, hardware, raw data and uncertainty |

The selected reference is not a production certification. The proof is explicit enough to attack: entry publication must precede tail advancement; queue metadata is SC in this reference; token admission prevents full internal enqueues; payload transfers occur at queue LPs, not function returns; no live identity wraps; and recovery never steals bytes from a resumable writer.

---

## 13. What Turn 3 should attempt to falsify

The highest-value challenge is no longer “can an early producer leave a ticket hole?” That hole is absent from the selected publish-first reference. The stronger attacks are: a saved consumer entry surviving slot reuse before head CAS; a producer finishing old bookkeeping after its block has already been reused; token loss in a crash gap; a lying layout manifest; and a wait-state optimization that silently removes the same-value RMW synchronization chain.

A valid kill must identify a permitted execution that violates an invariant, not merely show a slow context switch. A valid performance improvement must preserve the accepted memory model and lease contract, not silently change global ordering, failure results, or borrower lifetime.

**Bottom line:** SPSC stays minimal. MPMC gets a real publication algorithm, not a phase diagram masquerading as one. Alignment stays an explicit ABI policy, not folklore about Apple cores. The project proceeds to adversarial proof review with a specific reference, a specific optimization candidate, and no invented latency result.

---

## 14. Primary-source ledger and evidence boundaries

**Access date:** 2026-09-23. Source code was inspected as prior art, not copied into this deliverable. No source implementation, runnable header, benchmark harness, compiler probe, or executable model is included. Repository references below identify the observed commit or content blob. Documentation labeled live is not a pinned deployment binary or SDK. Source observations, this report’s proposed modifications, and its own deductions are distinguished in the body.

### Language and atomic contracts

**[R01] ISO C11 committee draft N1570.**

https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf

Stable document. Relevant sections: 5.1.2.4 for multithreaded executions and synchronization; 7.17.2 for initialization; 7.17.3 for ordering and the RMW predecessor rule; 7.17.5 for lock-free/address-free implementation considerations. Supports the language-level rules used in the proofs; it does not certify any particular interprocess ABI or OS mapping.

**[R02] C++ working draft — ordering and lock-free properties.**

https://eel.is/c++draft/atomics.order

https://eel.is/c++draft/atomics.lockfree

Live working-draft pages. Used for the corresponding C++ terminology, release/acquire synchronization, atomic modification order, and address-free recommendation. They are not represented as a frozen copy of the C++20 publication; the core C11 rules used here are also located in [R01].

### Cache geometry, shared memory, and page policy

**[R03] Apple XNU cache constants.**

https://github.com/apple-oss-distributions/xnu/blob/f6217f891ac0bb64f3d375211650a4c1ff8ca1ea/osfmk/arm64/proc_reg.h

Commit: `f6217f891ac0bb64f3d375211650a4c1ff8ca1ea`. Observed content blob: `2b6d7231cb1db4ee4f1a26e4775b5284de332acf`. The APPLEFIRESTORM, APPLEAVALANCHE, and APPLEEVEREST configuration blocks define MMU_CLINE=6 with 64-byte comments. This establishes a kernel constant, not a complete silicon geometry or proof that cache maintenance, allocation, and coherence granularities are identical. No cache-capacity figures from those comments are adopted.

**[R04] Apple XNU per-cluster cache discovery.**

https://github.com/apple-oss-distributions/xnu/blob/f6217f891ac0bb64f3d375211650a4c1ff8ca1ea/osfmk/arm/cpuid.c

https://github.com/apple-oss-distributions/xnu/blob/f6217f891ac0bb64f3d375211650a4c1ff8ca1ea/osfmk/arm/machine_routines_common.c

Same pinned XNU commit. The examined do_cacheid path reads architectural cache information by cluster type and records boot-CPU information for compatibility; the machine-info path selects cluster-specific cache information. No register values were obtained from Leon’s hardware.

**[R05] LLVM issue 182951 — interference-size macro mismatch.**

https://github.com/llvm/llvm-project/issues/182951

Primary issue record opened 2026-02-23 and closed 2026-07-10. The report contrasts a macOS cache-line query of 128 with compiler macros reporting 64. This is evidence of a historical compiler/OS interface mismatch, not a claim that every present compiler remains affected or a vendor cache-level specification.

**[R06] Apple XNU superpage admission path and flag definition.**

https://github.com/apple-oss-distributions/xnu/blob/f6217f891ac0bb64f3d375211650a4c1ff8ca1ea/osfmk/vm/vm_map.c

https://github.com/apple-oss-distributions/xnu/blob/f6217f891ac0bb64f3d375211650a4c1ff8ca1ea/osfmk/mach/vm_statistics.h

Pinned XNU commit above. In the inspected superpage request handling, a caller-supplied VM object is rejected, the recognized ANY/2MB cases are x86-64 conditional, and other cases fail. The userspace-visible flag definition is broader than kernel support for the requested use. This rules out the proposed shortcut through that path; it does not establish every possible internal Apple page-table policy.

**[R07] Linux kernel — HugeTLB administration.**

https://www.kernel.org/doc/html/latest/admin-guide/mm/hugetlbpage.html

Live official documentation. Supports explicit huge-page provisioning and backing requirements. Does not establish that the eventual deployment has provisioned huge pages or that they improve the target latency.

**[R08] Linux man-pages — memfd_create.**

https://man7.org/linux/man-pages/man2/memfd_create.2.html

Supports the HugeTLB-backed memfd option and page-size/permission requirements. Native file-descriptor distribution is bootstrap work, outside the data-plane timing interval.

**[R09] Linux kernel — Transparent Huge Pages.**

https://www.kernel.org/doc/html/latest/admin-guide/mm/transhuge.html

Live official documentation. Supports distinguishing THP, explicit HugeTLB, and shmem policy. A requested hint is not evidence of effective huge-page backing.

**[R10] Linux man-pages — mmap.**

https://man7.org/linux/man-pages/man2/mmap.2.html

Supports MAP_SHARED, backing-object and page-offset rules, mapping lifetime, and huge-page-specific constraints. It does not supply language synchronization or a process-crash recovery protocol.

### Arm and compiler mappings

**[R11] Arm — enabling RCpc in GCC and LLVM.**

https://developer.arm.com/community/arm-community-blogs/b/tools-software-ides-blog/posts/enabling-rcpc-in-gcc-and-llvm

Primary Arm compiler discussion, published 2023-11-15. Supports eligible LDAPR lowering of language acquire operations and the distinction from LDAR. Reported compiler support is source context, not a binary audit of the future project.

**[R12] Arm — When a barrier does not block: the pitfalls of partial order.**

https://developer.arm.com/community/arm-community-blogs/b/architectures-and-processors-blog/posts/when-a-barrier-does-not-block-the-pitfalls-of-partial-order

Primary analysis explicitly distinguishing LDAR/RCsc and LDAPR/RCpc and examining a DPDK ring ordering/capacity failure. Used to challenge cross-variable freshness assumptions. This report does not conclude that all current DPDK versions retain that historical fault.

**[R13] Cambridge memory-model researchers — C/C++11 processor mappings.**

https://www.cl.cam.ac.uk/~pes20/cpp/cpp0xmappings.html

Author-maintained mapping reference covering x86 and AArch64, including acquire/release loads/stores and fences. The source itself treats the mappings as discussion material, not an exhaustive guarantee for every compiler optimization. Expected lowering is distinguished from disassembly throughout this report.

### Lock-free queue primary literature and reference source

**[R14] Ruslan Nikolaev — A Scalable, Portable, and Memory-Efficient Lock-Free FIFO Queue, DISC 2019.**

https://drops.dagstuhl.de/entities/document/10.4230/LIPIcs.DISC.2019.28

https://drops.dagstuhl.de/storage/00lipics/lipics-vol146-disc2019/LIPIcs.DISC.2019.28/LIPIcs.DISC.2019.28.pdf

https://arxiv.org/pdf/1908.04511

DOI: `10.4230/LIPIcs.DISC.2019.28`. Stable publication. Relevant portions: two-queue indirection, §4/Figure 5 NCQ, and §5/Figure 8 SCQ, including the model and bounded progress arguments. The figure was visually inspected through the arXiv PDF when a publisher-page screenshot was unavailable. Published experiments are not reproduced or represented as Apple Silicon IPC measurements. NCQ-SC64’s explicit metadata orders, layout, payload phases, and composition arguments are this report’s specified reference refinement, not claimed source text.

**[R15] Nikolaev author repository — NCQ source.**

https://github.com/rusnikola/lfqueue/blob/708c0052872950dcb15b487fa7a5dd77ce2a2746/lfring_naive.h

Commit: `708c0052872950dcb15b487fa7a5dd77ce2a2746`. Content blob: `b0b0fbb478c39f544cfdc4f3cdad134b74d16494`. Full file inspected. Supports publish-before-tail advancement, helper tail advancement, consumer head claiming, cycle/index packing, and the source’s cache remapping and acquire/acquire-release orders. The proposed SC64 reference is explicitly stronger in metadata ordering and different in physical stride.

**[R16] Nikolaev author repository — single-width SCQ source.**

https://github.com/rusnikola/lfqueue/blob/708c0052872950dcb15b487fa7a5dd77ce2a2746/lfring_cas1.h

Same commit. Content blob: `b6bf963657c5d5ff19394476344a97f59ea4ca9c`. Relevant enqueue/dequeue ranges inspected. Supports FAA allocation, entry generation handling, threshold/catch-up behavior, transformed index representation, and the particular retry-before-invalidation bound. No optimized weak-memory IPC proof is claimed merely from reading this file.

**[R17] Nikolaev author repository — variant and licensing descriptions.**

https://github.com/rusnikola/lfqueue/blob/master/README.md

Observed README content blob: `42b9009b682851b71826107296a480816370426a`. Distinguishes NCQ, SCQ, SCQD, SCQ2, and wCQ; describes dual BSD-2-Clause/MIT licensing for the identified queue sources. The URL is a branch link, so the blob identity—not the mutable URL alone—identifies the inspected content. No star count or popularity ranking is used as a correctness argument.

### Comparative event/transport systems

**[R18] LMAX Disruptor user guide.**

https://lmax-exchange.github.io/disruptor/user-guide/

Live official guide, rendered as 4.0.0-SNAPSHOT in retrieval. Supports sequencers, gating, event-processing dependencies, and wait strategies. Java inter-thread semantics and padding are not automatically a C IPC ABI.

**[R19] LMAX multi-producer sequencer.**

https://github.com/LMAX-Exchange/disruptor/blob/c871ca49826a6be7ada6957f6fbafcfecf7b1f87/src/main/java/com/lmax/disruptor/MultiProducerSequencer.java

Commit: `c871ca49826a6be7ada6957f6fbafcfecf7b1f87`. Content blob: `b773920e06c0c3bc5056e04ad739460d2124242e`. Inspected availability publication/observation and highest-published-sequence scan. The stalled-claim interpretation is this report’s deduction from that scan, not a claim that Disruptor promised a different progress contract.

**[R20] DPDK ring library programmer’s guide.**

https://doc.dpdk.org/guides/prog_guide/ring_lib.html

Live official documentation, rendered as 26.07.0 during retrieval. Supports MP/MC head/tail separation, synchronization profiles, bulk/burst distinctions, and start/finish zero-copy restrictions. Its performance and preemption discussion is not an arbitrary-process-crash theorem.

**[R21] DPDK head/tail implementation anchors.**

https://github.com/DPDK/dpdk/blob/821542535cae702a4b3a4902de988ff74b8dfed5/lib/ring/rte_ring_elem_pvt.h

https://github.com/DPDK/dpdk/blob/821542535cae702a4b3a4902de988ff74b8dfed5/lib/ring/rte_ring_c11_pvt.h

Commit: `821542535cae702a4b3a4902de988ff74b8dfed5`. Targeted source search inspected the tail-update location and the C11 publication/reclamation pairing comment. This is not a whole-DPDK source audit. Detailed protocol descriptions are also grounded in [R20].

**[R22] DPDK 17.05 release notes.**

https://doc.dpdk.org/guides-17.05/rel_notes/release_17_05.html

Versioned official release notes, retrieved in the 17.05 documentation series. Records removal of built-in ring watermark support and the changed API information available for application-managed watermark policy. This is a historical API correction, not a current runtime benchmark.

**[R23] Aeron log buffers and images.**

https://aeron.io/docs/aeron/log-buffers-images/

Live official documentation. Supports the term-log architecture, three rotating term partitions, metadata, publication/image concepts, and unblocking context. It does not transform an in-memory transport into a durable or exactly-once application transaction system.

**[R24] Aeron tryClaim cookbook.**

https://aeron.io/docs/cookbook-content/aeron-try-claim/

Live official documentation. Supports bounded claim/commit/abort responsibilities and the danger of leaving a claim unfinished. No documented default timeout is adopted as this project’s safety or latency guarantee.

**[R25] Aeron BufferClaim implementation.**

https://github.com/aeron-io/aeron/blob/ad36b32434d61489ab3aef94f1628540d8208de2/aeron-client/src/main/java/io/aeron/logbuffer/BufferClaim.java

Commit: `ad36b32434d61489ab3aef94f1628540d8208de2`. Content blob: `f8add1df523ac3d34313dfe6dd5557ce5d6e3d9a`. Inspected commit/abort methods: release-publication of frame length and padding-type publication for abort. The currently resolved official repository is under aeron-io; the older real-logic path did not return the requested file through the connector.

**[R26] Aeron TermUnblocker implementation.**

https://github.com/aeron-io/aeron/blob/ad36b32434d61489ab3aef94f1628540d8208de2/aeron-client/src/main/java/io/aeron/logbuffer/TermUnblocker.java

Same commit. Content blob: `8d49e340a016ec3d1fbbb390f9c34b29f27bd22d`. Inspected treatment of negative and zero frame lengths, scanning, and padding-header reset. Unblocking logic is not evidence that arbitrary stale raw writer pointers have been physically revoked.

**[R27] Simon Cooke — original-author BipBuffer account.**

https://accidentalscientist.com/2010/02/the-darker-side-of-google-open-source-taking-without-attribution.html

Original article referenced by the author:

https://www.codeproject.com/Articles/3479/The-Bip-Buffer-The-Circular-Buffer-with-a-Twist

The author’s account was readable and establishes attribution and the two-region contiguous-allocation approach. The original CodeProject article was not successfully retrieved. The region inequalities and fragmentation counterexample in §9.4 are this report’s geometric reconstruction, not a claimed full original-source concurrency audit. No implementation or code excerpt is reproduced.

### Waiting and implementation admission

**[R28] Linux man-pages — FUTEX_WAIT.**

https://man7.org/linux/man-pages/man2/FUTEX_WAIT.2const.html

Supports atomic expected-value checking/block enrollment, mismatch, timeout and spurious-return behavior. The userspace single-waiter RMW proof is this report’s argument; the syscall documentation does not prove an arbitrary enrollment algorithm.

**[R29] Apple public address-wait header.**

https://raw.githubusercontent.com/apple-oss-distributions/libplatform/main/include/os/os_sync_wait_on_address.h

Live official source header, accessed on the report date. Supports availability annotations, alignment/width, shared wait/wake flags, timeout interfaces, and lack of priority-inversion avoidance. The exact deployment SDK must be pinned before implementation admission. This live API reference is not represented as a pinned kernel binary.

**[R30] GCC atomic built-ins documentation.**

https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html

Live official compiler documentation. Supports the order arguments, primitive-width/fallback considerations, and lock-free queries. It does not certify a particular generated AArch64 or x86-64 binary.

**[R31] Arm — memory access ordering in the Arm architecture.**

https://developer.arm.com/community/arm-community-blogs/b/architectures-and-processors-blog/posts/memory-access-ordering-part-3---memory-access-ordering-in-the-arm-architecture

Primary Arm explanation of barrier scope and shareability domains, including ISH versus SY. Used for that scope distinction, not as a complete modern AArch64 compiler mapping; [R11]–[R13] cover the latter topic.

### Evidence closure

The inspected sources establish the named mechanisms and important counterexamples. The specific NCQ-SC64 composition and single-waiter parking arguments are transparent pen-and-paper derivations with declared assumptions. No model checker, compiler, disassembler, stress harness, or target machine was run to validate them. Exact physical cache geometry for Leon’s machine and comparative nanosecond performance remain unmeasured. The repository snapshots make subsequent review reproducible without confusing source review with implementation certification.
