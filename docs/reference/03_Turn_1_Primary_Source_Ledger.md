# Turn 1 — Primary Source Ledger

**Research date:** September 23, 2026.  
**Use:** Resolve source identifiers in the architecture and proof documents.  
**Method boundary:** Standards drafts, vendor/OS/compiler documentation and source, original algorithm authors, and original research publication records. No benchmark has been reproduced. No source implementation has been copied into the deliverables. Mutable main/latest pages must be pinned to exact revisions before algorithm adoption in Turn 2.

## Language, compiler and memory model

**S01 — C++ working draft: lock-free properties.**  
https://eel.is/c++draft/atomics.lockfree  
Supports: the distinction between guaranteed atomic properties and the recommended address-free behavior useful for differently mapped shared memory. Does not establish the particular ABI of every C11/C++/Rust implementation.

**S02 — C++ working draft: ordering and consistency.**  
https://eel.is/c++draft/atomics.order  
Supports: release/acquire ordering and synchronization through the value observed. The project must separately admit an interprocess implementation contract.

**S03 — Dmitry Vyukov: bounded MPMC queue.**  
https://sites.google.com/site/1024cores/home/lock-free-algorithms/queues/bounded-mpmc-queue  
Supports: the sequence-numbered bounded design and the author's explicit distinction between a mutex-free queue and formal lock-freedom. Used for the reservation-hole critique, not adopted as the final MPMC algorithm.

**S04 — GCC: atomic built-ins.**  
https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html  
Supports: memory-order parameters, primitive lock-freedom queries, and implementation/fallback considerations. Does not certify the unbuilt project binary.

**S05 — University of Cambridge memory-model research group: C/C++11 processor mappings.**  
https://www.cl.cam.ac.uk/~pes20/cpp/cpp0xmappings.html  
Supports: baseline x86 and AArch64 instruction mappings, including LDAR/STLR and acquire/full fences. The page explicitly presents mappings for discussion, not an exhaustive guarantee for every compiler and optimization.

**S06 — Linux kernel: LKMM recipes.**  
https://docs.kernel.org/dev-tools/lkmm/docs/recipes.html  
Supports: useful publication and ordering patterns. Kernel memory-model primitives are not automatically interchangeable with userspace C11 semantics.

**S07 — Linux kernel: circular buffers.**  
https://www.kernel.org/doc/html/v5.6/core-api/circular-buffers.html  
Supports: single-producer/single-consumer ordering and circular-buffer reasoning. This versioned reference is conceptual prior art, not a claim that kernel helpers can be dropped into a portable userspace IPC ABI.

## ARM and Apple architectural grounding

**S08 — Apple: addressing architectural differences in macOS code.**  
https://developer.apple.com/documentation/apple-silicon/addressing-architectural-differences-in-your-macos-code  
Supports: querying architectural properties such as page/cache-line information rather than assuming a universal layout. Browser extraction of the full page is JavaScript-limited; the relevant architectural-query guidance was visible in search retrieval. This reference does not establish that every M-series coherence level has a 128-byte line.

**S09 — Arm: ARMv8 sequential consistency.**  
https://developer.arm.com/community/arm-community-blogs/b/tools-software-ides-blog/posts/armv8-sequential-consistency  
Supports: weak ordering, load-acquire/store-release, and the difference between hardware memory-order behavior and a naive source-order assumption.

**S10 — Arm: enabling RCpc in GCC and LLVM.**  
https://developer.arm.com/community/arm-community-blogs/b/tools-software-ides-blog/posts/enabling-rcpc-in-gcc-and-llvm  
Supports: target/compiler-dependent acquire lowering, including LDAPR. Exact deployment feature selection remains a future binary audit.

**S11 — Apple libplatform: public shared-address wait header.**  
https://raw.githubusercontent.com/apple-oss-distributions/libplatform/main/include/os/os_sync_wait_on_address.h  
Supports: macOS 14.4 availability, aligned 4/8-byte wait values, matching process-shared flags, compare-and-wait behavior, timeout variants, error handling, and absence of priority-inversion avoidance. Current main-branch header, not a pinned SDK snapshot.

**S12 — Apple XNU: private ulock interface.**  
https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/bsd/sys/ulock.h  
Supports: the distinction between private/shared ulock operations and the private interface boundary. Not the proposed stable application API.

## OS waiting, mappings, and capability lifetime

**S13 — Linux man-pages: futex.**  
https://man7.org/linux/man-pages/man2/futex.2.html  
Supports: futex word and shared/private operation semantics.

**S14 — Linux man-pages: FUTEX_WAIT.**  
https://man7.org/linux/man-pages/man2/FUTEX_WAIT.2const.html  
Supports: atomic expected-value comparison and blocking, mismatch behavior, interruptions and spurious wake-up considerations.

**S15 — Linux man-pages: mmap.**  
https://man7.org/linux/man-pages/man2/mmap.2.html  
Supports: MAP_SHARED mappings and mapping-lifetime considerations. Does not imply persistence, failure recovery or a particular language atomic ABI.

**S16 — Apple: Mach overview.**  
https://developer.apple.com/library/archive/documentation/Darwin/Conceptual/KernelProgramming/Mach/Mach.html  
Supports: Mach IPC objects, rights and task-local namespaces. The project still requires an explicit rights/bootstrap protocol for any semaphore fallback.

**S17 — Apple libdispatch: semaphore implementation.**  
https://raw.githubusercontent.com/apple-oss-distributions/libdispatch/main/src/semaphore.c  
Supports: dispatch semaphores being allocated runtime objects, not portable semaphore values created by copying their bytes or pointer into an mmap region.

**S18 — LLVM libc++: atomic wait implementation.**  
https://github.com/llvm/llvm-project/blob/main/libcxx/src/atomic.cpp  
Supports: current process-private Linux futex and Darwin wait flags, with deployment-dependent public/private Darwin backend selection. This is an implementation example, not an assertion that every standard-library implementation uses identical mechanisms.

## Queue algorithms and prior art

**S19 — Ruslan Nikolaev, DISC 2019: A Scalable, Portable, and Memory-Efficient Lock-Free FIFO Queue.**  
https://drops.dagstuhl.de/entities/document/10.4230/LIPIcs.DISC.2019.28  
Supports: the published SCQ candidate and its stated bounded, linearizable, lock-free design goals. Turn 1 reviewed the publication record/abstract and related author material; the full proof and an IPC composition proof are Turn 2 obligations.

**S20 — Original author repository: lfqueue.**  
https://github.com/rusnikola/lfqueue  
Supports: SCQ variants, portability/primitives discussion and author-maintained reference implementation. No code is vendored or adopted in this turn; variant selection and licensing review precede any later implementation reuse.

**S21 — DPDK ring library, retrieved as documentation version 26.07.0.**  
https://doc.dpdk.org/guides/prog_guide/ring_lib.html  
Supports: SP/SC, MP/MC, RTS, HTS and start/finish/zero-copy restrictions; HTS serializes the relevant producer or consumer operation. Do not infer formal lock-free arbitrary-duration payload borrowing from the word “lockless.”

**S22 — Aeron: tryClaim cookbook.**  
https://aeron.io/docs/cookbook-content/aeron-try-claim/  
Supports: claim/commit/abort as an explicit protocol. Default timeouts are deliberately not adopted as project requirements.

**S23 — LMAX Disruptor user guide.**  
https://lmax-exchange.github.io/disruptor/user-guide/  
Supports: sequencers, consumer dependency/gating, multicast use, and selectable wait strategies. These are not identical to the proposed MPMC work-sharing semantics.

**S24 — Aeron: log buffers and images.**  
https://aeron.io/docs/aeron/log-buffers-images/  
Supports: the surrounding log/driver model and unblocking responsibilities. Borrowing an algorithmic idea does not import Aeron's runtime into this zero-framework project.

**S25 — Aeron / Agrona: concurrent structures.**  
https://aeron.io/docs/agrona/concurrent/  
Supports: ring/queue distinctions and behavior around uncommitted records. Recovery guarantees must not be inferred merely from API similarity.

**S26 — Simon Cooke: original BipBuffer author's historical account.**  
https://accidentalscientist.com/2010/02/the-darker-side-of-google-open-source-taking-without-attribution.html  
Supports: attribution and historical context for BipBuffer. A complete reconstruction of its spatial invariants and its suitability for a new concurrent protocol remains Turn 2 work; this history is not a concurrency proof.

## Timing, scheduling, lifecycle, and Python

**S27 — Apple Technical Q&A QA1398: Mach absolute time.**  
https://developer.apple.com/library/archive/qa/qa1398/_index.html  
Supports: mach_absolute_time and timebase conversion. Does not certify actual resolution, overhead or scheduling on the target machine.

**S28 — Apple: prioritizing work at task level.**  
https://developer.apple.com/library/archive/documentation/Performance/Conceptual/power_efficiency_guidelines_osx/PrioritizeWorkAtTheTaskLevel.html  
Supports: QoS and priority-management context. It does not grant a hard nanosecond deadline or prove that Linux-style core pinning is available on Darwin.

**S29 — Linux man-pages: pidfd_open.**  
https://www.man7.org/linux/man-pages/man2/pidfd_open.2.html  
Supports: identity-bound process lifecycle handles. Correct registration timing and death-versus-pause policy are additional project obligations.

**S30 — CFFI: using ffi/lib objects.**  
https://cffi.readthedocs.io/en/latest/using.html  
Supports: buffer and cdata ownership/lifetime behavior and threading interface considerations. Keeping a CFFI object alive does not independently keep a ring slot reserved; that is the project's lease invariant.

**S31 — CFFI: installation.**  
https://cffi.readthedocs.io/en/latest/installation.html  
Supports: CFFI's external package/dependency boundary. “Zero dependencies” is scoped to the native core rather than misleadingly applied to the complete optional binding stack.

**S32 — Intel: instruction/timing reference entrypoint and speculation guidance.**  
https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html  
https://www.intel.com/content/www/us/en/developer/articles/technical/software-security-guidance/technical-documentation/analyzing-bounds-check-bypass-vulnerabilities.html  
Supports: the authoritative manual family for future timestamp-instruction audit and the separate nature of transient-execution mitigation. Turn 1 did not analyze or screenshot a downloaded SDM PDF; exact timestamp instruction sequences are deferred to the Turn 5 specification and subsequent compiled-binary audit.

## Evidence rules for the next turn

A vendor header documents an API; it does not prove our notification state machine. A queue paper proves its stated algorithm under its stated model; it does not prove a new process-crash or zero-copy lease wrapper. A compiler mapping describes possible lowering; it is not disassembly of a binary that does not yet exist. A published benchmark is not a measurement on Leon's hardware.

Use these boundaries when extending the project. Preserve proposed-versus-proved-versus-measured status rather than upgrading claims by repetition.
