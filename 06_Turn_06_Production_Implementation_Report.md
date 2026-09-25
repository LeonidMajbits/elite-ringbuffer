# Turn 06 — Production C11 Implementation Report
## ELITEIPC / LE128-V1 / SPSC and NCQ-SC64

**Project:** Leonid Majbits / Gemini Operator Lab, paired with Leon  
**Date:** 2026-09-23  
**Stage:** 6 of 8 — first explicitly authorized implementation gate  
**Disposition:** Compiling native implementation delivered, with scoped Linux execution evidence. Production release qualification and Apple-native performance certification remain open.  
**Canonical-SHA256:** `131f96d8d6a735fcdaf866df515cfbbb1c27a7fa84eba8b196d8958a27dca2d4`  
**Hash convention:** Replace only the preceding field's 64 hexadecimal characters with 64 ASCII zeros before computing the embedded digest. The adjacent `.md.sha256` authenticates literal finalized bytes.  
**Frozen ABI [P4]:** 93,610 bytes; SHA-256 `6aaa3d6706c29096cfab56ea1fe569afa561c4cba7caea26ebd3c09cc9812386`.  
**Frozen verification plan [P5]:** 96,883 bytes; SHA-256 `bc7ae2702cecb8d46820cf5365046fe40dda36c59dde9c80fda98ff7833ecdf8`.  
**Destination:** Deployment Research / Elite_Systems_LockFree_RingBuffer.  
**Evidence convention:** [P4]/[P5] are preserved unchanged in `docs/reference/`. References beginning `evidence/` identify executed observations. Source-derived requirements, implementation decisions, and measured observations are separated below. This report does not predict a successful Drive transfer; a separate delivery record follows actual readback.

---

## 1. Implementation verdict

The package contains a complete C11 library, static/shared build targets, public
header, exact shared-memory layouts, two zero-copy data paths, managed POSIX
backing, bounded SPSC waiting, native integration examples and executable tests.
All six native translation units build with both GCC 14.2.0 and Clang 17.0.0 on
the available Linux/x86-64 host under `-O3 -Wall -Wextra -Werror -pedantic` and the
additional warning policy in the Makefile. There are no TODO bodies, placeholder
queue functions, hidden socket payload paths, or third-party runtime libraries.

This implements the reference **for verification**, which is what [P5, §0]
approved. It does not convert five planning documents into a measured production
certificate. Actual Apple SDK compilation/linking/execution, the full Turn 5
qualification matrix, formal checker coverage and the requested 15/50-ns latency
results remain outstanding. The implementation makes those gates runnable and
falsifiable; it does not invent passing results for them.

Two final-binary native integration trials did each complete 100,000,000 checked
64-byte messages: SPSC 1P/1C and NCQ 16P/16C. Their exact in-memory membership
oracles found no duplicate, missing or bad-payload observations, and final token
reconciliation passed. The NCQ result was an isolated retry after an earlier
combined invocation exceeded its external 200-second execution window. The
incomplete attempt is retained and unexplained, not relabeled a pass.

No per-message latency was measured. Printed controller elapsed times are not
converted into p50/p99/p99.9 or advertised as inverse-throughput latency.

## 2. Delivered source and build surface

| Artifact | Implementation responsibility |
|---|---|
| `include/elite_ringbuffer.h` | Public C11 API; complete frozen structs; 103 compile-time assertions; status/outcome registry; inline acquire/release helpers |
| `src/elite_spsc.c` | Private producer reservation; covering peer acquires; release publication and reclamation; dormant descriptor status |
| `src/elite_mpmc_ncq.c` | Strong-SC QF/QR queues; install-before-tail publication; helping; complete stale-CAS reclassification; owned descriptor phases |
| `src/elite_shm.c` | POSIX creation, canonical name, registration, one-shot constructed grants, atomic enrollment, detach, retirement, child fencing and bounded management |
| `src/elite_core.c` | Native handle/lease validation, dispatch, tracked views, metadata/CRC checks, failure/retained-outcome handling |
| `src/elite_format.c` | Overflow-safe geometry, stable-prefix validator, CRC32 and CRC64, local atomic/platform admission |
| `src/elite_wait.c` | Shared Linux futex and public Darwin 14.4+ address-wait adapters; finite single-waiter SPSC protocol |
| `src/elite_internal.h` | Private per-endpoint state; no additional shared-format region |
| `Makefile` | Static/shared native libraries, GCC/Clang builds, separate hook/sanitizer targets, inspection and executable tests |

The public header plus `src/` comprise 1,814 source lines at this delivery.
Line count is inventory, not a quality or correctness metric. All source/build/
verification files have SHA-256 entries in `SOURCE_MANIFEST.sha256`; the complete
archive contents have a separate manifest. Build artifacts themselves are not
shipped; executed binary identities are recorded under `evidence/`.

The polling native core is C11 plus POSIX. Linux pidfd/futex and Darwin public
address-wait interfaces are isolated OS extensions, not strictly POSIX-standard
functions. This reconciles the dependency language with the frozen wait and
process-lifecycle contracts: no Boost, dispatcher framework, external daemon,
libatomic lock fallback, Python runtime or sanitizer runtime is required in the
normal library. The inspected final Linux shared object depends on libc and the
normal ELF loader only; per-object symbol records contain no mutex or libatomic
fallback reference. Setup allocation can use libc; no allocation is performed
per message.

## 3. Frozen ABI conformance

The header realizes the exact LE128-V1 byte tables, rather than substituting a
new compact in-memory structure. Both ring headers occupy 2,048 bytes, aligned
to 128. Their immutable prefix occupies 512 bytes. Each slot descriptor and
queue entry occupies a complete 128-byte cell; each participant record occupies
256 bytes. All fields, explicit padding ranges, aggregate extents and scalar/
atomic representations are checked by 103 preprocessed `_Static_assert`
statements. Both final native compilers and the queue-only cross compilation
accepted those assertions. This establishes the compiled layout for those
compilation profiles, not every hypothetical C11 implementation. [P4, §§1–4]

The exact ASCII magic is `45 4C 49 54 45 49 50 43`. Numeric ABI version remains
`0x00010000`; canonical arithmetic uses the 16,384-byte format quantum on both
target families. The allocator checks the actual base-page relationship and
uses ordinary MAP_SHARED pages, not a huge-page request. The prefix validator
rejects noncanonical strides/offsets even when a malformed range would fit.

Header CRC32 is the frozen ISO-HDLC variant over exactly 512 immutable bytes,
with its own four bytes treated as zero during calculation. The implementation
never temporarily edits the live CRC field to verify it. Optional CRC64 is
ECMA-182, exactly over the valid payload prefix. `123456789` vectors passed for
both algorithms. No whole live atomic header is memcpy-snapshotted for CRC.
Mutable payload bytes are inspected only by their current owner. [P4, §§6–7]

The SPSC status word is present but dormant. NCQ packs phase and epoch in one
AU64 and retains the ordinary 64-bit epoch mirror. It never reads that mirror
as an unowned speculative lease check. Ticket ceiling J and epoch ceiling G
remain those in [P4]; the implementation checks a publication's eventual t+1
before the entry-installation CAS, not merely after publication during tail help.

Native leases and cleanup receipts are newly concretized *process-local API*
representations. They do not consume reserved shared bytes or change any frozen
status number, memory order, profile identifier or LP. The public header is
C11-only; C++/Python clients must use a future opaque binding rather than overlay
foreign atomics on the shared structures.

## 4. Data-path ownership and failure behavior

### 4.1 SPSC

Reserve first validates local state and acquires the lifecycle gate. It uses
its local publication cursor and an acquire-obtained reclamation snapshot to
authorize writing. It reserves no shared ticket and performs no CAS/FAA on the
reservation path. The producer increments the owned slot epoch; abort consumes
that epoch without publishing the cursor.

Commit finishes length/type/ID/checksum and payload work, then release-stores P.
The consumer acquires a covering P before reading, retains C throughout its
borrow, and release-stores C only after all reads end. These implement both
ownership directions in [P4, §8.1]. No descriptor/payload access follows the
publication or reclamation LP. Optional notification uses separate cells and
returns immediately without an atomic wait-word operation in POLL_ONLY.

### 4.2 Publish-first NCQ-SC64

QF supplies one unique payload token. A producer owns and completes that block
before attempting QR entry installation. Head, tail and entry loads/CAS use
`memory_order_seq_cst`, with strong CAS and SC failure order. An entry exactly
one cycle older is eligible for replacement; a current-cycle entry is already
published and its tail can be helped. Other mismatches cause a fresh observation.

A winning entry CAS is publication. A winning head CAS is removal. Failure
invalidates the saved expected, desired, entry and index; the implementation does
not turn compare/exchange's refreshed expected argument into permission to reuse
an old payload index or overwrite a competitor. Tail assistance after a winning
publication uses only saved locals and queue metadata. It never clears the
transferred block's owner, phase, checksum or payload. [P4, §8.2]

Descriptor phase changes are owner release stores, not a second contested
allocation algorithm. A phase mismatch retires the generation; it does not
indefinitely spin on another producer's RESERVED payload. Terminal counter or
integrity failures retain owned tokens rather than attempting an unproved
rollback or duplicate insertion.

### 4.3 Native identity and aliases

Each endpoint is single-owner/nonreentrant, with one token maximum. Native lease
identity binds the session, authority, endpoint, serial, epoch, block and role.
A stale/duplicate operation is rejected against local owned state before any
unowned descriptor access. C requires a valid live connection object; arbitrary
dangling pointer values cannot be validated safely by dereferencing a magic
number. Caller serialization of each endpoint and its destruction is an explicit
API precondition, not a lock hidden inside the SPSC fast path.

Tracked views block commit/abort/release/detach until ended. Raw pointers remain
an unsafe borrow obligation: the caller must stop using every raw alias at the
transfer boundary. The library does not claim to enumerate arbitrary C aliases.
`elite_abandon_retained` is an explicit retired-generation disposal operation:
it ends local lease responsibility after alias closure but never returns that
token to QF. Old backing remains accounted until safe whole-object reclamation.

## 5. Construction, enrollment and managed lifecycle

`elite_create` records a bounded staging object before exclusive `shm_open`,
sets mode 0600 and exact length, maps it, initializes every atomic separately,
then release-publishes READY. It never truncates an existing name. Fresh names
are generated from session identity using the frozen 30-character base32 form.
The authority supplies nonreused identity domains and a trusted grant channel;
CRC, timestamps and raw grant bytes are not authentication. [P4, §§10–11; R1]

At attach, fixed-offset gate access is justified by the constructed-object grant,
not by an untrusted discovered filename. Canonical header and registry validation
precedes queue use. NEW→JOINING is one-shot. The READY/count enrollment update
competes with retirement on one SC atomic word. A post-enrollment retirement race
returns a handle with an explicit detach obligation; it does not discard a
reference because the attach result was not usable.

Cleanup receipts include whether the attaching attempt actually claimed the
one-shot grant. A failed duplicate attempt therefore cannot resolve the original
holder's mapping lifetime. Failures before a successful grant claim can remain
conservatively unresolved and require terminal child evidence or application
reconciliation. An ambiguous close error leaves a fail-closed cleanup tombstone;
it is not retried blindly against a potentially reused FD number.

Normal detach ends all local aliases/calls, writes QUIESCENT, then decrements the
combined gate. No shared access follows that decrement, including a final
DETACHED marker. The authority requires cleanup acknowledgments or accounted
terminal child evidence for every issued grant before sealing. The enrollment
count alone is insufficient. Linux registers owned children with pidfds before
exposure; Darwin code uses owned-child wait status. A stopped child is not
termination, and inherited unregistered mappings are forbidden. [P4, §11; R2]

The application-owned authority permits at most four objects, two quarantines,
one active object and one staged candidate. Failed candidates remain charged
when cleanup is uncertain. It does not supply automatic takeover after its own
death. Successor allocation uses distinct backing and cannot map over still-live
old mappings through this API. No timeout can reset an epoch, steal a payload or
make unknown delivery certain. The helper functions implement safe managed
building blocks; a complete application liveness channel and all-reference
acknowledgment discipline remain integration responsibilities.

## 6. Instruction lowering: observed versus not executed

Two actual queue source units, not a miniature substitute algorithm, were
cross-compiled with Clang 17.0.0. The assembly-only adapter uses compiler resource
headers plus minimal declaration shims, and contains the real layout assertions.
It does not provide a Darwin SDK or establish compilation of the OS adapter.
Full assembly and the script are supplied. [R3]

| Compilation profile | Observed lowering |
|---|---|
| Linux x86-64 GCC final object | Release/acquire loads/stores use ordinary ordered compiler-generated accesses; NCQ has locked CMPXCHG |
| AArch64 ARMv8 source-to-assembly | SPSC LDAR/STLR; NCQ LDAXR/STLXR strong exclusive retry paths |
| AArch64 ARMv8.1-A+LSE source-to-assembly | Strong-SC NCQ CASAL |
| Apple M4 default source-to-assembly | Eligible SPSC acquire uses LDAPR; release uses STLR |
| Apple M4 RCpc-disabled source-to-assembly | Requested SPSC LDAR/STLR; NCQ LDAR/CASAL in its SC mapping |

The M4 observation is important: C11 `memory_order_acquire` alone does not mandate
one mnemonic across targets. To satisfy the requested strict form, the Darwin
Makefile defaults to `ELITE_STRICT_LDAR=1`, passing Clang's explicit RCpc feature
disable. `ELITE_STRICT_LDAR=0` keeps the frozen legal alternate mapping but must
be separately labeled. Unsupported flags fail rather than disappearing from the
binary identity. No source acquire is strengthened to SC merely to force LDAR.

**Not performed:** native Apple SDK compile/link, actual M4 execution, Apple
atomic-helper/runtime admission, or empirical Apple memory timing. The assembly
adapter's files are not presented as a portable Apple library binary. No DMB or
MFENCE is used as raw-pointer revocation; no such operation exists. [P4, §9]

## 7. Executed verification ledger

Host: Debian GNU/Linux 13.3 container, kernel 6.18.44, x86-64, reported AMD EPYC
9V74 CPU. Five logical CPUs were allowed (0–4), with cgroup CPU quota
400000/100000 microseconds, equivalent to four CPU-seconds per second of quota.
Thus 32 endpoint workers were heavily oversubscribed. This is not a 16-core
Apple machine. Full compiler versions, hashes, page size and linkage are in
`evidence/machine-and-toolchains.json`.

| Evidence | Actual observation | Scope |
|---|---|---|
| GCC/Clang final strict builds | All native translation units and library/test links succeeded | Linux x86-64, named flags |
| Public layout assertions | 103 compiled assertions accepted | Named native and queue-only cross compilers |
| Core groups | 68 groups passed under both compilers | Both queues; NONE/CRC64; N2/32/1024; leases, views, CRC, retirement, quotas |
| Header one-byte mutation set | 512/512 rejected | Stable immutable inputs, not raced live atomics |
| Stable-prefix fuzz corpus | 1M deterministic inputs: 89,908 valid mutations accepted, 910,092 rejected | Deep checks include recomputed CRC; GCC native and Clang ASan/UBSan |
| Controlled adversarial groups | 8 passed under GCC/Clang | Includes one-mapping 2P2C/100k, stalled writers, stale head, late returner, integrity and SPSC wait |
| Near-limit fixtures | 4 passed | SPSC/NCQ ticket and epoch ceilings; quiescent verification-only states |
| Native chaos | 3 passed under both compilers | Actual spawned child SIGKILL/SIGSTOP/SIGCONT histories |
| Parkable SPSC IPC | 100,000 checked messages passed | Linux shared futex adapter |
| Final SPSC integration | 100,000,000 checked messages passed | One spawned producer and consumer; no observed duplicates/loss/bad bytes; all tokens returned |
| Final NCQ reduced retry | 10,000,000 checked messages passed | Sixteen spawned producers and sixteen consumers |
| Final NCQ large retry | 100,000,000 checked messages passed | Same 16P/16C native IPC, exact membership and final token reconciliation |
| Clang native cross-check | 16P/16C 1M and SPSC100k passed | Independently compiled Linux executable |
| Clang ASan+UBSan | Core68, adversarial8, fuzz1M passed; exit0 | Instrumented scope only |
| GCC TSan | Adversarial8, one virtual mapping for thread lane; exit0 | No core suppressions; not cross-process race certification |
| TSan detector control | Deliberate standalone race reported; expected exit66 observed | Confirms relevant detector activation, not all negative mutants |

The native membership oracle checks each eight-word payload, local duplicate
bits, pairwise-disjoint consumer bitmap union, every expected ID, producer quotas,
consumer releases and the final free/ready state. A counts-only/XOR certificate
is not used. Raw bitmaps are checked in memory and not exported by this initial
integration driver; saved logs summarize those executions. [P5, §§3, 14]

**Incomplete attempt:** a combined final-GCC invocation completed the SPSC 100M
run, then exceeded an outer 200-second tool limit before NCQ reported completion.
No cause was established. The record is INCOMPLETE, without an invented loss
count or claim that it was merely scheduler noise. An isolated 10M run and an
isolated 100M NCQ retry subsequently completed. Those do not repair or erase the
incomplete record. The terminated cohort was confirmed absent before removing
only its specifically observed orphaned test object.

The reported controller intervals were 14,237,911,235 ns for the final SPSC100M
run and 24,525,737,497 ns for the isolated NCQ100M retry. These include control and
bitmap-collection work. They are execution provenance, **not per-message latency
or the complete frozen V5 goodput definition**. No inference of <15 ns is drawn.

### 7.1 Adversarial behaviors actually exercised

A producer paused after owning a block allowed 10,000 healthy transfers. A
producer paused after actual QR entry publication allowed consumption and later
queue progress without its tail update. A stale reader observation was retained
across 101 competing claims and then discarded correctly. A delayed releaser
resumed after its returned block had circulated without modifying the new owner.
Integrity mutations happened under the producer's ownership after checksum
calculation, not through a racing observer. The returned corrupt token was
retained and the generation retired.

The native chaos tests killed a spawned child mid-write, killed it after QR LP
before tail assistance, and stopped/resumed an old writer after a successor was
created. Unpublished partial bytes were not delivered; healthy independent work
continued under the declared scenario; a held successor message remained
unchanged when the old writer resumed. No token was recovered by phase scan or
elapsed-time theft. These are three concrete scenarios, not the entire [P5]
100-per-cut/1000-random/1000-recovery-trial campaign.

## 8. Sanitizer and build boundaries

The final GCC and Clang logs preserve the full language/optimization/target
arguments. Normal library objects use PIC; Linux executables use explicit PIE.
Sanitizer binaries use O1, debug information, frame pointers, no LTO and the
corresponding compiler runtime. ASan and TSan are separate builds. [R4]

TSan ran with no suppressions, no project annotations pretending a release/acquire
edge exists, and `force_seq_cst_atomics=0`. The test adapter explicitly maps all
thread-visible references to one virtual mapping for that lane and restores each
public attachment's mapping ownership after joins. This is an instrumented
verification arrangement, not a production alias-sharing optimization. Native
spawn/exec tests exercise the real independent attachment paths separately.

ASan cannot infer that still-allocated shared bytes have changed logical lease
owner. The explicit ownership checks and adversarial histories remain necessary.
A clean sanitizer run cannot discharge the whole interprocess protocol, and none
is used as a performance sample. [P5, §12]

## 9. Remaining blockers and honest release status

The following are **not supplied as completed qualification**:

- Native Apple SDK compilation/linking/execution; actual P/E/topology and helper
  admission; native Darwin waits and process adapter execution.
- The full five-repeat 100M protocol with prescribed warmup, start schedule,
  drained-cohort endpoints and exported evidence. The included integration
  driver does not silently stand in for that harness.
- Direct one-way latency, qualified clock uncertainty, strict <15-ns p50,
  <50-ns p99, retained p99.9 service gate, profiler/PMC measurements or scaling
  comparison against a semantically equivalent lane fabric.
- Exhaustive finite model/weak-memory checking and the complete negative-mutant
  register, full OS failure-injection permutations, all chaos repetitions,
  quantitative managed-restoration distribution or authority-death takeover.
- CFFI/C++ wrappers, Python buffer-lifetime certification, persistent power-loss
  behavior, MPMC sleeping, or exactly-once application effects.

Current concrete limitations are documented rather than hidden: connections and
the authority require caller serialization; arbitrary raw aliases cannot be
tracked; failed pre-claim grants can remain unresolved; ambiguous close errors
retain cleanup tombstones; a crashed authority stops managed recovery; a public
QUIESCING setter is not supplied in this core API. Cooperative local stop,
accounted cleanup, quarantine and sealing still follow the specified lifecycle
predicates. None permits in-place reinitialization or an unaccounted new cohort.

The <15-ns goal may or may not be met by the full native ABI. This report supplies
no evidence deciding it. It does supply working, tested source that retains the
lifecycle and lease costs rather than deleting checks to manufacture a timing.

## 10. Reproduction, package integrity and next gate

Start with `README.md`, then `docs/API.md`. `make check check-hooks limits chaos
fuzz` runs the small native suite. Separate `test_ipc` invocations run 100M
integration trials. Use fresh build directories for compiler/flag changes.
`tools/cross_lowering.sh` regenerates the assembly-only evidence. No compiled
executables or proprietary SDK/font/runtime files are distributed.

`SOURCE_MANIFEST.sha256` covers source, headers, Makefile, test and tool inputs.
`MANIFEST.sha256` covers every packaged file other than itself. The Markdown
literal sidecar and ZIP sidecar are separate. `evidence/TESTED_CORE_SOURCE_IDENTITIES.sha256`
binds the observed tests to the delivered core; `evidence/BUILD_BINARY_IDENTITIES.sha256`
identifies the local compiled artifacts without shipping them. The frozen
predecessor hashes were checked before copying them into the reference folder.

**Turn 6 exit:** compiling core implementation and scoped executed evidence
accepted for the next validation campaign. Do not promote the package to a
latency-certified, all-target production release until its remaining gates run.
A single valid future ownership witness still rejects that implementation,
regardless of these successful finite trials. [P5, §15]

## 11. Sources and authority

**[P4]** `docs/reference/04_Turn_04_C11_ABI_and_Assembly_Specification.md`, exact
hash recorded in this header. Governs all shared bytes, atomic orders, ownership,
bootstrap, counter bounds and result semantics. No layout change is inferred
from a compiler convenience.

**[P5]** `docs/reference/05_Turn_05_Verification_Harness_and_Test_Plan.md`, exact
hash recorded in this header. Governs evidence scopes and unfulfilled release
qualification. The completed subset is listed above instead of being generalized
to all planned tests.

**[R1]** Linux man-pages, shm_open(3):
https://man7.org/linux/man-pages/man3/shm_open.3.html
Official interface grounding for exclusive creation and separate object/name/
mapping lifetimes; implementation tests are local evidence, not claims of the
manual itself.

**[R2]** Linux man-pages, pidfd_open(2):
https://man7.org/linux/man-pages/man2/pidfd_open.2.html
Identity-bound lifecycle handle and registration conditions. Owned-child and
no-unregistered-descendant constraints remain application obligations.

**[R3]** Arm, Enabling RCpc in GCC and LLVM:
https://developer.arm.com/community/arm-community-blogs/b/tools-software-ides-blog/posts/enabling-rcpc-in-gcc-and-llvm
Supports eligible LDAPR selection. Actual compiler outputs are independently
preserved in `evidence/assembly/`; no exact Apple runtime is inferred from the
article.

**[R4]** LLVM Clang, ThreadSanitizer and AddressSanitizer:
https://clang.llvm.org/docs/ThreadSanitizer.html
https://clang.llvm.org/docs/AddressSanitizer.html
Official instrumentation/ignorelist/runtime-scope guidance. Local final logs,
not documentation examples, support the reported executions.

**[R5]** Apple libplatform public address-wait header:
https://raw.githubusercontent.com/apple-oss-distributions/libplatform/main/include/os/os_sync_wait_on_address.h
Consulted for exact shared wait/wake/timeout signatures and flags. The Darwin
adapter follows it but remains unexecuted on native Apple hardware here.

**Algorithmic attribution:** Ruslan Nikolaev, DISC 2019,
DOI 10.4230/LIPIcs.DISC.2019.28, Figure 5 (NCQ), as analyzed by the supplied
Turn 2 reference. No vendor queue implementation or license grant is silently
copied into the native source; see `NOTICE.md`.
