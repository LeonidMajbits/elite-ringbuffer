# Contributing

Start from a verified source inventory and preserve the wire ABI and ownership
contract. Changes to memory orders, linearization points, phase semantics, layout,
Python alias lifetime, or reclamation require an explicit proof/test update rather
than only a faster benchmark. Keep code, finite model bounds, native evidence and
hardware claims distinguishable. A valid counterexample overrides pass counts.

Run `make CC=clang release-check check-seeding` and the installed CMake consumer
check. CI tool results have the scopes in docs/CI_MATRIX.md. Add deterministic
negative tests for repaired bugs; never remove the original failed history just to
make the record green. Formal source hash updates require a documented review,
not automatic regeneration to bypass drift. Windows, unregistered mapping transfer,
GPU/DMA ownership and live pointer revocation are not implied supported features.

Include OS/architecture, compiler flags, payload/participant/workload, exact source
hash and a minimal reproduction. Do not paste secrets, private payloads, user file
paths or unrelated process data. For safety defects use SECURITY.md. Public patches
retain the MIT terms and algorithmic attribution in NOTICE.md. Maintainers decide
actual branch protections, signed tags, registry publication and release identity.
