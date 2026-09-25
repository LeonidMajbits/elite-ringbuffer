# v1.1.0 candidate admission and deployment boundary

This candidate consolidates implementation and evidence. It does not turn finite
successful runs into a guarantee for every failure, machine or client.

| Area | Supported contract | Not supplied |
|---|---|---|
| Core | Admitted C11 ABI, CPU-coherent same-host mappings, native single-owner endpoints | Arbitrary compiler ABI, mixed foreign atomic overlays, malicious shared writers |
| SPSC | Two-way release/acquire ownership, cached covering cursors, optional single-waiter parking | A scheduling deadline or replacing a lost one-shot endpoint in place |
| NCQ | Publish-first SC dual queue, unique token claims and helper-only tails | Per-caller fairness, multicast, reserved-order side effects, MPMC parking |
| Lifetime | One outstanding token per endpoint; tracked Python views; checked counters/epochs | Revocation of arbitrary C pointers or indefinite generation wrap |
| Failure | Unknown owners retained; bounded whole-generation quarantine and managed successor | Autonomous orphan reinsertion, authority takeover, gap-free crash delivery |
| Persistence | Volatile process-shared transport | Host power-loss durability or exactly-once external effects |
| Evidence | Scoped models, native tests, raw receipts and independent numeric/membership oracles | Full ISO C11 source refinement, every original V1/V8 bound, original 15/50-ns qualification |
| Counters | Linux perf groups when exposed, Darwin public accounting and timer correlation | Generic cache misses equaling coherence invalidations, CNTVCT equaling core cycles |

Use a new immutable installation directory and fresh managed generation for a
release transition. Preserve a live, schedulable authority, all issued grants and
holder identities, and the four-object/two-quarantine budget. Do not reclaim old
storage to conceal capacity exhaustion. Stop or reject new admission when the
required recovery exceeds that budget.

For HFT or another hard service deadline, benchmark the full application and
external side effects under its actual offered schedule, topology, CPU budget,
OS power/thermal policy and failure model. A 250-ns median RTT does not establish
a one-way tail bound, and Python native-kernel payload rates do not establish
Python-bytecode serialization capacity. The retained asymmetric matrix includes
very slow outcomes; they are part of the release's evidence, not discarded noise.

Security reporting: use the owner's existing trusted private contact route before
publicly distributing a new actionable defect. No unverified email address or
response-time promise is invented in this package. The original audit is retained
under `docs/ADVERSARIAL_AUDIT.md`; Turn 10 documents its remediation. Future valid
counterexamples override previous pass counts and require a new identified build.
