# Lane V1 — Executable models and formal claim boundaries

This delivery adds a dependency-free, exact-state transition explorer, a separate
reads-from / happens-before graph checker, symbolic boundary proofs, structural
mutation witnesses, optional Promela models, and optional C11/RC11 checker inputs.
The production queue and binding implementation bytes are not edited.

## What each lane means

1. `formal/models/spsc.py` enumerates control, cursor snapshots, two ordinary
   payload words, reservations, aborts, and retained read ownership. It permits
   nondecreasing conservative stale cursor observations. It does NOT turn this
   interleaving projection into a C11 weak-memory proof.
2. `formal/models/ncq.py` enumerates both QF and QR, saved head/tail/entry
   observations, strong CAS outcomes, helpable tails, epoch mirrors, packed-status
   projections, two payload words, aliases, and ghost token ownership. At most one
   shared queue atomic occurs per transition. Private work and observational ghost
   updates can accompany it. Queue metadata is SC as in the frozen source.
3. `formal/handoff.py` enumerates designated covering reads-from assignments for
   fixed finite handoff skeletons, constructs sb/sw/hb, checks atomic coherence,
   and checks all conflicting ordinary accesses for happens-before order. This
   is a small explicitly defined C11-RA fragment, NOT a complete ISO C or RC11
   frontend. The unbounded protocol proof is written separately in the report.
4. `formal/boundaries.py` checks small-width arithmetic exhaustively, native-width
   boundary values, an explicit 260-advance scalar wrap witness, and six
   attach-versus-retire schedules. It also supplies a symbolic starvation lasso.
5. `formal/targeted.py` pauses an owner before or after QR publication while peers
   complete work, then checks the published owner's late bookkeeping. This is a
   prescribed schedule, not an exhaustive stopped-process/lifecycle proof.

## Ghost state versus implementation

`U` is the contiguous publication frontier. It is not a proposed shared field.
`owner[b]` is changed at the actual queue LP, not at the later function response.
Historical entry words outside `[H,U)` are not live tokens. Model checks inspect
all ghost state atomically only as an observer; they do not supply locks, fences,
or access permissions to the program. This is why a producer may retain the old
local block index after installation while no longer owning its payload.

The NCQ invariant includes `H <= U`, `0 <= U-H <= N`, and `U-1 <= T <= U` for
both queues. `H=T+1` is legal and reached in baseline exploration. The model does
not require global numeric message IDs or consumer completion order to increase.

Cyclic queue reuse is genuine: QF starts at H=0,T=N and QR at H=T=N. Entry identity
is `(floor(ticket/N), block)` with physical position `ticket % N`. There is no FAA
or globally reserved unpublished QR position. Status generations and ordinary
mirrors can temporarily differ while the writer exclusively initializes them.

## Local reduction and scheduling scope

There is no symmetry reduction, bitstate hashing, randomized search, or partial
order reduction. The complete immutable state is the dictionary key; hash
collisions are compared for equality. Dead local observations are explicitly
cleared after they cease affecting any later branch, except mutant retries that
intentionally retain them. All model transition choices are traversed.

A paused actor can be absent from a finite schedule for arbitrary duration. The
no-progress analysis removes LP/response edges and tests the remaining graph for
cycles by exact topological elimination. No scheduler-stutter edge is inserted:
a stopped processor is not an executed queue step. Successful empty try responses
count as API progress, never as useful message throughput. A finite result cannot
prove infinite useful service with no producer, permanently held tokens, or an
unscheduled required participant.

## Atomicity and ordering mutations are distinct

A strong relaxed CAS remains atomic. It cannot grant the same nonrepeated head
value to two competing expected-value claims. A weak memory order may remove the
cross-location visibility proof; it does not split the read-modify-write.

The order-only NCQ mutation removes entry publication/observation synchronization.
The actual source reads the ordinary epoch BEFORE its subsequent acquire status
load, so the first handoff exposes a race on that metadata. The later status
acquire cannot retroactively order the earlier read. No duplicate-ticket witness
is fabricated for this order-only mutation.

Separate structural mutants replace a head CAS with split/unconditional update,
retain a stale entry after a failed head CAS, overwrite a competing publication,
advance tail first, access a block after LP, or return with a live read alias.
They must be rejected by the same ownership/frontier oracle as the baseline.

## Optional external checker inputs

`formal/spsc_ownership.pml` and `formal/ncq_sc64.pml` are handwritten Promela
specifications of SC/control projections with LTL capacity/frontier and
termination claims. Native Spin was unavailable during this delivery. They are
provided for review and external execution, NOT as executed Spin results.

For example, in a disposable build directory with one model copied there:

```
spin -a ncq_sc64.pml
cc -O2 -DSAFETY -DNOCLAIM -DNOREDUCE -o pan pan.c
./pan -m1000000
```

That first run checks assertions. For each LTL claim, generate/compile a separate
non-SAFETY verifier and select the claim explicitly, e.g. `./pan -a -N frontier`
or `./pan -a -f -N termination`. Weak process fairness is an extra condition for
that termination claim. Do not enable BITSTATE or hash-compact compression and
label it exact exhaustion. A depth/memory limit or an acceptance cycle is not PASS.
Consult the installed Spin version's documented switches before use.

`formal/c11/*.c` are ordinary C11/pthread handoff inputs for a suitable pinned
checker such as GenMC. They are syntax-built with GCC and Clang here. They have
NOT been executed under GenMC or CDSChecker in this delivery. Native success,
particularly on x86, cannot replace the missing weak-memory tool execution.

## Reproduction

```
make check-formal
python3 tools/run_formal.py --out build/formal-run
python3 tools/verify_formal_results.py build/formal-run --replay-baselines
```

The output directory must be new. `--extended` adds the substantially larger
N=4,2P/2C,eight-reservation configuration with explicit time/state caps. Hitting a
cap returns 2 and makes that campaign incomplete. Default bounds, not the whole
Turn 5 V1 register, are what a successful default invocation establishes.

The verifier always replays structural counterexamples. With
`--replay-baselines`, it re-executes each complete state search and compares state
and transition totals, terminal totals, state-sequence digest, and cycle result.
Without that option it authenticates receipts/source and checks completeness, but
does not independently rerun baseline exhaustion. Manifests are integrity checks,
not signatures or a proof that a dishonest result producer could not forge data.

## Final coverage and build caveats

The completed default population contains 2,158,499 states and 5,700,615
transitions. It does not include the capped N4/2P2C/eight-reservation run.
The final targeted set also pauses a QR observer across nine peer removals
and confirms its stale claim fails after more than two physical laps.

Receipt replay always recomputes the designated handoff, scalar and targeted
results. The exact-state explorer also rejects nonterminal deadends; a separate
self-test checks this negative. The new verifier suite has 34 self-tests and
15 semantic-receipt tests.

The C11 pthread inputs are candidate GenMC inputs, not directly adapted
CDSChecker tests. CDSChecker's documented thread API, entry point and ordinary
access instrumentation need a separately reviewed adapter. No external checker
execution is claimed.

For a custom BUILD path, the hardened Make recipe forwards the corresponding
ELITE_TOPOLOGY_BINARY to Python test discovery; otherwise topology fixtures can
look in the wrong default build directory.
