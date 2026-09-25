# Multi-process chaos and managed recovery — Turn 14

ELITEIPC remains version 1.0.1 / LE128-V1. This is a **verification extension**,
not a new queue or autonomous recovery daemon. Existing `src/`, `include/`, and
`bindings/` files are unchanged. The added harness uses the public library and
its preexisting opt-in test hooks. Application payloads travel through POSIX
shared memory, not the grant/evidence pipes.

## Build and reproduce

```sh
python3 tools/verify_release.py --strict
make CC=clang BUILD=build/chaos chaos-multiprocess check-chaos-multiprocess
python3 tools/run_chaos.py --build build/chaos --out chaos-new --trials 3
python3 tools/verify_chaos_results.py chaos-new --source-root . --build build/chaos
```

Use a new output directory. A standard three-trial campaign contains 93 cases:
90 real multi-process histories and three separate single-process authority
resource negatives. The timeouts cycle through 1, 10, and 100 ms; these are
observer policies, **not valid lease expiration/reuse deadlines**. Each ordinary
healthy old/new cohort has 10,000 measured messages, each signal-storm cohort has
32,000, and asynchronous retirement uses a one-million-offer old cohort plus a
one-million-message successor. The old asynchronous cohort is deliberately cut
short and reports published/read/pending/retained work separately.

The callable single-case form is:

```sh
build/chaos/chaos_multiprocess_hooks write_claim ncq 10000 10 1 1
build/chaos/chaos_multiprocess resume_read spsc 10000 10 1 1
build/chaos/chaos_multiprocess storm ncq 32000 10 16 16
build/chaos/chaos_resources
```

For controlled NCQ cases, the last `1 1` arguments are bootstrap CLI placeholders:
the native plan explicitly provisions **3P/2C for a writer victim, or 2P/3C for a
reader victim**, leaving 2P/2C healthy peers. Storm cases use the actual requested
P/C pair. SPSC always uses 1P/1C. Successors use newly spawned 1P/1C processes;
these tests do not reattach the old healthy PIDs into a new session.

## Executables and instrumentation

`chaos_multiprocess` links the ordinary production archive. Its caller-level
cuts and externally delivered signals do not add source hooks to the queue.
`chaos_multiprocess_hooks` compiles the identical sources with the inherited
`ELITE_TESTING` hook surface. Hooks establish exact claim/publication cut points
but can change scheduling and synchronization; those runs are V2/V8, not timing
qualification or a substitute for weak-memory exploration.

`chaos_resources` is deliberately different: one process exercises the four-
object/two-quarantine budget and unsafe cleanup rejection while local views are
live. It is not counted as an additional interprocess crash run.

Every worker starts through `posix_spawn` + executable re-entry. The authority
registers its child before delivering a constructed one-shot grant. Each worker
attaches independently through `shm_open` / `mmap(MAP_SHARED)`. Logged numerical
mapping addresses need not differ across processes: address spaces are distinct.
The parent keeps its own diagnostic alias, closes it before destruction, and
never passes it to a worker. No unregistered descendants or mapping transfers
are allowed in this test contract.

## Scenario catalogue

| Scenario | Native cut / intended outcome |
|---|---|
| write_claim | QF head CAS won, before RESERVED/ordinary ownership metadata; token can be orphaned |
| write_reserved | RESERVED set, no valid publication |
| write_committed | COMMITTED set, QR entry not yet installed; phase is not membership |
| write_published | QR entry installed, tail help unfinished; sentinel is consumable |
| read_claim | QR head CAS won, before CONSUMED; reader token can be orphaned |
| read_returning | EMPTY set, QF entry not installed; EMPTY is not free membership |
| read_returned | QF entry installed, release has not returned; block is reusable |
| late_publication | Already-admitted QR insertion resumes after quarantine; old LP stays old |
| write_partial | Four of eight payload words written; kill before commit |
| read_held | Native read lease held; kill before return |
| resume_write / resume_read | False suspicion, successor holds a sentinel, old victim resumes |
| async_resume_write | Quarantine interrupts live 2P/2C traffic on monotonic suspicion |
| random_abort_kill | No-hook timing kill in reserve/write/abort traffic; exact instruction unknown |
| storm / storm_restart | USR1, ALRM, STOP/CONT with and without SA_RESTART |

The no-hook timing kill is **not** a universal random-crash test. The victim
never publishes in that scenario. Its last confirmed abort precedes an interval
in which it can hold zero or one token; the final stable inventory checks that
bound. Death before versus after a queue LP is not guessed from signal timing.

## Heartbeats do not revoke memory

Heartbeats are monotonic, endpoint-local progress records on a separate control
channel. The coordinator's last **received** timestamp plus a recorded interval
forms a suspicion deadline. The immutable wire header and slot descriptors gain
no timestamp, expiration, owner PID, or `commit_seq` field.

On expiry the authority may quarantine/retire and restore service elsewhere.
It must not increment a dead endpoint's cursor, edit a foreign epoch, republish
a COMMITTED token, or put an EMPTY-looking orphan into QF. A paused process can
resume with a raw pointer; even a correct generation check cannot undo a store
already made through that pointer. The false-suspicion tests retain old backing
and verify the successor sentinel remains unchanged after old work resumes.

For an exact stopped-cut test, native `waitpid(WUNTRACED)` establishes STOPPED;
the library's terminal reap API must still return NOT_READY. Killed children are
then observed through the library's owned-child reaping path. Signal request,
terminal observation, and authority notification are separate events. The
controlled lost-owner cases deliberately let healthy NCQ peers finish before
notifying the authority of terminal failure. This exposes old-generation
progress, not a hidden immediate restart.

## Signal-handler contract

The installed USR1/ALRM handlers assign `volatile sig_atomic_t` flags only. No
allocation, printing, locking, native queue operation, cleanup, or long jump
runs inside them. TERM/INT sets an interruption flag. SIGPIPE is ignored and
pipe errors are checked. SA_RESTART is explicitly selected per storm profile;
EINTR paths are handled outside the handlers.

Standard signals may coalesce, so sent counts and boolean observed flags are
not described as exact delivery counts. A signal delivered during a run does
not prove it interrupted one particular RMW instruction. STOP/CONT is confirmed
for a selected owned worker; it is never treated as terminal death.

## Token and message accounting

The current library allows completed queue transfers to lose their caller's
return receipt, and can retain tokens when their holder dies. The safe claim is
not zero loss of old usable capacity. With all old participants terminal, the
observer reconstructs live QF and QR membership and computes the complement:

`F ∪ Q ∪ X = {0, …, N−1}`, pairwise disjoint.

Here X means unavailable/orphaned or explicitly abandoned old tokens. Entire
generation quarantine separately charges all N blocks against the backing
budget. Adding N and X would double count the same storage. During execution,
not every token's owner is recoverably observable: unknown transfers remain an
explicit responsibility rather than a fictitious free slot.

The observer reads ordinary descriptor fields **only after every participant in
that generation has exited or been terminally reaped**. It never scans a live
partial payload. It checks the immutable 512-byte header/CRC, permitted gate and
failure words, entry cycles, cursor relationships, packed phase/epoch agreement,
and final partition. Historical entry words outside the live interval are not
extra members. The legal NCQ `H = T + 1` state is admitted.

Every ordinary delivered message is validated over all eight 64-bit payload
words by its owning native consumer. Per-consumer bitmaps are exported. Replay
requires unique exact membership, producer range/quota agreement, and token
reconciliation. The logs do not export every payload word, so their offline
checker validates accounting plus source-bound native checking results, not a
second replay of every read byte.

In asynchronous retirement, completed producer prefixes form the published ID
set. The disjoint consumer bitmaps form the read set. Any published-but-unread
IDs must occur exactly in the final QR membership. Cooperative read/write leases
that receive RETIRED/RETAINED are explicitly abandoned after alias closure and
must occur in X. This stops old service safely; it does not give gap-free or
exactly-once delivery across a session switch.

## Important current API consequence

After QUARANTINED, `elite_read_release` returns RETIRED/RETAINED, not a successful
QF return. The wrapper must stop using aliases and call `elite_abandon_retained`
when appropriate. The token stays outside QF until whole old-storage disposal.
The same retained handling applies to an uncommitted old write. A QR operation
already admitted before retirement can still linearize afterward, exclusively
in the old generation. The late-publication scenario preserves that case.

SPSC cannot continue its old lane without its sole failed producer or consumer.
The tests explicitly declare old-lane service unavailable and create a distinct
successor. No same-generation endpoint slot is reset or reused.

## Evidence, verifier, and failure behavior

Each campaign directory includes a frozen `plan.json`, one raw `.jsonl`, stderr,
run receipt and replay per case, and `COMPLETE.json` only after every case passes.
The plan binds 18 source/build inputs and all three executable hashes. Source
and binary identities are rechecked before campaign completion. Verification
never regenerates or rewrites this evidence.

`tools/verify_chaos_results.py` has no dependency outside Python 3.11+ standard
library and no removable assertion-based checks. It rejects semantic mutations
even when a fixture is rehashed. An absent completion marker, a timeout, unknown
cut masquerading as exact, or incompatible artifact does not count as a pass.

A full-result replay with a newly rebuilt executable may legitimately have a
different binary hash (paths/debug metadata/toolchain). Supply `--build` only
when checking the exact tested binary; `--source-root` independently checks the
source identity. Hashes do not provide remote attestation or prove OS fencing.

The native coordinator has a 60-second deadline and workers 90 seconds. The
runner defaults to a 75-second outer timeout, asks the owned coordinator to exit,
then after five seconds may kill only its created process group. No process-name
or namespace-wide cleanup is used. Native cleanup waits for its own children
before unlinking its recorded names. An unresponsive authority or unkillable
kernel task cannot be turned into a bounded cleanup guarantee. Such a run stays
incomplete and retains its names/events for operator investigation.

## Limits and next qualification

This delivery ran selected finite native histories on Linux. It is not all C11
executions, Apple-native qualification, a latency benchmark, all 100 repeats at
every Turn 5 cut, or the 1,000-per-class recovery campaign. The resource negative
is single-process. The new harness does not exhaust authority death, parked wake
gaps, pending-grant crashes, random consumer publication intervals, every payload
word cut, power failure, malicious writes, or externally transferred mappings.

Recovery timing uses authority-side monotonic observations. Existing healthy old
processes close; freshly spawned successor processes provide service. The field
`successor_all_attached_ns` measures notification/suspicion to their READY
observations, and `successor_first_validated_read_ns` ends at the observed held
sentinel read. These are descriptive intervals, not a p99≤100 ms certificate.

For actual deployment, keep one live authority, bound pending objects/grants,
end every alias before detach, retain unknown outcomes, and apply an application
replay/deduplication policy when losing or duplicating external work is forbidden.
There is no new automatic authority takeover, raw-memory lease revocation, or
power-loss persistence in this release.
