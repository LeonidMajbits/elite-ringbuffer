# Native API and integration contract

## The frozen bytes versus the native interface

`elite_spsc_ring_header` and `elite_mpmc_ncq_header` are exactly 2048 bytes and
128-byte aligned. Their immutable prefix is exactly 512 bytes. Descriptors and
queue-entry cells are exactly 128 bytes; participant records are 256 bytes.
Every frozen member offset and explicit reserved range is represented and
asserted in the public header. `SOURCE_MANIFEST.sha256` identifies that header.

Native config, result, grant, receipt, span, and opaque lease values are **not**
additional shared-memory regions. Grant transport is the application's trusted
control plane. Do not serialize native padding or `pid_t` and claim a portable
wire protocol. Do not overlay a C++, Rust, or Python atomic representation.

## Manager sequence

1. Supply nonzero, nonreused host and authority identities. Create one
   `elite_authority` with a fixed maximum backing size. The caller must preserve
   the no-reuse condition over unresolved old holders; this is not inferred from
   timestamps. No cryptographic authentication service is supplied.
2. Predefine nonzero unique endpoint IDs, process-incarnation IDs and fixed roles.
   `elite_create` reserves a manager slot before exclusive OS creation. It
   constructs the whole segment, initializes each atomic and publishes READY.
   It can return an error **and a nonnull object** needing cleanup.
3. Activate the candidate with `elite_object_activate`. At most one object is
   active; another may be staged. Register each owned child or self *before*
   issuing a grant. Linux obtains a process-level pidfd; owned-child status
   prevents PID reuse during registration. Another thread must not independently
   reap a registered child before this authority handles its failure.
4. `elite_object_grant` issues one constructed grant per endpoint and charges its
   potential holder before returning. Deliver it exactly once over the owned
   channel to the assigned process. Copying grant bytes is not an authenticated
   capability handoff. Unregistered fork descendants or descriptor/mapping
   transfers violate the contract.
5. Worker `elite_attach` opens without create/truncate, validates length/mode,
   acquires the known constructed gate, checks the exact prefix/CRC/identities,
   validates immutable registry/reserved bytes, claims NEW→JOINING, then competes
   with retirement on the combined state/count word. A failed attach can return a
   cleanup-only handle. A retirement race can return an enrolled RETIRED handle.
   The caller must dispose of every nonnull handle explicitly.
6. Finish aliases and calls, detach, and deliver the cleanup receipt to the
   authority. Resolve it using `elite_object_ack_cleanup`. Only the recipient
   that actually claimed the one-shot record can produce a resolving receipt;
   cleanup of a rejected duplicate cannot free the original holder's lifetime.
7. Stop grants, account for all holders, destroy the object, then destroy the
   authority. Destruction never discovers quiescence by subtracting guessed dead
   references or scanning phase words.

The authority's own process is fixed. A forked copy cannot operate that manager.
The application must monitor authority liveness and stop using new grants after
its loss; the library has no invisible monitoring thread or automatic takeover.
A prospective replacement authority must reconcile/fence the old cohort before
starting another allowance. Session generation is injective for the local
nonwrapping authority sequence, but globally nonreused authority/session identity
remains a bootstrap obligation, not a proof from a pseudorandom value.

## Data operations and output ownership

| Call | Success | Important failure behavior |
|---|---|---|
| `elite_write_reserve` | WRITE_OWNED and span capacity B | NO_CAPACITY is an observation, not N committed messages |
| `elite_write_commit` | PUBLISHED; lease consumed | Pre-LP error retains responsibility; post-LP notify error remains PUBLISHED |
| `elite_write_abort` | RETURNED; unpublished lease consumed | Retired/corrupt terminal state can retain the token instead |
| `elite_read_borrow` | READ_OWNED, valid length/type/ID/epoch | Integrity error after claim returns a retained lease for fail-closed disposal |
| `elite_read_release` | RETURNED; lease consumed | No post-QF-publication descriptor cleanup |
| `elite_view_retain/end` | Changes local tracked view count | Cannot make an untracked raw pointer revocable |
| `elite_wait_data/space` | Hint to retry predicate | Finite budget; polling or MPMC returns UNSUPPORTED |
| `elite_detach` | Destroys local handle; receipt returned | BUSY until lease/call/view ends; error can retain cleanup obligation |

Lease bytes are opaque. They bind the session, authority, endpoint, serial,
block, generation and role. An invalid or stale token is rejected using local
owned state before reading an unowned descriptor. A valid connection pointer
must still designate a live C object; arbitrary dangling pointers cannot be
made safe by this API. Do not alias output arguments with an existing active
lease/span or with shared control storage.

All eight frozen transfer outcomes remain distinct. A RETAINED result may mean
a live local lease or storage retained in a retired generation, as documented
for the called operation. It is never free storage. A failed operation does not
license a blind resend. Do not advance an application send ID after an uncertain
publication without a separate deduplication/reconciliation policy.

## Retirement, drain and token abandonment

`elite_retire` or `elite_object_retire` closes enrollment through the composite
SC gate update and records the first supplied failure reason. Calls already
inside a valid transfer can still publish to the old object. Its identity and
backing never redirect into a successor.

A consumer can request `elite_enable_drain`. It admits new old-generation
borrows only in RETIRE_REQUESTED, with no disqualifying integrity/counter/protocol
failure. It is not allowed in QUIESCING or QUARANTINED. A valid retained borrow
may be completed/returned while the generation remains trusted and in the
permitted state. The manager has no dedicated public QUIESCING setter in this
core release: caller-controlled local stopping plus resolved holders precedes
SEALED/destruction, or the old object is explicitly quarantined.

For an integrity or terminal counter failure, close every raw/tracked alias and
call `elite_abandon_retained` with the current lease. This only clears *local*
lease responsibility and poisons that endpoint. It performs no QF insertion,
slot reset, or guessed recovery. The old token remains unavailable until whole
backing reclamation is safe. Detach can then report mapping cleanup.

`elite_object_quarantine` retains an old physical mapping while permitting a
separately allocated successor to be activated. There are at most two retained
quarantines, one active object and one staged candidate. Attempts beyond that
budget stop admission; they never evict an uncertain writer's bytes.

## Fail-closed cleanup limits

A grant that fails before it can claim NEW→JOINING may leave a deliberately
unresolved authority holder record. The library cannot use a failed duplicate's
receipt to establish that the original grant will never be used. Terminal
owned-child evidence or a later more complete application reconciliation is
needed; there is no unsafe cancel-grant shortcut in this version.

An ambiguous close failure retains a `cleanup_uncertain` object/connection rather
than retrying an FD number that the OS may already have released and reused.
Subsequent cleanup reports OUTCOME_UNCERTAIN. This can block normal manager
destruction and require process termination/reconciliation. It is an availability
limitation, not permission to reset the allowance or leak unbounded replacements.

No arbitrary control corruption recovery is supplied. SIGKILL of an otherwise
conforming writer is a stop failure; a buggy writer that first overwrites other
participants' queue controls is a different threat. CRC is not authentication.

## Platform boundary

Normal polling needs C11 and POSIX APIs. Native process identity and blocking
use isolated platform extensions: Linux pidfds/futexes, Darwin owned-child wait
status and public address-wait APIs. They are OS facilities, not third-party
framework dependencies. The statement 'strictly POSIX' does not imply futex or
Darwin address waits are POSIX-standard functions.

Darwin compilation/linking still needs an actual macOS 14.4+ SDK and execution
host. The bundled freestanding assembly shims declare only types/functions
needed to lower the two queue translation units; they must not be used to build
the shared-memory or waiting adapter as a fake SDK.
