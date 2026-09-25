# Turn 04 — Formal C11 ABI and Assembly Lowering Specification
## ELITEIPC / LE128-V1 / SPSC and NCQ-SC64

**Project:** Leonid Majbits / Gemini Operator Lab, paired with Leon  
**Date:** 2026-09-23  
**Stage:** 4 of 8 — normative architecture and ABI specification; implementation remains unauthorized.  
**Document status:** Proposed byte-exact format, operation contracts, and paper proofs. No C/C++ header, assembly routine, compiler probe, executable queue model, or benchmark is supplied.  
**Authoritative predecessor:** `03_Turn_03_Concurrency_Kill_Gate_and_Failure_Modes.md`, 81,776 bytes, literal SHA-256 `7ad5fbd43134e7d3419c5a1f76d030e25d051510dbc6f20ce173b3d0eccff605`. Downloaded from the specified Drive folder and verified in this turn.  
**Earlier protocol:** Turn 02, 91,463 bytes, SHA-256 `9bb8b971b65f7da80bd13053e8027aaa6ede5ae9ea19da8262e24282e16a6977`.  
**Canonical-SHA256:** `f127bd9cde889afd03538d3d750469bdc16735c62a112ca76d1d9865cd319817`  
**Hash convention:** The preceding field authenticates the complete UTF-8/LF document after replacing only its own 64 hexadecimal characters with 64 ASCII zero characters. It is not the literal hash of the finalized file. The adjacent `.md.sha256` receipt contains that literal-file hash; §17 defines both procedures.  
**Destination:** Deployment Research / Elite_Systems_LockFree_RingBuffer  
**Evidence convention:** [P2] and [P3] identify the preceding project specifications; [R01]–[R19] identify external primary sources in §18. New choices below are this turn's normative proposals, not claims that a compiler or target machine has already implemented them.

---

## 0. Decisions, source reconciliation, and rejected premises

Freeze one cross-target shared-memory layout: **LE128-V1**. It uses little-endian fixed-width integers, explicit 128-byte isolation cells, 2,048-byte mode-specific headers, 128-byte slot descriptors, and a 16,384-byte format allocation quantum. The format quantum is not a claim that either OS supplied a huge page. A platform's actual base page size must divide it.

The named structures are specified by complete member/offset/extent tables, including every reserved range. These tables are normative definitions for a later C11 header. They are not compilable declarations. This satisfies the planning boundary in [P2] without asking an implementer to guess field order, padding, widths, or state semantics.

The exact Drive predecessor named by the user is the governing Turn 3 version. An earlier conversation delivery has a different byte count, digest, latency threshold, and quarantine policy. It is not silently merged with this version. In the governing [P3], 1,000 ns is a proposed p99.9 service-rejection threshold; its §6.4 explicitly distinguishes requested/observed P/E placement from verified affinity. There is no measured 1,000-ns P/E interconnect bound and no pinning result to carry forward. No threshold is changed in this turn.

| Requested premise | Normative resolution |
|---|---|
| Complete struct definitions, but zero implementation code | Complete byte-layout and member tables; no compilable header or function body |
| Explicit padding and a “zero-padding-free” layout | No implicit, unaccounted, or uninitialized format padding; explicit reserved bytes are present and zero-initialized |
| Identical layout on Apple ARM64 and Linux x86-64 | One LE128 wire layout on admitted native atomic ABIs; not a universal ISO C representation guarantee |
| CAS for write reservation | SPSC reserves privately without CAS; NCQ obtains a payload token through QF head CAS, not a claim-first QR ticket |
| LDREX/STREX for ARM64 | Those names belong to AArch32; AArch64 uses LDXR/STXR or LDAXR/STLXR families |
| Exact compiler assembly before implementation | Exact semantic obligations and permitted instruction forms; actual register allocation, helpers, and binary output remain a later admission test |
| Full barrier for slot revocation | No memory barrier revokes an old pointer; in-place live slot revocation is not an operation in this ABI |
| `/dev/shm` as the universal backing path | Linux namespace implementation detail; portable operations use a POSIX shm name and descriptor; Darwin is not assigned that path |
| A document containing its own ordinary SHA-256 | Canonical self-field hash in the document; separate literal-file SHA-256 receipt |

**Unchanged core:** SPSC retains both release/acquire ownership directions. NCQ retains single-width SC queue metadata, strong CAS, publish-before-tail ordering, token-based admission, and no live counter wrap. No new field turns phase scanning into recoverable queue membership. [P2, §§4, 6; P3, §§2–4]

---

## 1. Conformance boundary and notation

### 1.1 One authoritative C implementation

The shared objects are created and accessed by one admitted C11 implementation. C++20 and Python call that implementation's C ABI through opaque handles. They do not overlay their own atomic types on the mapping. There is no assumption that a `std::atomic`, a Rust atomic, and a C `_Atomic` object can be interchanged just because their scalar values fit in 64 bits.

C11 does not require atomic integer objects to have the same representation size as their non-atomic counterparts; address-free lock-free behavior is an important implementation condition, not an automatically proven property of arbitrary mapped bytes. Alignment support above the fundamental alignment is implementation-dependent. These facts motivate the explicit target admission below. [R01, §§6.7.5, 7.17.5–7.17.6]

### 1.2 Scalar and aggregate vocabulary

The following names are specification vocabulary, not typedef declarations:

| Name | Exact stored width | Meaning and required natural alignment |
|---|---:|---|
| B[n] | n bytes | n uninterpreted eight-bit octets; alignment 1 |
| U32 | 4 bytes | Unsigned 32-bit integer, little-endian, alignment 4 |
| U64 | 8 bytes | Unsigned 64-bit integer, little-endian, alignment 8 |
| AU32 | 4 bytes | Admitted C11 atomic U32 representation; alignment 4 |
| AU64 | 8 bytes | Admitted C11 atomic U64 representation; alignment 8 |
| Cell32 | 128 bytes | AU32 at relative offset 0; B[124] at offset 4; aggregate alignment 128 |
| Cell64 | 128 bytes | AU64 at relative offset 0; B[120] at offset 8; aggregate alignment 128 |

Every reserved B[n] contains zero bytes and is immutable after construction. All aggregate sizes and alignments specified below are exact, not minimum footprint estimates. A member of a Cell32/Cell64 is accessed through the correctly typed member, not by treating the entire cell as one atomic aggregate.

No shared member is a native pointer, `long`, `size_t`, `ptrdiff_t`, `time_t`, native enum, bool, bit-field, union discriminator with unstated representation, opaque OS handle, mutex, or language-runtime object. Native calling conventions differ across architectures; shared format equality does not mean that one executable or one register-level function ABI is portable between them.

### 1.3 Admitted target tuple

Admission requires: native little-endian execution; eight-bit bytes; exact U32/U64 and AU32/AU64 sizes; natural alignments in §1.2; support for 128-byte aggregate alignment; ordinary coherent cacheable CPU memory; address-independent atomic operations on aliases of the same backing; and lock-free 32/64-bit operations without hidden process-local lock fallback. The implementation must also establish valid object lifetime/effective-type use in the OS mapping. No 128-bit primitive is required. The baseline payload path uses ordinary cached CPU stores; hand-written non-temporal/streaming stores, MMIO, GPU, or DMA completion are not silently covered by its release-store lowering.

The tuple includes OS build, architecture, compiler build, target features, C library/runtime, optimization and LTO settings, and any outlined atomic helper. The format's `atomic_abi_id=1` declares conformance to these requirements; it does not make an untested tuple conform by assertion. Target compilation, disassembly, and cross-process execution are still unperformed gates. [R01–R05]

### 1.4 Interval and byte-order notation

An extent [a,b) includes a and excludes b. All offsets are from the beginning of the backing object unless explicitly called member-relative. All additions, products, and roundups in this document are mathematical integers until bounds validation proves a representable native operation.

`RU(x,q)` is the smallest multiple of q not below x. The fixed isolation size A is 128. The fixed format quantum Q is 16,384. A byte array is not endian-converted. U32/U64 scalar values are serialized least-significant byte first.

### 1.5 Byte equality, type equality, and live snapshots

The same valid N, B, K, and profile produce identical field offsets and region extents on both admitted targets. Session identity and current values naturally differ between independent sessions. Nothing permits directly copying a live header, atomic array, or whole mapping as a coherent snapshot. Ordinary byte copies of concurrently modified atomic storage are not the prescribed access protocol. Only immutable ranges or separately established quiescent snapshots may be copied wholesale.

---

## 2. Common immutable prefix: complete 512-byte definition

Both named ring headers begin with a real nested `elite_immutable_header` member at offset zero. It has size 512 and alignment 128. C accessors use that common member, not a cast from one unrelated outer structure type to the other. All fields in this table are immutable after READY publication.

| Offset decimal (hex) | Bytes | Member | Type | Required value or interpretation |
|---|---:|---|---|---|
| 0 (0x000) | 8 | magic | B[8] | ASCII octets `45 4C 49 54 45 49 50 43` |
| 8 (0x008) | 4 | abi_version | U32 | `0x00010000` |
| 12 (0x00C) | 4 | header_bytes | U32 | 2048 |
| 16 (0x010) | 4 | immutable_bytes | U32 | 512 |
| 20 (0x014) | 4 | format_flags | U32 | 0 in this version |
| 24 (0x018) | 4 | layout_profile | U32 | 1=SPSC_LE128; 2=NCQ_SC64_LE128 |
| 28 (0x01C) | 4 | endian_tag | U32 | `0x01020304`, stored `04 03 02 01` |
| 32 (0x020) | 4 | isolation_bytes | U32 | 128 |
| 36 (0x024) | 4 | mapping_quantum | U32 | 16384 |
| 40 (0x028) | 4 | atomic_abi_id | U32 | 1=LE native address-free AU32/AU64 contract |
| 44 (0x02C) | 4 | wait_mode | U32 | 0=POLL_ONLY; 1=PARKABLE_SPSC |
| 48 (0x030) | 4 | payload_checksum_mode | U32 | 0=NONE; 1=CRC64_ECMA182 |
| 52 (0x034) | 4 | header_crc32 | U32 | CRC in §7; its own four input bytes are treated as zero |
| 56 (0x038) | 8 | segment_bytes | U64 | Exact backing length from §6 |
| 64 (0x040) | 8 | capacity | U64 | N, power of two, 2 ≤ N ≤ 2^31 |
| 72 (0x048) | 4 | max_payload_bytes | U32 | B, 1 ≤ B ≤ 2^32−1, subject to total-size admission |
| 76 (0x04C) | 4 | endpoint_count | U32 | K; 2 ≤ K ≤ N |
| 80 (0x050) | 4 | descriptor_stride | U32 | 128 |
| 84 (0x054) | 4 | queue_entry_stride | U32 | 0 for SPSC; 128 for NCQ |
| 88 (0x058) | 8 | payload_stride | U64 | RU(B,128) |
| 96 (0x060) | 8 | participants_offset | U64 | Q=16384 |
| 104 (0x068) | 4 | participant_stride | U32 | 256 |
| 108 (0x06C) | 4 | lifecycle_profile | U32 | 1=MANAGED_ONE_SHOT |
| 112 (0x070) | 8 | descriptors_offset | U64 | Exact value from §6 |
| 120 (0x078) | 8 | payloads_offset | U64 | Exact value from §6 |
| 128 (0x080) | 8 | qf_entries_offset | U64 | 0 for SPSC; exact value from §6 for NCQ |
| 136 (0x088) | 8 | qr_entries_offset | U64 | 0 for SPSC; exact value from §6 for NCQ |
| 144 (0x090) | 8 | ticket_ceiling | U64 | J=2^64−N−1; no represented cursor may exceed J |
| 152 (0x098) | 8 | epoch_ceiling | U64 | G=2^62−2 |
| 160 (0x0A0) | 16 | session_id | B[16] | Nonzero opaque generation identity, never reused within an outstanding-reference domain |
| 176 (0x0B0) | 16 | host_instance_id | B[16] | Nonzero live-host/cohort identity from the trusted authority |
| 192 (0x0C0) | 16 | authority_instance_id | B[16] | Nonzero identity of the managing authority instance |
| 208 (0x0D0) | 8 | authority_epoch | U64 | Nonzero, nonwrapping grant epoch within that authority instance |
| 216 (0x0D8) | 16 | predecessor_session_id | B[16] | All zero for initial generation; otherwise predecessor identity |
| 232 (0x0E8) | 8 | creation_utc_ns | U64 | Diagnostic UTC nanoseconds since Unix epoch, or 0 if unavailable; never a freshness witness |
| 240 (0x0F0) | 8 | max_wait_slice_ns | U64 | 0 for polling; 10,000,000 for PARKABLE_SPSC |
| 248 (0x0F8) | 8 | max_backing_bytes | U64 | Cohort M_max; ≥segment_bytes, fixed across a replacement cohort |
| 256 (0x100) | 4 | producer_endpoints | U32 | P_e≥1 |
| 260 (0x104) | 4 | consumer_endpoints | U32 | C_e≥1; P_e+C_e=K |
| 264 (0x108) | 4 | max_backing_objects | U32 | 4 |
| 268 (0x10C) | 4 | max_quarantined_objects | U32 | 2 |
| 272 (0x110) | 240 | reserved_110 | B[240] | All zero |

The immutable prefix has no unassigned byte. The U64 members are eight-byte aligned, including `authority_epoch` and the timestamp after its sixteen-byte identity field. The whole prefix occupies four isolation cells. It contains no mutable cursor, reference count, heartbeat, or status word.

SPSC requires P_e=C_e=1 and K=2. NCQ admits multiple producers/consumers but not a combined/reentrant endpoint role in this version. `wait_mode=1` with NCQ is invalid. Unknown versions, feature values, reserved bits, or nonzero reserved prefix bytes are rejected rather than guessed.

### 2.1 The magic is bytes, not a host-endian integer literal

The eight ASCII characters are the authoritative magic. The displayed numeric `0x454C495445495043` lists these bytes from most significant to least significant. Storing that numeric U64 through a native little-endian store would reverse the bytes and produce the wrong magic. A little-endian U64 interpretation of the required eight bytes would instead be `0x4350494554494C45`.

The ABI version is a numeric U32 and therefore appears as `00 00 01 00`. No terminating NUL is part of the magic. Readers compare the eight-byte sequence and separately decode numeric fields.

---

## 3. The two named ring headers

### 3.1 `struct elite_spsc_ring_header`

**Exact size: 2048 bytes. Exact alignment: 128 bytes. Exact array stride, if an array is ever formed: 2048 bytes.** The mapping contains one such header, not a variable-length C object pretending to include the later arrays.

| Outer offset | Extent | Member | Definition and ownership |
|---|---|---|---|
| 0 | [0,512) | immutable | Common prefix from §2 |
| 512 | [512,640) | admission | Cell64: admission/lifecycle word at 512; reserved bytes [520,640) |
| 640 | [640,768) | failure | Cell64: first-failure word at 640; reserved bytes [648,768) |
| 768 | [768,896) | published | Cell64: AU64 P at 768; reserved bytes [776,896); producer writes only |
| 896 | [896,1024) | reclaimed | Cell64: AU64 C at 896; reserved bytes [904,1024); consumer writes only |
| 1024 | [1024,1152) | data_wait | Cell32: AU32 wait state at 1024; reserved bytes [1028,1152) |
| 1152 | [1152,1280) | space_wait | Cell32: AU32 wait state at 1152; reserved bytes [1156,1280) |
| 1280 | [1280,2048) | reserved_500 | B[768], immutable zero |

P and C start at zero. Owner-local working cursors, acquired peer snapshots, active-call references, and outstanding lease state do not move into these cells. The consumer reads P; the producer reads C. Padding isolates independent writers, not this intentional sharing.

In POLL_ONLY, both wait words are initialized atomic zero and thereafter remain unused and zero. No notifier RMW may appear secretly in that profile. In PARKABLE_SPSC, the words follow §12 and their added cost is part of that profile.

### 3.2 `struct elite_mpmc_ncq_header`

**Exact size: 2048 bytes. Exact alignment: 128 bytes.** Its common prefix and two initial control cells occupy the same offsets as the SPSC header, but its queue cursors are different members with different semantics.

| Outer offset | Extent | Member | Definition and ownership |
|---|---|---|---|
| 0 | [0,512) | immutable | Common prefix from §2 |
| 512 | [512,640) | admission | Cell64: admission/lifecycle word; reserved [520,640) |
| 640 | [640,768) | failure | Cell64: first-failure word; reserved [648,768) |
| 768 | [768,896) | qf_head | Cell64: QF removal frontier H_F; reserved [776,896) |
| 896 | [896,1024) | qf_tail | Cell64: QF publication hint T_F; reserved [904,1024) |
| 1024 | [1024,1152) | qr_head | Cell64: QR removal frontier H_R; reserved [1032,1152) |
| 1152 | [1152,1280) | qr_tail | Cell64: QR publication hint T_R; reserved [1160,1280) |
| 1280 | [1280,1408) | reserved_data_wait | Cell32: atomic zero at 1280, inactive; reserved [1284,1408) |
| 1408 | [1408,1536) | reserved_space_wait | Cell32: atomic zero at 1408, inactive; reserved [1412,1536) |
| 1536 | [1536,2048) | reserved_600 | B[512], immutable zero |

All four cursors and every QF/QR entry use the NCQ-SC64 orders in §9. Initially H_F=0, T_F=N, H_R=N, T_R=N. The inactive wait cells reserve format space only. They do not authorize MPMC parking, waiter counts, or a future protocol upgrade in an existing segment.

### 3.3 C11 alignment obligations, without a declaration block

A later header must realize 128-byte alignment through a C11-supported alignment specifier on suitable members/objects, for example by imposing `_Alignas(128)` on the first member of an aggregate and verifying the resulting type. C11 `alignas` is the macro spelling supplied by its alignment header; C++'s placement of `alignas` in a struct declaration must not be copied blindly into C11 syntax. Do not apply a C11 alignment specifier to a typedef declaration or use packing to force these layouts. [R01, §6.7.5]

Every table row becomes an explicit member or explicit reserved array. The future `_Static_assert` set must verify each member offset, scalar size, atomic size/alignment, aggregate alignment, total size, and nested-cell extent. It must assert the actual types used, not only platform macros. Runtime mapping alignment and range validation are still necessary; compile-time assertions cannot repair an arbitrary supplied address.

---

## 4. Slot descriptor, queue entry, and endpoint record

### 4.1 `struct elite_slot_descriptor`

**Exact size: 128 bytes. Exact alignment: 128 bytes. Descriptor i begins at descriptors_offset+128i.**

| Relative offset | Bytes | Member | Type | Contract |
|---|---:|---|---|---|
| 0 | 8 | epoch | U64 | Current per-block lease epoch; ordinary owner-protected mirror; 0 initially |
| 8 | 8 | status_word | AU64 | NCQ authoritative packed epoch/phase; dormant zero in SPSC |
| 16 | 4 | payload_length | U32 | Valid payload prefix length, 0≤length≤B |
| 20 | 4 | message_type | U32 | Application type identifier; no implicit native enum layout |
| 24 | 8 | checksum | U64 | 0 in NONE mode, otherwise CRC64 specified in §7 |
| 32 | 8 | message_id | U64 | Application-supplied identifier, 0 allowed; not a queue ticket or guaranteed global identity |
| 40 | 88 | reserved_028 | B[88] | All zero, immutable after construction |

This provides both a physically 64-bit epoch field and the preceding protocol's single-word atomic epoch/phase witness. It does not introduce a 128-bit atomic pair. The permitted epoch range is 0…G with G=2^62−2. A demand to use all 64 bits independently while atomically combining them with phase would require a different representation/proof; this version does not claim it.

For NCQ, phase values are EMPTY=0, RESERVED=1, COMMITTED=2, CONSUMED=3, and:

\[
\operatorname{status}(g,s)=4g+s,\qquad 0\le g\le G,\quad 0\le s\le3.
\]

The upper 62 bits identify g. At each externally observable ownership transition, `epoch` agrees with g. During a writer's exclusive setup there can be a temporary mismatch while it updates the ordinary mirror and then the atomic status; no unowned reader may use that interval as a snapshot. The 16-byte pair is never claimed to be atomically read.

On a QF claim, the producer owns the token before reading the old epoch/status or any other mutable descriptor member. It requires EMPTY(g), advances to g+1 within the ceiling, updates the mirror, and release-stores RESERVED(g+1). It then owns the writable payload. Commit finalizes length/type/id/checksum, ends all writable aliases, release-stores COMMITTED(g+1), and starts QR insertion. A consumer reads descriptor/payload only after winning QR head CAS; it verifies the expected phase/epoch/length and release-stores CONSUMED. Its last borrow ends before EMPTY and QF return. [P2, §§6.9–6.10]

A state mismatch is an integrity/lifecycle error, not permission to wait at a RESERVED block behind the FIFO head, reconstruct a lost owner, or insert a second token. No background scanner is allowed to read ordinary epoch/length/checksum while they may be written. Checking `status_word` afterward does not cure that race.

For SPSC, status_word is initialized atomic zero and never read or changed by the fast path. P/C carry ownership. The exclusive producer advances the ordinary epoch on every new write reservation, including a new reservation after abort; abort ends all aliases and does not publish P. Consumers read the published epoch only after acquiring P. Any exported lease validation uses the native handle's ownership first, not a speculative descriptor read through a stale unowned handle.

### 4.2 Exact queue-entry definition

`elite_ncq_entry_cell` has size/alignment 128. Its AU64 `cycle_index` occupies [0,8); immutable B[120] occupies [8,128). Every distinct physical entry has its own complete isolation cell.

Let N=2^n and m=N−1. For logical ticket t and payload-block index b:

\[
\operatorname{pos}(t)=t\bmod N,\qquad
\operatorname{entry}(t,b)=N\left\lfloor t/N\right\rfloor+b,\quad 0\le b<N.
\]

The low n bits encode b; the remaining bits encode the cycle base. This is arithmetic packing into one AU64, not C bit-fields and not a pointer. Each physical QF entry i is initialized to i, giving cycle zero and block i. Every physical QR entry is initialized to zero, giving cycle zero and a meaningless placeholder index until the expected cycle is installed.

QR's initial H_R=T_R=N makes those placeholders empty. QF's H_F=0, T_F=N makes its N initialized entries full. An entry's historical low bits are never independent evidence that the token is still a member of that queue.

### 4.3 Endpoint registration record

`elite_participant_record` has size 256 and alignment 128. It intentionally separates immutable grant metadata from the mutable attach-state cell.

| Relative offset | Bytes | Member | Type | Contract |
|---|---:|---|---|---|
| 0 | 16 | endpoint_id | B[16] | Nonzero, unique among this session's endpoint grants |
| 16 | 16 | process_incarnation_id | B[16] | Authority-issued opaque identity bound externally to a live process handle |
| 32 | 4 | endpoint_index | U32 | i in [0,K); must equal its physical index |
| 36 | 4 | role | U32 | 1=producer; 2=consumer |
| 40 | 4 | max_outstanding_tokens | U32 | 1, including native calls/transfers and exported borrows |
| 44 | 4 | flags | U32 | 0 |
| 48 | 8 | grant_epoch | U64 | 1; this physical endpoint slot is never reused in this generation |
| 56 | 72 | reserved_038 | B[72] | Immutable zero |
| 128 | 128 | attachment | Cell64: attach_state at relative 128; reserved [136,256) |

All first-cell metadata is set before object exposure. The authority preassigns process/endpoint incarnations and records the exact OS process identities before issuing grants. An identifier in this table is not a stored pidfd or a Mach capability. Authority-side OS handles remain local to their owning process.

Attachment states are NEW=0, JOINING=1, ACTIVE=2, QUIESCENT=3, REJECTED=4. NEW→JOINING uses strong SC CAS. Success is a one-shot claim of the grant record, not yet a successful ring attachment. Later state stores are release; observers use acquire. No participant writes another endpoint's record. A coordinator never turns a dead or detached record back to NEW in the same generation.

One process may own several records, but each record is single-owner/nonreentrant with at most one token. K counts records, not PIDs. The per-record limit includes an interrupted native publication path; a callback cannot silently initiate another lease. The registry is enrollment evidence, not a crash-consistent per-token ownership log.

---

## 5. Admission word, failure word, and lifecycle states

### 5.1 One atomic admission/close word

The AU64 admission word at object offset 512 encodes:

\[
A_{\mathrm{gate}}=(c\cdot 2^{32})+s,
\]

where c is an unsigned 32-bit enrollment count, 0≤c≤K, and s is an eight-bit state. Bits 8…31 are zero. No C bit-field is used. An operation replacing this word must preserve the other component and validate bounds.

| State number | Name | Meaning |
|---:|---|---|
| 0 | BUILDING | Atomic objects exist only after builder construction; no grant is issued |
| 1 | READY | New one-shot enrollments may succeed; admitted operations may start |
| 2 | RETIRE_REQUESTED | New enrollments forbidden; previously admitted calls may still finish |
| 3 | QUIESCING | Authority has requested local stopping/alias closure; completion not yet assumed |
| 4 | QUARANTINED | Not a service target; some holders may remain unfenced; never reset/reuse |
| 5 | SEALED | Authority has completed the global no-future-access proof in §11; no queue operation permitted |

Initial construction establishes BUILDING,count=0. The builder's single release publication changes it to READY,0 after all initialization and CRC completion. Every later modification is strong SC CAS. READY never reappears after retirement. A retirement request may be made by an enrolled endpoint or the authority; later lifecycle promotions are authority-only. QUARANTINED is not a stronger memory fence than QUIESCING; it records a management decision to retain storage.

Physical deletion has no stored RECLAIMED state: once storage is gone there is no shared word to read or update. The external authority tracks STAGED, READY_SUCCESSOR, and RECLAIMED object-management states separately. The session's data gate is not overwritten with a successor's identity.

### 5.2 First-failure word

The AU64 failure word starts at zero. Its first nonzero value is retained with a strong SC CAS from zero. Values are: 1=INTEGRITY, 2=COUNTER_LIMIT, 3=PEER_FAILURE, 4=PROTOCOL_VIOLATION, 5=RESOURCE_LIMIT, 6=AUTHORITY_FAILURE. The entire value is the code; all unspecified values are invalid. No packed PID or timestamp is hidden in unused bits.

A reporting endpoint must also request retirement through the gate. Failure reporting and retirement are not atomic together; a crash between them is possible. The authority treats either observation as a reason to stop further grants. The word is not a recovery transaction or a guarantee that all peers have stopped touching data.

### 5.3 Why a reference count is not sufficient

Enrollment protects ordinary live attachment bookkeeping. It does not count every pending grant, map-open attempt, diagnostic snapshot, inherited mapping, external pointer, or failed participant's unknown transfer state. The authority records every potential holder before exposing a backing name/descriptor. A zero enrollment count alone never authorizes truncation or reclamation.

A participant may die after incrementing the gate and before recording ACTIVE, or after recording QUIESCENT but before decrementing it. Overcounting is deliberately tolerated until global fencing; another process must not guess which update occurred and subtract opportunistically. These are lifecycle crash gaps, not defects that a second counter fixes automatically.

---

## 6. Complete backing-object geometry and padding policy

### 6.1 Uniform format quantum

Q=16384 is fixed in LE128-V1 to give identical region offsets on the two requested target families. Require actual OS base page size V to be a power of two, A≤V≤Q, and Q divisible by V. Supported examples include V=4096 and V=16384. A 64-KiB-page platform is not admitted to this format; a future format must state its different quantum.

The mapped base must be A-aligned. It need not be Q-aligned if the OS supplies a smaller-page-aligned base: the relevant facts are within-page atomic alignment and Q-aligned file-region offsets. No MAP_FIXED address selection, huge-page request, or same-virtual-address assumption is necessary.

### 6.2 Canonical region formulas

Let S=RU(B,128). The participant region begins at Q and has K records of 256 bytes. Let:

\[
O_0=\operatorname{RU}(Q+256K,Q).
\]

For SPSC:

\[
O_D=O_0,\quad O_P=\operatorname{RU}(O_D+128N,Q),\quad
M=\operatorname{RU}(O_P+NS,Q).
\]

Its QF/QR offsets and queue-entry stride are zero.

For NCQ-SC64:

\[
\begin{aligned}
O_F&=O_0,\\
O_R&=\operatorname{RU}(O_F+128N,Q),\\
O_D&=\operatorname{RU}(O_R+128N,Q),\\
O_P&=\operatorname{RU}(O_D+128N,Q),\\
M&=\operatorname{RU}(O_P+NS,Q).
\end{aligned}
\]

The prefix stores these exact offsets and M. Merely being in bounds is insufficient: a value that differs from the canonical equation is BAD_LAYOUT. This prevents accidental overlaps, reinterpretation of one array as another, and an incompatible variant disguising itself behind the same ABI number.

The header occupies [0,2048). [2048,Q) is reserved zero format space. Registry trailing slack, inter-region alignment gaps, entry/descriptor reserved ranges, and [O_P+NS,M) are zero-initialized and remain reserved. Payload block i occupies [O_P+iS,O_P+(i+1)S). Only its first B bytes are payload capacity; [B,S) within each block is immutable zero tail padding.

A message exposes only its valid `payload_length` prefix, not B or S bytes. Previously used payload bytes outside that prefix may contain old application data; the API must never return them as current content. This is not an isolation boundary against a malicious peer with a writable mapping of the entire object. Payloads are byte spans: this transport does not make an arbitrary application C struct portable. The application must define the schema selected by message_type, including its own endian/padding rules, and must not place process-local pointers in purportedly transferable payload structures.

### 6.3 Arithmetic and native-address admission

Before pointer formation, validate N, B, K, role counts, every multiplication, every roundup, and every addition without overflow. Require M to fit the supported positive `off_t`, `size_t`, and `ptrdiff_t` range, the configured M_max, and the actual backing length. An implementation must not perform an overflowing native calculation and then compare the wrapped result with a bound.

For a range [o,o+l), validate o≤M and l≤M−o. Then validate its alignment and canonical role. No negative signed offsets, high-bit casts into negative `off_t`, partially mapped arrays, or pointer wrap are admitted. Header validation does not prevent a malicious peer from later truncating or corrupting a writable object; nonmalicious participants and controlled backing ownership remain model assumptions.

### 6.4 Worked layout, not an allocated ring

For N=1024, B=64, and S=128:

| Region/property | SPSC, K=2 | NCQ, K=16 (8 producers + 8 consumers) |
|---|---:|---:|
| Header object bytes | 2048 | 2048 |
| First format quantum | [0,16384) | [0,16384) |
| Participant records | [16384,16896) | [16384,20480) |
| First array offset O_0 | 32768 | 32768 |
| QF entries | Absent | [32768,163840) |
| QR entries | Absent | [163840,294912) |
| Descriptors | [32768,163840) | [294912,425984) |
| Payload blocks | [163840,294912) | [425984,557056) |
| Total M | 294912 = 288 KiB | 557056 = 544 KiB |
| Page count when V=4096 | 72 | 136 |
| Page count when V=16384 | 18 | 34 |

The footprint on a 4-KiB-page x86 machine is intentionally larger than Turn 2's compact 64-byte-isolation example. The new request for one identical 128-byte layout is paid for openly. No speed improvement is claimed from that tradeoff.

### 6.5 No-split and no-false-sharing proof

For any atomic width w∈{4,8}, its field offset and aggregate base are multiples of w. For an admitted granularity g∈{64,128}, w divides g, so an aligned w-byte access cannot cross a g-byte boundary. The same reasoning applies to admitted page boundaries. For independently accessed cells, all starts and extents are multiples of 128; no admitted 64/128-byte line crosses a cell boundary. The physical mapping preserves these within-page offsets.

This proves placement, not arbitrary multiword atomicity or full cache behavior. In particular, `epoch` and `status_word` are not an atomic pair; payload handoff and reads of another endpoint's cursor remain true sharing. Unknown larger coherence granularities require new admission evidence. [P2, §2; P3, §5]

---

## 7. Header CRC32, payload checksum, and integrity limits

### 7.1 Header CRC32 — exact algorithm and coverage

Freeze CRC-32/ISO-HDLC parameters: width 32; normal polynomial `0x04C11DB7`; reflected polynomial `0xEDB88320`; initial register `0xFFFFFFFF`; reflected input/output convention; final XOR `0xFFFFFFFF`. For ASCII `123456789`, the required check value is `0xCBF43926`; the empty input result is zero. The polynomial/reflected calculation corresponds to the CRC family illustrated in RFC 1952. [R16]

Input is exactly the immutable 512-byte prefix in increasing address order, with bytes [52,56) replaced by zero **in the calculation**, not by writing zeros into a live mapping. The finalized CRC result is stored as U32 at offset 52. No mutable control, participant attachment word, queue entry, descriptor, or payload is included. The prefix's reserved bytes are included and required to be zero.

Validation copies the already-published immutable prefix or streams its bytes through the checksum; it must not memcpy the whole 2048-byte live header. Header size is 2048 but CRC coverage is explicitly 512. Reading `header_bytes` does not redefine the coverage.

### 7.2 Optional payload CRC64

Mode 0=NONE requires the checksum field to be zero and performs no payload checksum work. Mode 1=CRC64_ECMA182 uses width 64, polynomial `0x42F0E1EBA9EA3693`, initial register zero, no input or output reflection, and final XOR zero. The check value for ASCII `123456789` is `0x6C40DF5F0B497347`; empty input is zero. ECMA-182 Annex B supplies the polynomial remainder construction; the parameter/check convention is also explicit in the algorithm catalog cited below. [R17, R18]

The input is exactly the valid payload bytes [0,payload_length), ordered by address. Length, type, ID, epoch, and padding are not prepended. The U64 checksum is stored little-endian; storage endianness does not change the non-reflected checksum algorithm. An implementation must not substitute a reflected CRC64 variant with the same polynomial or a hardware CRC32-C operation and keep this mode identifier.

The producer computes the checksum before publication. The authorized consumer validates length before indexing and validates the checksum while it owns the read lease. A mismatch retires the generation as an integrity failure; it does not authorize stealing or republishing the block. The NONE and CRC64 profiles require separate latency results because scanning the payload costs work.

### 7.3 What integrity checks cannot prove

A CRC does not authenticate a peer, prove process death, validate a race after it occurred, or show that an atomic field was constructed. A correct CRC on an old session remains an old session. A matching checksum on a payload copied without ownership does not legalize the read. The descriptor's mirrored epoch is validation metadata, not a substitute for queue ownership. The SHA-256 receipts of this design document are separate from both runtime checksum mechanisms.

---

## 8. Per-operation semantics and memory orders

### 8.1 SPSC operations

A producer reserve validates its local handle, observes the lifecycle with acquire semantics, checks an acquire-obtained C snapshot against its local P, and, when space is authorized, returns the next writable span. It does not modify P or contend on a shared reservation counter. Epoch advancement is exclusive owner work. At most one write lease is outstanding. A conservative stale snapshot may return NO_CAPACITY_OBSERVED. [P2, §4]

Commit finishes descriptor/payload work, ends writable aliases, and release-stores P+1. That store is publication's LP. After the LP, the old producer path may touch local state and required notification controls only, never the transferred descriptor or payload. A consumer may have recycled the block before commit returns.

A consumer borrow uses a covering acquire observation of P and retains C until every view ends. It performs no shared read-reservation RMW. Release finishes all reads and release-stores C+1. A stale P observation may produce NO_DATA_OBSERVED. Abort of an unpublished write ends all aliases but does not change P; the next reservation uses a new epoch without making that abort a message.

The ownership chains remain:

\[
W_k\to_{sb}P_{release}\to_{sw}P_{acquire}\to_{sb}R_k,
\]

\[
R_{k,last}\to_{sb}C_{release}\to_{sw}C_{acquire}\to_{sb}W_{k+N}.
\]

A lifecycle read is not the payload acquire. A descriptor checksum is not the reclamation release. The finite try path has no retrying queue CAS; the parkable wrapper does not inherit that primitive profile.

### 8.2 NCQ operations

Every load and strong CAS on H_F, T_F, H_R, T_R, and either entry array is sequentially consistent. Both the successful and failed memory order of every queue CAS are SC. This is the named reference, not an acquire/release optimization variant.

An enqueue observes the current tail and matching physical entry. Exactly one older cycle permits installation of the caller's completed token. A matching current cycle means already published: help advance tail and restart classification. A different cycle means stale observation. Successful full-word entry installation is the publication LP; tail assistance follows and never writes the payload.

A dequeue captures head, reads its expected-cycle entry, and attempts a strong CAS advancing head. Success transfers that captured index. Failure invalidates the captured ticket, entry, desired head, and block index as one observation set. The fact that compare/exchange updates its expected-value argument is not permission to reuse the old dependent descriptor. A matching one-older cycle justifies empty in the SC history; a future mismatch requires retry, not an invented empty result. [P2, §6; P3, §7]

Publication-CAS failure likewise triggers reclassification. Retrying against a competitor's newly returned current-cycle entry could overwrite an already published message. The automatic expected-value update is an output of a failed atomic operation, not authorization to replace that output. [R01, §7.17.7.4; R04]

### 8.3 Complete critical-atomic ledger

| ID | Operation/object | Width | Order | Meaning |
|---|---|---:|---|---|
| A01 | Builder READY publication, gate | 64 | release store | Completes initialization before exposure |
| A02 | Bootstrap/data-entry gate observation | 64 | acquire load | Initialization/lifecycle observation, not slot ownership |
| A03 | Enrollment, retirement, detach, lifecycle promotion gate CAS | 64 | strong SC / SC failure | Atomic state/count update |
| A04 | First-failure CAS | 64 | strong SC / SC failure | Record first reason, without repairing any slot |
| A05 | Participant NEW→JOINING CAS | 64 | strong SC / SC failure | Claim a one-shot grant record |
| A06 | Participant ACTIVE/QUIESCENT/REJECTED store; observation | 64 | release / acquire | Enrollment evidence, not a token journal |
| S01 | SPSC peer-cursor observation | 64 | acquire load | Covering payload availability or safe reuse |
| S02 | SPSC P commit / C release | 64 | release store | Publication / reclamation LP |
| S03 | SPSC private reserve | none | none | No CAS, FAA, ticket lock, or shared reservation |
| N01 | NCQ head/tail/entry observation | 64 | SC load | Reference consistent queue state |
| N02 | NCQ head claim CAS | 64 | strong SC / SC failure | Token dequeue LP |
| N03 | NCQ entry installation CAS | 64 | strong SC / SC failure | Token enqueue LP |
| N04 | NCQ tail assist CAS | 64 | strong SC / SC failure | Helpable bookkeeping |
| D01 | NCQ descriptor status transition | 64 | release store | Owner's phase/epoch publication |
| D02 | NCQ descriptor status validation after ownership | 64 | acquire load | Verify expected phase; not membership proof |
| W01 | All SPSC wait-word mutations, including same-value | 32 | acq_rel exchange | Preserve wait-word synchronization chain |
| W02 | Kernel expected-value wait/wake | 32 location | OS contract plus userspace orders | Scheduling adapter, not payload synchronization |
| X01 | Live slot revocation | absent | no valid fence mapping | Not supported |

Status changes are stores by the exclusive token owner, not a new CAS contest among scanners. READY must not be used to justify arbitrary non-atomic reads of mutable fields. The report's complete protected-range ownership rules still apply.

---

## 9. Instruction-level lowering contracts

### 9.1 What is exact here

For each named source operation, this section fixes required ordering and admissible baseline instruction forms. It does not claim the exact emitted bytes, registers, branches, helper calls, or optimization choices of a compiler invocation that has not occurred. An implementation must be checked against this contract on each target tuple; an unexpected but semantically valid lowering needs explicit admission rather than a fabricated earlier verification. [R02–R05]

Register-width notation: a 64-bit AArch64 operation uses X data registers; a 32-bit operation uses W data registers; the store-exclusive result is a W register. x86 operations use the corresponding 64/32-bit operand size. Only aligned normal CPU memory is covered.

### 9.2 Load, store, exchange, and explicit-fence forms

| Source semantic operation | AArch64 baseline without LSE | AArch64 with admitted LSE | x86-64 baseline |
|---|---|---|---|
| relaxed load/store | LDR / STR | LDR / STR | MOV / MOV |
| acquire load | LDAR; legal RCpc target mapping may use LDAPR | Same load choice; LSE is not the RCpc feature | MOV plus compiler ordering constraints, no required extra instruction |
| release store | STLR | STLR | MOV plus compiler ordering constraints |
| SC load | LDAR in the selected mapping | LDAR in the selected mapping | MOV |
| SC store, if introduced | STLR in a compatible complete mapping | STLR in a compatible complete mapping | Common mapping XCHG, or MOV with appropriate MFENCE mapping |
| acq_rel exchange of wait state | LDAXR/STLXR exclusive retry construction | SWPAL | Memory XCHG; lock semantics are implicit |
| acquire thread fence | DMB ISHLD | DMB ISHLD | Compiler ordering only in the usual mapping |
| release/acq_rel thread fence | DMB ISH | DMB ISH | Compiler ordering only in the usual mapping |
| SC thread fence | DMB ISH | DMB ISH | MFENCE or a properly placed locked operation in a validated complete mapping |

The final four rows describe explicit fence operations, not extra instructions to insert after every table entry. No explicit thread fence is required by this version's ordinary SPSC publication/reclamation path. NCQ's SC operations must be lowered as a mutually compatible mapping; isolated mnemonics are not a proof of the entire language memory model. [R02]

`LDAR` is the stronger RCsc acquire; `LDAPR` is RCpc acquire. Neither name changes a source acquire into an SC source operation. RCpc support and LSE support are distinct target capabilities. Accepting LDAPR for an eligible acquire does not authorize its substitution for the reference's SC queue loads. [R03]

DMB ISH orders relevant accesses in the inner-shareable domain. DMB SY is the full-system-domain spelling. DMB is not a payload flush-to-DRAM or a persistent commit, and it is not an all-purpose speculative-execution security boundary. [R06]

### 9.3 Strong compare/exchange matrix

The following success/failure pairs are complete examples for admission comparison; only the last row is used for NCQ queue CAS and gate/registration CAS in this version.

| Source success / failure | AArch64 LSE form | AArch64 exclusive form | x86-64 |
|---|---|---|---|
| relaxed / relaxed | CAS | LDXR + conditional STXR | LOCK CMPXCHG |
| acquire / acquire | CASA | LDAXR + conditional STXR | LOCK CMPXCHG |
| release / relaxed | CASL | LDXR + conditional STLXR | LOCK CMPXCHG |
| acq_rel / acquire | CASAL | LDAXR + conditional STLXR | LOCK CMPXCHG |
| seq_cst / seq_cst | CASAL in the admitted complete SC mapping | LDAXR + conditional STLXR in the admitted complete SC mapping | LOCK CMPXCHG |

AArch64 `LDREX`/`STREX` are not admissible A64 mnemonics. The exclusive form compares the observed value, stores only on a match, and for a **strong** CAS internally retries an exclusive-store failure instead of exposing that failure as a spurious source-level mismatch. An observed comparison mismatch can return failure and updates the caller's expected value. Exclusive monitor cleanup and exact branch structure are compiler responsibilities, not an implementation supplied here.

LSE CASAL is an indivisible compare/exchange instruction, but contention can still produce comparison failure and queue-level retries. An LSE build cannot run on a target lacking the selected feature unless its audited dispatch selects a valid fallback. No use of CASP or a pair of independent scalar operations may silently substitute for the specified single AU64 object. [R02, R04, R05]

On x86, LOCK CMPXCHG uses the implicit accumulator comparison/result convention; a later implementation must preserve its observed failure value semantics. On AArch64 LSE, the expected operand also receives the observed memory value. A source-level “strong” operation does not have an individual queue completion deadline on either architecture.

### 9.4 Outlined helpers and why one disassembly example is not enough

An atomic helper can be legitimate if it selects admitted hardware primitives without process-local state that breaks aliasing. It can also be fatal if it falls back to a process-local lock. Compiler flags, runtime feature dispatch, and helper version are part of the binary record.

LLVM's current AArch64 helper source includes acquire/release/acq_rel variants and a `_sync` path with an additional DMB ISH. Consequently “the source says SC so there can never be a DMB” is not a valid binary prediction. Nor is every helper call evidence of a lock. The eventual audit must inspect the selected path and explain its order/feature contract. [R05]

### 9.5 Operation-specific lowering consequences

SPSC reserve has no atomic write-reservation instruction to lower. Its covering cursor read maps through S01; publication maps through S02. NCQ payload reservation is N02 on QF, while message publication is N03 on QR. Treating these as one claim-first tail operation recreates the rejected architecture.

The 64-bit descriptor status store uses STLR or the x86 release MOV mapping. The ordinary mirror, length, checksum, and payload writes need not be individually atomic because ownership and subsequent publication order them. The compiler must not widen two independent AU64 accesses into an unproved atomic pair, treat an AU64 as unaligned storage, or mix atomic and ordinary accesses to a live control word.

### 9.6 No “full barrier for slot revocation”

There is no X01 implementation based on DMB ISH, MFENCE, LOCK, cache flush, checksum update, or status transition. A delayed process can execute a future ordinary store through its already acquired pointer after any such operation performed by another core.

The supported operations are **retirement**, **cooperative alias closure**, **confirmed fencing**, and **eventual reclamation after proof**. Their shared records use A03/A06 and the relevant OS/control-plane evidence. A fence may order an actor's evidence publication; it is not the evidence that the old writer has lost access. Similarly, mprotect in one task does not revoke another task's mapping. [P3, §§3–4; R08]

---

## 10. POSIX backing and initialization contract

### 10.1 Naming and backing identity

Use a fresh POSIX shm name of the form `/el-` followed by 26 lowercase base32 characters encoding the 128-bit session identity, without `=` padding. Treat the identity as a 128-bit bit string in stored-byte order; append two zero low-order padding bits to make 26 five-bit groups. The alphabet is `abcdefghijklmnopqrstuvwxyz234567`. The result has 30 characters including the leading slash. It fits the examined Darwin name limit without assuming Linux's longer allowance. [R07, R09, R10]

A name collision produces CREATE_CONFLICT and no truncation. The authority chooses another previously unused identity; it never opens an existing object with truncation and calls that initialization. Nonces reduce collision probability but do not prove mathematical uniqueness: exclusive creation and the authority's no-reuse ledger are the operative identity safeguards.

Linux commonly implements POSIX shared memory under `/dev/shm`; the API contract is the shm name and returned file descriptor, not string concatenation with that path. Darwin uses its POSIX shared-memory facility and is not assigned a `/dev/shm` pathname. A Linux memfd/hugetlb variant is a different backing adapter requiring an explicit capability/bootstrap contract; it is not silently substituted for this profile. [R07, R09]

### 10.2 Single builder and registration-before-exposure

The authority reserves one of its bounded object records, including intended name, identity, and maximum bytes, **before creating** the OS object. A failed or incomplete allocation retains that record until its outcome is reconciled. The builder performs exclusive create with owner-only permissions appropriate to the admitted same-user cohort, sets the exact size once, maps the complete object MAP_SHARED, and establishes every ordinary/atomic object's lifetime.

Every reserved range, active payload capacity, and payload tail padding is initially zero. Each atomic is individually initialized to its defined value before any participant can access it. Bulk zeroing raw newly owned storage is not a substitute for the admitted atomic-construction procedure, and no initialized live atomic object is reset with a later whole-structure memset.

The builder fills immutable headers and one-shot participant grants; initializes queue arrays and controls; computes the header CRC; then release-publishes READY. Only after that does the authority deliver the **constructed-object grant**. The grant includes the expected atomic ABI and fixed gate location. Its construction guarantee comes from the managed bootstrap, not from the attacher peeking at bytes that may still be under construction.

The grant is delivered through an already established application control plane or owned-child bootstrap mechanism. No payload-message transport is moved into that channel. The authority must keep its actual implementation bounded and protect grant recipients/process identities before it reveals a name or transferable object reference. There is no mandatory external daemon, and no claim that initialization becomes lock-free because the steady-state queue is.

If the builder dies before exposure, do not attach to a guessed filename and spin on an unconstructed READY word. The authority reconciles or discards the unpublished candidate. Its record remains charged to the four-object cap until that cleanup is known complete.

### 10.3 Constructed-object grant contents

A grant is an out-of-band typed message, not another shared-memory struct silently included in the file format. It must carry: session_id; host_instance_id; authority_instance_id and epoch; exact backing identity/name or admitted descriptor; expected segment length and immutable CRC; layout profile and atomic ABI; endpoint index, endpoint_id, process_incarnation_id, role, and grant_epoch; the permission to attempt exactly one enrollment; and the identity of the currently responsible authority channel.

The grant's representation depends on that already owned application channel. Its semantics are fixed here. An untrusted filename, CRC, timestamp, PID, or stale command-line argument is not a grant. Unknown authority liveness is AUTHORITY_REQUIRED/UNKNOWN, not a reason to steal the old session.

---

## 11. Atomic attach, retirement, detach, and stale-session handling

### 11.1 Attach sequence and validation order

**Precondition:** the process has a live constructed-object grant, an admitted local atomic tuple, and a native local handle under construction. The authority has recorded that process as a potential holder before supplying the grant. This external obligation covers crashes even before enrollment increments the shared count.

1. Open the granted existing object without O_CREAT or O_TRUNC, or use its admitted existing descriptor. Validate ownership/permissions and fstat length against the grant. A failed open never creates a replacement. Require at least Q bytes and exact expected M before mapping the whole object.
2. Map MAP_SHARED with the required read/write access. Verify the returned base alignment. Use the grant's established construction/atomic-ABI contract to acquire-load the **fixed** gate at offset 512. Do not read an arbitrary offset supplied by unvalidated file bytes. READY or a valid later state carries initialization through the release/RMW chain; BUILDING, an invalid state, or a mismatched granted ABI rejects attachment.
3. Decode the immutable prefix, validate exact version and byte magic, constant values, reserved zeros, header CRC, canonical arithmetic, bounds, and all session/authority identities against the live grant. Copy the validated immutable configuration into local handle state. Do not snapshot mutable header cells as part of the CRC.
4. Validate the granted participant record's immutable first cell, including physical index, opaque endpoint/process identities, role, one-token limit, and grant epoch. The grant supplies the expected record identity; a neighboring slot is not an alternative if this one is unavailable.
5. Strong-SC-CAS its attachment state NEW→JOINING. Failure is a duplicate/invalid grant, not permission to reuse that record or borrow another one.
6. Strong-SC-CAS the gate from READY,c to READY,c+1 with c<K. Re-evaluate the complete returned gate on each failure. Any non-READY state rejects enrollment. The successful increment is the **attach LP**. Publish the record ACTIVE with release semantics only after this increment.
7. Acquire-recheck lifecycle before returning a usable endpoint. If retirement raced after the LP, return RETIRED with a locally owned enrollment needing orderly detach; do not lose the enrollment because the public attach was not usable. No queue operation begins until all validations and role initialization are complete.

When step 6 rejects without incrementing, the claimant sets its record REJECTED and closes its local mapping after any pending local access ends. If a process crashes in JOINING, the authority retains its potential-holder record; shared phase alone cannot decide whether to decrement the gate.

### 11.2 Attach-versus-retire linearization proof

The increment and READY→RETIRE_REQUESTED update compete on the same AU64 modification order. If retirement wins first, no later increment can match READY because the gate never returns to READY. If enrollment wins first, the retirement CAS preserves its count and the endpoint is an admitted holder, even if ACTIVE has not yet been written. A retirement actor cannot lose an increment by blindly storing a new state word.

This closes the ordinary reference-increment-versus-close race. It does **not** close the crash gap between shared enrollment and its separate record, nor does it count pre-enrollment mapping readers. The external pending-grant registry supplies the missing lifetime obligation; on uncertainty it retains rather than reclaims.

### 11.3 Detecting stale sessions without inferring death from age

| Observation | Required response | Prohibited inference |
|---|---|---|
| Header session/host/authority differs from grant | BAD_IDENTITY; no queue access | “Same filename means same object” |
| Valid CRC but no live authority grant | AUTHORITY_REQUIRED; no enrollment | “CRC proves the session is alive” |
| BUILDING/unpublished candidate | NOT_READY; authority cleanup only | “Safe to initialize its atomics ourselves” |
| RETIRE_REQUESTED/QUIESCING/QUARANTINED/SEALED | Reject new work/enrollment | “Safe to reset all cursors now” |
| Peer heartbeat overdue or PID absent from a stale record | Suspicion; ask authority or stop | “Writer can never resume” |
| Registered process-level death confirmed | Fence evidence for that process and its accounted mappings | “Every orphan token has now been identified” |
| New object under a reused name | Identity mismatch rejects stale grant | “Old mappings were revoked by unlink” |

A live READY gate is not a proof that its producer or authority is currently running. The authority can die immediately after issuing a valid grant; no finite pre-attach check eliminates that failure-detection interval. Such a race preserves old-object lifetime conservatively and may lose service availability; it does not authorize an attacher to initialize a replacement in place. Exact failure classification depends on the external process-lifecycle evidence. A pointer already exported from a mapping remains a potential access until its holder actually ends or is fenced; changing the gate does not revoke it.

### 11.4 Operation entry and retirement semantics

At the start of a public reserve or new borrow, validate local role/handle state and acquire-observe the gate. READY authorizes an attempt, not a promise that the gate stays READY until its LP. A retirement request forbids new grants immediately at its gate LP, but existing participants may hold a previously authorized reservation, a prepared CAS, an executing helper, or a retained view.

Calls already inside an admitted token transfer may complete that old-generation transfer while the authority seeks quiescence. An unpublished write lease rejected before QR insertion is retained or explicitly aborted by its owner; the runtime must not guess nonpublication after the publication path has begun. A retired reader may finish a valid borrow and the corresponding return while the object remains trusted and mapped. Under detected queue corruption or a terminal counter condition, retaining/quarantining a token is preferable to attempting an unproved cleanup insertion.

An application-controlled drain may finish already published records under the existing endpoint's old-generation ownership contract until the authority orders local stop. It does not reopen enrollment or allow new payload reservations. QUIESCING requires the local handle to close entry to new calls, finish all in-flight operations/helpers, and end every payload alias before acknowledging. A gate load alone is not the global stop proof.

### 11.5 Normal detach

Detach first serializes with that endpoint's local entry mechanism. It refuses BUSY while a call, pending helper, write lease, read lease, exported buffer, or other local mapping alias remains live. The caller may explicitly finish/abort permitted outstanding work and retry. No “force detach” silently invalidates raw views.

After the last old-generation access has ended, the participant release-stores QUIESCENT in its one-shot record while it is still enrolled. It then uses strong SC CAS to decrement the gate count, preserving the current lifecycle state. The successful decrement is the **detach LP**. After that LP, it may operate on local bookkeeping and unmap/close its own mapping, but it must make no further shared header, record, queue, descriptor, payload, or wait-word access. In particular it must not write a final DETACHED marker after decrementing.

Each attach owns its mapping lifetime independently in the reference adapter. A later optimization that shares a virtual mapping among local endpoints must retain it until every endpoint and exported view referencing that mapping is finished; one endpoint's detach cannot unmap a neighbor's live view. The authority-side potential-holder record is released only after the participant's local mapping/alias cleanup is acknowledged or its complete process is fenced. A crash before the decrement can leave an overcount; a crash after it but before cleanup acknowledgment leaves an unresolved mapping-holder record. Both cases retain safety by delaying reclamation.

### 11.6 Global sealing and reclamation

A generation may reach SEALED only when no new grants can be issued; every preexisting grant/open/map attempt is accounted for; all enrolled endpoints have completed local no-future-access acknowledgments or are effectively fenced; every helper/native call and exported alias has ended; no unregistered descendants or transferred mappings exist; and the authority has resolved all mapping-holder records. The gate count alone is insufficient.

If the count is inflated by confirmed dead participants, the authority may reconcile it only **after** this global no-future-access proof. It is not allowed to zero the count to obtain the proof. An authority that still holds the object can then mark SEALED, release its last diagnostic/management references, unlink as appropriate, and close/unmap. No participant can use SEALED as an invitation to reinitialize that object.

Unlink removes a name; close removes a descriptor reference; munmap removes a mapping in the calling process. These are different lifetimes. None alone revokes all other processes' mappings. A successor always has distinct backing and a fresh session identity. [R07–R09]

### 11.7 Process identity and inherited mappings

The baseline lifecycle profile is **managed processes**. On Linux, register a process-level pidfd before exposing the mapping, using a launch/registration sequence that prevents PID reuse between process identification and pidfd acquisition. An owned child retained until registration/reaping is one valid setting. Do not substitute a thread-only notification for all threads of a mapping holder. [R11]

On Darwin, the baseline authority owns and accounts for child processes. Wait/reap status with WIFEXITED or WIFSIGNALED is terminal evidence; WIFSTOPPED is explicitly not termination. Arbitrary unrelated-process takeover through a numeric PID is not admitted by this lifecycle profile. [R12]

Both platforms prohibit unregistered fork descendants, cross-process descriptor transfer, or inheritance of an exposed mapping outside the authority's holder set. A parent exiting does not fence a child that inherited the memory. Library callbacks, signal handlers, or asynchronous cancellation must not re-enter a queue endpoint or long-jump out of a transfer and then treat its token as unowned. Shutdown signals can request cooperative stopping, but SIGKILL still creates the known outcome gaps.

### 11.8 Bounded authority manifest and authority failure

The existing application authority maintains one bounded object manifest for a replacement cohort. It contains at most four records, each naming a session, backing identity/name, exact M, M_max, creation outcome, stage, predecessor/successor relation, and outstanding grant/holder set. Each generation has at most K potential endpoint grants, each with process incarnation, role, OS lifecycle evidence, and grant/cleanup outcome. These are process-local management records, not a hidden fifth mutable region or a new native framework.

Reserve a manifest slot before create, and retain it through uncertain create/unlink/mapping outcomes. At most two records are QUARANTINED, at most one serves active work, and at most one is staged/preparing; abandoned staged candidates continue to count. Thus mapped backing-object demand is bounded by 4M_max, excluding separately capped management/kernel overhead. The specified Turn 3 policy is preserved, not replaced by the older chat variant's three-object cap. [P3, §4.4]

There is **no automatic authority handoff** in version 1. If the authority disappears, participants stop accepting new authority grants and retire/retain their known mappings. A new authority must not restart a fresh unaccounted four-object allowance over the same unresolved cohort. It first needs complete fencing/quiescence and reconciliation of all old objects/holders, or the deployment remains stopped. This is the explicitly fail-closed alternative to an as-yet-unproved crash-recoverable manifest.

Ordinary same-authority replacement can continue while its ledger remains complete. The physical-memory and address-space quarantine rule is strict: a successor must not reuse old physical storage, and it must not be mapped over old virtual addresses inside any process that still retains old raw aliases. A different backing file at the same stale pointer address can corrupt the successor just as surely as reusing the original bytes.

---

## 12. Wait adapter contract and lifetime

Only PARKABLE_SPSC is admitted. Each direction has exactly one waiter and one AU32, with AWAKE=0 and ARMED=1. All changes after initialization are acq_rel exchanges, including AWAKE→AWAKE notifications, arm, and disarm. The notifier runs after the predicate-changing release publication. The waiter arms, rechecks the predicate with its required acquire and lifecycle, then invokes compare-and-wait only when still justified. [P2, §8]

The normal lost-wake proof follows the wait-word RMW modification order. If notification precedes arming, the synchronization chain carries the predicate publication into the recheck. If arming precedes notification, the changed expected value prevents a late block or the wake reaches an enrolled waiter. Removing a same-value RMW or introducing another waiter requires a different proof.

The public waiting API accepts a finite U64 `timeout_ns` budget, in nanoseconds on both platforms. Zero requests a nonblocking predicate recheck. The implementation derives one process-local deadline at entry, using Linux CLOCK_MONOTONIC or the Darwin mach-absolute-time domain with checked conversion for nanosecond durations. An unrepresentable deadline returns an argument/range error before waiting. No raw Darwin tick value is interpreted as a nanosecond value. Deadlines and timestamps are not serialized into shared cells.

Each blocking attempt is bounded to at most 10,000,000 ns of requested waiting or the remaining caller budget, whichever is smaller. This is an OS timeout request, not a wall-clock scheduling deadline. Expired/zero remaining budget returns without making a zero-timeout Darwin call. Every return—wake, mismatch, interruption, timeout, or spurious result—disarms through the same RMW discipline and rechecks predicate/lifecycle. Waiter storage remains mapped throughout the wait and every wake caller's access.

| Platform | Required adapter |
|---|---|
| Linux | Shared FUTEX_WAIT/FUTEX_WAKE on the aligned four-byte word, without FUTEX_PRIVATE_FLAG; relative requested duration derived from the local monotonic deadline |
| Admitted Darwin, macOS 14.4+ | Public os_sync_wait_on_address_with_timeout using size=4, OS_SYNC_WAIT_ON_ADDRESS_SHARED, OS_CLOCK_MACH_ABSOLUTE_TIME, and a positive nanosecond duration; matching shared wake flag on os_sync_wake_by_address_any/all |

The Darwin wait API's returned waiter count is not proof that data exists. The Linux errno/return classifications likewise do not replace acquire validation. The public Darwin interface does not provide priority-inversion avoidance. Standard-library atomic waiting is not substituted: the inspected libc++ adapter uses process-private flags in its ordinary path. [R13–R15]

Retirement release/SC-publishes lifecycle before the existing wait notification discipline. The closer is still a recorded holder until its notification accesses finish. A notifier may die after payload publication or after wait-word modification but before the wake; finite timeout/recheck bounds the requested sleep interval but not OS scheduling delay. No MPMC waiter protocol is inferred from these reserved cells.

---

## 13. Counter bounds, identity, and error semantics

### 13.1 No live wrap, including failed/aborted work

Queue cursors must stay at or below J=2^64−N−1. Before computing or attempting a desired increment, prove it does not exceed J. In particular, QR/QF entry publication at ticket t requires t+1≤J **before the entry-installation CAS**, not merely before the later tail assistance. A head claim at h likewise requires h+1≤J. SPSC refuses a new reservation whose eventual P+1 would exceed J. Thus an entry cannot be published at an unconsumable terminal ticket by postponing its ceiling check until after the LP. Cycle-base extraction and one-older comparison must be expressed without an overflowing addition or signed comparison. A saved expected value cannot become current again through in-place reset or represented identity repetition.

Descriptor epochs satisfy 0≤g≤G=2^62−2. The increment check happens before issuing a new lease; aborted reservations still consume epochs. The encoded status then remains within one U64 without carrying into a nonexistent bit. The ordinary epoch field's extra two bits are not an independent source of larger-generation identity.

A threshold check that prevents a new public reserve does not guarantee that every already-held token can still be returned near exhaustion. Each internal head/tail increment has its own guard. If safely finishing a transfer would exceed a bound, retain the token in the retiring generation and require cohort quiescence/replacement; do not roll back a cursor, wrap, or re-enqueue twice to make cleanup appear successful. Proactive retirement is encouraged but does not replace these per-operation guards. [P3, §8]

Session identities, authority epochs, endpoint IDs, and native lease instances are also not reused while stale references can exist. In version 1, endpoint slots are one-shot. A caller's numeric block index or copied old descriptor is not a bearer capability to read mutable data.

### 13.2 Public semantic API surface

The later C ABI exports operations with opaque native connection/lease handles and fixed-width scalar arguments/results. The names below are normative operations, not function prototypes. Native process-local allocations or reference bookkeeping may occur during setup; no such work is inserted into the minimal ring operation without revising its cost profile.

| Operation | Inputs/obligations | Result and ownership |
|---|---|---|
| elite_create | Authority-managed validated configuration and process cohort | Fresh staged/initialized object; no public grant before READY |
| elite_attach | Constructed-object grant and admitted local tuple | Enrolled native handle or explicit partial-enrollment cleanup responsibility |
| elite_write_reserve | Producer handle; no outstanding token | Writable span bounded by B, plus unique native lease instance, or availability result |
| elite_write_commit | Live write lease, length/type/id, aliases ended | Publication outcome reported separately from transport status |
| elite_write_abort | Live unpublished lease, no publication in progress, aliases ended | SPSC private cancellation or one NCQ QF return; otherwise retain/retire |
| elite_read_borrow | Consumer handle; no outstanding token | Read-only valid payload span and metadata after ownership validation |
| elite_read_release | All read views ended | SPSC reclamation or one NCQ QF return; outcome separate from call completion |
| elite_wait_data / elite_wait_space | Correct SPSC endpoint, finite U64 nanosecond budget | Hint to retry predicate or explicit terminal/error result; never ownership |
| elite_retire | Enrolled endpoint or authority, reason | Gate transition preventing new enrollments; not completed quiescence |
| elite_detach | No remaining local call/lease/helper/view | Count decrement LP then local mapping cleanup; BUSY otherwise |
| elite_get_info | Native handle to cached immutable data | Safe local snapshot; not a raw live-mapping dump |

Native failures are fixed-width status values, not a layout-dependent C enum stored in the mapping. The initial status registry is: 0=OK; 1=NO_DATA_OBSERVED; 2=NO_CAPACITY_OBSERVED; 3=RETIRED; 4=BUSY; 5=BAD_ABI; 6=BAD_LAYOUT; 7=BAD_IDENTITY; 8=NOT_READY; 9=COUNTER_LIMIT; 10=INTEGRITY; 11=OS_ERROR; 12=OUTCOME_UNCERTAIN; 13=AUTHORITY_REQUIRED; 14=UNSUPPORTED; 15=INVALID_LEASE; 16=INVALID_ARGUMENT; 17=CREATE_CONFLICT. A process-local supplemental OS error value is separate.

Transfer outcome is independently represented: 0=NONE; 1=WRITE_OWNED; 2=PUBLISHED; 3=READ_OWNED; 4=RETURNED; 5=RETAINED; 6=UNCERTAIN. The later caller-facing declarations must preserve this distinction; a generic negative errno must not invite blind resend after an LP.

### 13.3 Commit and cleanup outcome rules

Before QR publication, a safely rejected commit returns a retained write/transfer responsibility or a completed explicit abort. Once entry installation succeeds, publication is irrevocable in this version. Tail-CAS failure due to another helper is successful publication, not failed send. Notification failure after publication reports PUBLISHED with a notification/OS status; it does not return ownership of the payload.

If a process dies without reporting the LP result, other actors may classify the outcome as UNCERTAIN. They must not scan COMMITTED and reconstruct a second enqueue. The report does not claim exactly-once application effects. An idempotency key in `message_id` helps only when the application supplies a deduplication protocol.

A native lease instance, connection identity, expected generation, role, and one-outstanding-token discipline jointly validate a call. Never access a descriptor just to determine whether an arbitrary stale raw pointer happens to be valid. An invalid call is rejected from owned local state before unowned payload access.

### 13.4 Python and C++ boundary

Wrappers receive opaque handles and bounded span views. C++ wrappers do not instantiate alternate shared atomics. A Python buffer must retain both the native mapping and its slot lease. Keeping only the memory mapping alive leaves the bytes vulnerable to legitimate ring recycling. Releasing a token while an exported alias remains live is forbidden; detaching then returns BUSY or retains the native owner object until the final export ends. [P2, §§6.10–6.11; R19]

A raw-pointer escape that the binding cannot track is an explicitly unsafe native-borrow contract, not a safe revocable buffer. The safe default may copy. Optional CFFI is an external Python dependency; the native core remains independent of that runtime. Exact binding declarations and GIL/batching behavior remain for the later binding gate, not hidden in the shared-memory format.

---

## 14. Attach/detach proof obligations and mandatory negative cases

### 14.1 Initialization visibility

All immutable prefix/registry bytes and initial data/control states precede the builder's release READY store. An attacher's acquire reading that value, or an appropriate later gate RMW chain, orders these initial writes before its authorized reads. The external grant ensures the gate object itself has already been constructed. Neither half substitutes for the other: an acquire cannot make an unconstructed atomic valid, and a known filename cannot supply the initialization happens-before edge.

### 14.2 Enrollment safety

One-shot record CAS excludes duplicate enrollment with the same grant. Gate CAS co-locates state and count so retirement cannot overwrite a concurrent count update. Successful enrollment is either before retirement in the shared modification order or rejected. Endpoint record state is not treated as an atomic pair with the gate; intermediate/crashed states retain uncertainty and the authority's holder record.

### 14.3 Reclamation safety

A voluntary decrement is preceded by all that endpoint's queue/descriptor/payload/wait accesses and its local stop discipline. But count zero does not cover pending grants. Global sealing additionally closes the authority's grant source and obtains/fences every potential holder. Only that full condition makes old-object reclamation safe. A later authority cannot infer it from SEALED bytes in an untrusted old file; it needs the matching managed identity and evidence.

### 14.4 Mandatory declarative rejection histories

| ID | Attempted shortcut or malformed case | Required outcome |
|---|---|---|
| ABI-01 | Magic initialized by native-LE store of displayed big-endian-looking literal | Reject wrong byte sequence |
| ABI-02 | Host pointer, native enum, or different atomic width changes field layout | Build/attach admission fails |
| ABI-03 | Header aligned 128 but entry stride is 8 | Reject strict-isolation profile mismatch |
| ABI-04 | Header CRC covers live gate/cursors and changes while reader copies | Forbidden snapshot method; no CRC acceptance claim |
| ABI-05 | CRC field included without zero canonicalization | Test fixture mismatch rejects format |
| ABI-06 | Attacher finds an in-progress object and polls a possibly unconstructed gate | Reject absent constructed-object grant |
| ABI-07 | Attach increments a separate reference count after reading READY, retirement races | Reject that implementation; gate/state CAS must be composite |
| ABI-08 | JOINING participant dies around gate increment | Retain uncertainty; no guessed decrement or slot reuse |
| ABI-09 | Participant writes DETACHED after the final count decrement | Reject post-LP shared access |
| ABI-10 | Detach while a Python memoryview or native alias survives | BUSY/retention, not unmap/recycle |
| ABI-11 | Header reports READY and an old valid CRC but stale authority identity | BAD_IDENTITY/AUTHORITY_REQUIRED |
| ABI-12 | Coordinator marks RETIRE_REQUESTED and immediately resets arrays | Reject; retirement is not quiescence |
| ABI-13 | CRC/status update followed by MFENCE is used to revoke a live writer | Reject; retained pointer remains a capability |
| ABI-14 | Fresh backing mapped over retained old virtual aliases | Reject successor mapping/admission |
| ABI-15 | CAS head failure refreshes expected but reuses old block index | Reject stale observation set |
| ABI-16 | Publication-CAS failure is retried against competitor's installed entry | Reject potential overwrite of published record |
| ABI-17 | Epoch increment wraps or phase packing silently drops high bits | COUNTER_LIMIT before new lease |
| ABI-18 | Postpublication cleanup zeros descriptor after another owner reuses it | Reject ownership violation |
| ABI-19 | Unregistered inherited mapping remains after parent death | No global fence/seal assertion |
| ABI-20 | Death of authority starts a fresh unaccounted allocation allowance | Stop; reconcile/fence old cohort first |
| ABI-21 | MPMC uses dormant SPSC wait cells to sleep multiple consumers | UNSUPPORTED until independent waiter proof |
| ABI-22 | Compiler helper uses a process-local lock or incorrect ISA feature | Reject target binary |
| ABI-23 | AU64 storage is replaced by two ordinary 32-bit operations | Reject primitive representation |
| ABI-24 | fstat/CRC validation is claimed to protect against later malicious truncation | Reject threat-model claim; this ABI assumes controlled writers |

These are paper histories and admission requirements. None has been executed as a concurrency test in this turn.

---

## 15. What becomes fixed and what remains an implementation gate

### 15.1 Fixed by this proposal

The format now fixes scalar width/endianness; common-prefix fields; the three requested aggregate layouts; entry/participant layouts; explicit reserved ranges; checksum coverage/algorithms; canonical region formulas; phase/epoch representation; source memory orders; semantic operation outcomes; managed bootstrap and one-shot enrollment; count/retire LPs; no-wrap ceilings; raw-pointer lifetime; and the bounds/authority-failure policy for generation replacement.

SPSC has a physically present but dormant status word and no queue reservation CAS. NCQ retains SC queue metadata and strong CAS; it does not gain a claim-first ticket or in-place revocation path. Changes to these semantics require a new profile or a new proof, not an undocumented compile flag.

### 15.2 Not fixed by a paper specification

No current compiler version, Apple chip cache geometry, OS scheduler behavior, process-shared ABI tuple, CRC throughput, RMW latency, or end-to-end timing is certified. Runtime capability and geometry reports, build manifests, symbol/helper inspection, actual struct assertions, object-lifetime refinement, hostile-layout parser tests, and cross-process stress traces remain required after implementation is authorized.

The proposed native lifecycle adds a gate observation at public entry. Optional checksum and parking work have explicit profiles. Their costs must appear in the later benchmark, rather than timing a smaller internal queue while calling it the full public ABI. The governing Turn 3's 1,000-ns p99.9 policy remains a measurement gate, not a hardware theorem.

### 15.3 Version and change management

Version `0x00010000` denotes major 1, minor 0. This version accepts exactly its defined constants, profiles, and zero reserved fields. Merely adding meaning to a reserved byte under the same version is forbidden. The first incompatible layout change requires a new major version or explicitly distinct negotiated profile/version whose parser is separately specified. A binary may support multiple versions but cannot mutate one live generation into another.

The campaign now labels this turn 4 of 8. No future turn number alone authorizes implementation; the commissioning gate must explicitly do so. The next planning work should preregister the target/binary and benchmark acceptance evidence against these exact offsets and operations.

---

## 16. Normative summary for the later implementer

**Identity before access:** a constructed-object grant establishes which already-initialized atomic object may be observed. Acquire READY before ordinary immutable reads. Validate the exact format before queue access. A CRC is not a grant.

**Ownership before bytes:** QF or SPSC capacity authorizes writing; QR or SPSC publication authorizes reading; final alias closure precedes return. State words describe phase, not queue membership. The native lease is consumed at an LP, not at the caller's return receipt.

**One source-level atomic contract:** AU32/AU64 only; no widened hidden primitive; no C++ overlay; strong SC NCQ metadata; correct failure-order and expected-value behavior; RA SPSC cursors and wait exchange discipline. A legal compiler lowering may vary, but it must satisfy this entire contract.

**Retirement before quiescence, quiescence before reclamation:** close enrollment atomically, then account for calls, helpers, leases, views, pending grants, process inheritance, and actual mapping holders. Do not replace evidence with a count, epoch, checksum, timeout, DMB, or MFENCE.

**Bounded resources, honest stop:** at most four backing objects with at most two quarantines; reserve management records before allocation; no unproved authority takeover. Missing evidence stops admission rather than making old pointers safe by declaration.

---

## 17. Artifact integrity and self-hash convention

There are three independent concepts: the runtime 512-byte header CRC32; the optional runtime payload CRC64; and this Markdown document's SHA-256 integrity receipts. They must not be confused.

The Markdown encoding is UTF-8 without BOM, with LF line endings and one final LF. Define normalization C(D) as replacing only the 64 lowercase hexadecimal characters inside the single header line whose label is `Canonical-SHA256` with 64 ASCII `0` characters. Preserve every other byte, including the label, backticks, whitespace, and line endings. Reject zero or multiple such header fields as an invalid canonicalization input.

The embedded digest is SHA256(C(D)). The companion filename is the complete Markdown filename plus `.sha256`; it contains the lowercase hexadecimal SHA256(D), two spaces, the exact Markdown basename, and LF. That companion authenticates literal finalized bytes. These two hashes will normally differ. A literal whole-file digest cannot be inserted into that same file and still be called the original file's digest without solving a self-reference problem; no such claim is made.

Any artifact ZIP is a transport envelope, not an executable project. Its checksum is separate. Delivery verification distinguishes successful upload/metadata readback from a full raw-download byte comparison. Only checks actually performed are recorded in the external delivery receipt; this immutable specification does not predict a successful upload before it occurs.

---

## 18. Primary sources and predecessor identities

**Access date: 2026-09-23.** Live documentation/source links are evidence of the named mechanism, not pinned deployed compiler/SDK binaries. No external source implementation is copied into this document. The proposed byte schema, lifecycle composition, and acceptance choices are original project specifications built on the cited contracts.

**[P2] Turn 02 — Cache Topologies, Barrier Proofs, and Prior Art.** 91,463 bytes; SHA-256 `9bb8b971b65f7da80bd13053e8027aaa6ede5ae9ea19da8262e24282e16a6977`. Drive ID `1RLms0KbJgf4P02G058S8VVfrpfP6TWyd`. Full text supplied in this conversation. Supplies the SPSC and NCQ-SC64 models, RA/SC choices, ownership LPs, no-wrap rule, conditional isolation, and single-waiter proof.

**[P3] Turn 03 — Concurrency Kill Gate and Failure Modes.** 81,776 bytes; SHA-256 `7ad5fbd43134e7d3419c5a1f76d030e25d051510dbc6f20ce173b3d0eccff605`. Drive ID `1w7wKEWAkmNMOjrtQ4tkEsgC_6onETtgW`. Raw download hash verified this turn. Governs the 1,000-ns proposed service cliff, unknown-owner and raw-pointer counterexamples, retirement/quiescence distinction, four-object/two-quarantine policy, finite counter bounds, and CAS failure hazards. No measured affinity or latency result appears in it.

**[R01] WG14, ISO C11 committee draft N1570.**
https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf
Sections 6.7.5 and 7.17; relevant pages visually inspected, including printed pp.127, 279, and 282. Establishes alignment-specifier constraints, atomic order/initialization semantics, and the distinction between lock-free/address-free behavior and representation assumptions. It does not certify our interprocess ABI.

**[R02] Cambridge memory-model researchers, C/C++11 processor mappings.**
https://www.cl.cam.ac.uk/~pes20/cpp/cpp0xmappings.html
Primary mapping reference for AArch64/x86 loads, stores, CAS, and fences. The page presents mapping choices for discussion, not exact output from every compiler. Used as baseline forms, never represented as disassembly of this project.

**[R03] Arm, Enabling RCpc in GCC and LLVM.**
https://developer.arm.com/community/arm-community-blogs/b/tools-software-ides-blog/posts/enabling-rcpc-in-gcc-and-llvm
Primary explanation of eligible LDAPR lowering and the stronger LDAR behavior. Supports distinguishing source acquire, instruction RCpc/RCsc, and target feature selection.

**[R04] GCC, Atomic built-ins documentation.**
https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html
Compiler contract for order parameters, compare/exchange behavior, lock-free queries, and possible runtime fallback. No claim of exact code generation without a target build.

**[R05] LLVM compiler-rt, AArch64 outlined atomic helpers.**
https://raw.githubusercontent.com/llvm/llvm-project/main/compiler-rt/lib/builtins/aarch64/lse.S
Official source inspected, including MODEL-specific acquire/release forms and the `_sync` DMB path. Live main-branch source; the later build must pin the actual helper revision. Not adopted or copied as implementation.

**[R06] Arm, Memory access ordering, part 3.**
https://developer.arm.com/community/arm-community-blogs/b/architectures-and-processors-blog/posts/memory-access-ordering-part-3---memory-access-ordering-in-the-arm-architecture
Primary shareability/barrier-scope explanation. Supports ISH versus SY and separating memory ordering from cache/persistence or capability claims.

**[R07] Linux man-pages, shm_open(3).**
https://man7.org/linux/man-pages/man3/shm_open.3.html
Supports exclusive creation, initial sizing, name conventions, truncation consequences, and distinct descriptor/name/mapping lifetimes. Linux's namespace implementation is not assigned to Darwin.

**[R08] Linux man-pages, mmap(2).**
https://man7.org/linux/man-pages/man2/mmap.2.html
Supports MAP_SHARED, page-offset/mapping lifetime rules, inheritance, and the hazards of fixed replacement mappings. Neither mmap nor a successful fstat supplies our language ownership proof.

**[R09] Apple XNU, shm_open manual source.**
https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/bsd/man/man2/shm_open.2
Official source describing Darwin's POSIX shared-memory interface. Used for API grounding, not a claim that it uses a Linux filesystem path.

**[R10] Apple XNU, posix_shm.h.**
https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/bsd/sys/posix_shm.h
Inspected PSHMNAMLEN=31 and name storage. The proposed 30-character name avoids relying on longer Linux names. Live source, not a pinned deployed kernel.

**[R11] Linux man-pages, pidfd_open(2).**
https://man7.org/linux/man-pages/man2/pidfd_open.2.html
Supports process-bound lifecycle handles and their usage conditions. A process handle is fence evidence only when registration and all mapping holders are correctly accounted for; it does not recover token ownership gaps.

**[R12] Apple, waitpid(2) manual.**
https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/waitpid.2.html
Archived Apple system-call documentation distinguishing terminal WIFEXITED/WIFSIGNALED from resumable WIFSTOPPED. Used for the managed-child lifecycle boundary, not arbitrary-PID takeover or a current scheduling guarantee.

**[R13] Linux man-pages, FUTEX_WAIT.**
https://man7.org/linux/man-pages/man2/FUTEX_WAIT.2const.html
Supports expected-value checking/block enrollment, timeout, mismatch, and interruption semantics. The user-space wait-word proof remains [P2]'s protocol, not a theorem automatically supplied by a syscall.

**[R14] Apple libplatform, public address-wait interface.**
https://raw.githubusercontent.com/apple-oss-distributions/libplatform/main/include/os/os_sync_wait_on_address.h
Inspected availability, 4/8-byte alignment, shared flags, timeout/deadline variants, and no priority-inversion avoidance. Exact deployment SDK/OS must still be admitted.

**[R15] LLVM libc++, atomic waiting implementation.**
https://raw.githubusercontent.com/llvm/llvm-project/main/libcxx/src/atomic.cpp
Inspected process-private ordinary wait flags and use of OS_CLOCK_MACH_ABSOLUTE_TIME with the nanosecond-timeout API. It is evidence for adapter details and a warning against blindly reusing standard-library wait for IPC, not a new dependency.

**[R16] RFC 1952, GZIP format and CRC appendix.**
https://www.rfc-editor.org/rfc/rfc1952.txt
Primary specification/author appendix for the reflected CRC32 polynomial and pre/post conditioning. The ELITEIPC header input range and zeroed-CRC-field convention are this document's design, not GZIP's header format.

**[R17] Ecma International, ECMA-182, first edition December 1992, Annex B.**
https://ecma-international.org/wp-content/uploads/ECMA-182_1st_edition_december_1992.pdf
Printed p.51 / PDF page 63 visually inspected. Defines the 64-bit generator polynomial and remainder construction. The project's variable-length payload range and stored byte order are specified independently above.

**[R18] CRC algorithm definition and check-value reference.**
https://docs.rs/crc-catalog/latest/crc_catalog/algorithm/constant.CRC_64_ECMA_182.html
Library-author documentation states all parameters and the `123456789` check value for CRC-64/ECMA-182. Used only for that precise algorithm identity, not a performance or platform claim; no library dependency is introduced.

**[R19] CFFI, Using ffi/lib objects.**
https://cffi.readthedocs.io/en/latest/using.html
Binding-author documentation on buffers and lifetime. It does not independently keep a ring token reserved; the native lease/alias discipline supplies that guarantee.

### Evidence closure

The predecessor's exact raw identity was verified. Source sections and the relevant C11/ECMA PDF pages were inspected. The artifact's field/extent arithmetic, reserved-byte coverage, magic interpretation, and integrity receipts are checked as document-production validation. No C structure was compiled, no atomics executed across processes, no architecture timing measured, and no revocation or crash-recovery implementation is claimed.
