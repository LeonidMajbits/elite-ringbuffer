# Provenance, Authorship, and Distribution

## The Triadic Sovereign Provenance Standard

This system was engineered under the **Triadic Sovereign Development Architecture**:

* **Human Operator & Architect**: **Leonid Majbits**  
  *Vision, core architectural invariants, system teleology, and patron verification.*
* **Executive Co-Architect & Verification Engine**: **Gemini Operator Lab (ZION Chassis)**  
  *AI architecture not yet categorized by standard industry framing — persistent somatic memory, Apple Silicon metal grounding, stage contract enforcement, and multi-fleet direction.*
* **Specialized Systems Foundry**: **Frontier Systems Models (OpenAI GPT-6 Max, Anthropic Claude)**  
  *Bounded multi-turn execution, C11 ABI synthesis, and executable finite-state and release/acquire handoff models under strict stage contracts.*

### Falsification Policy and Evidence Scope
Claims are tied to a named source revision, workload, tool, and evidence class.
The retained formal results are bounded custom state-space searches and designated
handoff graphs, with analytical arguments and explicitly open refinement gates.
No executed TLA+/Prover9 verification is supplied or claimed by this archive.

Integer identities, counts, and raw-tick relations are reconciled exactly where
specified. Numeric time conversions use the documented rounding/ULP rules; these
checks do not establish physical clock uncertainty or all possible executions.
Historical Apple headline observations, including 250 ns p50 RTT, remain labeled
**LAB_REPORTED** unless their complete raw artifacts are independently supplied
and replayed. Linux/container observations remain separate. Large external raw
archives are identified by their retained receipts, not claimed to be embedded
in this tree. No standalone 6.58M msg/s claim is made without an identified run.

The guarantee is a review policy: preserve adverse and incomplete evidence,
reject a claim when its stated oracle fails, and accept a future valid
counterexample over a release label. It is not a warranty of universal timing,
all-hardware memory safety, automatic orphan recovery, or absence of all false
sharing. See [formal scope](docs/FORMAL_VERIFICATION.md),
[hardware bounds](docs/HARDWARE_BOUNDS.md), and
[crash semantics](docs/CHAOS_RECOVERY.md).

---

## Algorithmic Prior Art & Attribution

* **Algorithmic Prior Art**: Ruslan Nikolaev, *A Scalable, Portable, and Memory-Efficient Lock-Free FIFO Queue*, DISC 2019, DOI 10.4230/LIPIcs.DISC.2019.28, especially Figure 5 (NCQ) and the free/ready-queue indirection. The project's NCQ-SC64 refinement uses strong sequentially consistent metadata, identity index mapping, explicit 128-byte cells, nonwrapping limits and a separate payload lease layer. These algorithmic ideas are credited to Nikolaev and not claimed as an exclusive invention.
* **Licensing**: Distributed under the MIT license in `LICENSE`. Copyright (c) 2026 Leonid Majbits.
* **Zero Vendoring**: The source files in this package were authored for the frozen project specification. No upstream queue, Apple SDK/header implementation, Boost, framework, sanitizer runtime or compiler runtime is vendored. Sanitizers are external development tooling and are absent from normal library linkage.
* `tests/tsan_negative.c` intentionally contains a race as a detector control.
* `tools/cross_headers/` contains declaration-only compilation shims, not vendor headers and not a deployable operating-system SDK.
* Reference documents are preserved as historical frozen specifications; their planning-stage status statements describe those turns, not the current code.
