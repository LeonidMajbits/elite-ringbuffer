/** @file elite_api.h
 * C/C++/FFI calling interface. Shared atomics are never redefined here.
 * All handles are process-local, single-owner and require external serialization.
 * Every buffer alias ends before commit/release/detach. A non-OK result must be
 * interpreted together with its outcome; PUBLISHED must never be resent blindly.
 * Raw pointers require the caller's lifetime discipline; managed Python exports
 * add a tracked buffer lifetime. See docs/INTEGRATION.md and docs/API.md.
 */
#ifndef ELITE_API_H
#define ELITE_API_H
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include "elite_version.h"
#ifdef __cplusplus
extern "C" {
#endif
struct elite_immutable_header;
#define ELITE_ABI_VERSION UINT32_C(0x00010000)
#define ELITE_ISOLATION UINT32_C(128)
#define ELITE_QUANTUM UINT32_C(16384)
#define ELITE_EPOCH_CEILING ((UINT64_C(1) << 62) - UINT64_C(2))
#define ELITE_MAX_WAIT_SLICE_NS UINT64_C(10000000)
#define ELITE_NAME_BYTES 31
#define ELITE_SPSC UINT32_C(1)
#define ELITE_NCQ UINT32_C(2)
#define ELITE_POLL_ONLY UINT32_C(0)
#define ELITE_PARKABLE_SPSC UINT32_C(1)
#define ELITE_CHECKSUM_NONE UINT32_C(0)
#define ELITE_CHECKSUM_CRC64 UINT32_C(1)
#define ELITE_PRODUCER UINT32_C(1)
#define ELITE_CONSUMER UINT32_C(2)

/* Numbers are frozen. Enums are NOT stored in the shared format. */
#define ELITE_OK UINT32_C(0)
#define ELITE_NO_DATA_OBSERVED UINT32_C(1)
#define ELITE_NO_CAPACITY_OBSERVED UINT32_C(2)
#define ELITE_RETIRED UINT32_C(3)
#define ELITE_BUSY UINT32_C(4)
#define ELITE_BAD_ABI UINT32_C(5)
#define ELITE_BAD_LAYOUT UINT32_C(6)
#define ELITE_BAD_IDENTITY UINT32_C(7)
#define ELITE_NOT_READY UINT32_C(8)
#define ELITE_COUNTER_LIMIT UINT32_C(9)
#define ELITE_INTEGRITY UINT32_C(10)
#define ELITE_OS_ERROR UINT32_C(11)
#define ELITE_OUTCOME_UNCERTAIN UINT32_C(12)
#define ELITE_AUTHORITY_REQUIRED UINT32_C(13)
#define ELITE_UNSUPPORTED UINT32_C(14)
#define ELITE_INVALID_LEASE UINT32_C(15)
#define ELITE_INVALID_ARGUMENT UINT32_C(16)
#define ELITE_CREATE_CONFLICT UINT32_C(17)

#define ELITE_NONE UINT32_C(0)
#define ELITE_WRITE_OWNED UINT32_C(1)
#define ELITE_PUBLISHED UINT32_C(2)
#define ELITE_READ_OWNED UINT32_C(3)
#define ELITE_RETURNED UINT32_C(4)
#define ELITE_RETAINED UINT32_C(5)
#define ELITE_UNCERTAIN UINT32_C(6)

#define ELITE_BUILDING UINT32_C(0)
#define ELITE_READY UINT32_C(1)
#define ELITE_RETIRE_REQUESTED UINT32_C(2)
#define ELITE_QUIESCING UINT32_C(3)
#define ELITE_QUARANTINED UINT32_C(4)
#define ELITE_SEALED UINT32_C(5)
#define ELITE_PART_NEW UINT64_C(0)
#define ELITE_PART_JOINING UINT64_C(1)
#define ELITE_PART_ACTIVE UINT64_C(2)
#define ELITE_PART_QUIESCENT UINT64_C(3)
#define ELITE_PART_REJECTED UINT64_C(4)
#define ELITE_EMPTY UINT64_C(0)
#define ELITE_RESERVED UINT64_C(1)
#define ELITE_COMMITTED UINT64_C(2)
#define ELITE_CONSUMED UINT64_C(3)
#define ELITE_FAILURE_INTEGRITY UINT64_C(1)
#define ELITE_FAILURE_COUNTER UINT64_C(2)
#define ELITE_FAILURE_PEER UINT64_C(3)
#define ELITE_FAILURE_PROTOCOL UINT64_C(4)
#define ELITE_FAILURE_RESOURCE UINT64_C(5)
#define ELITE_FAILURE_AUTHORITY UINT64_C(6)

/* All following types are PROCESS LOCAL; none is a file-format object. */
typedef struct elite_connection elite_connection;
typedef struct elite_authority elite_authority;
typedef struct elite_object elite_object;
typedef struct elite_result {
    uint32_t status;
    uint32_t outcome;
    int32_t os_error;
    uint32_t reserved;
} elite_result;
typedef struct elite_lease { uint64_t opaque[10]; } elite_lease;
typedef struct elite_write_span { void *data; uint32_t capacity; } elite_write_span;
typedef struct elite_read_span {
    const void *data;
    uint32_t length;
    uint32_t message_type;
    uint64_t message_id;
    uint64_t epoch;
} elite_read_span;
typedef struct elite_endpoint_definition {
    uint8_t endpoint_id[16];
    uint8_t process_incarnation_id[16];
    uint32_t role;
} elite_endpoint_definition;
typedef struct elite_config {
    uint32_t layout_profile;
    uint32_t wait_mode;
    uint32_t payload_checksum_mode;
    uint32_t max_payload_bytes;
    uint64_t capacity;
    uint32_t producer_endpoints;
    uint32_t consumer_endpoints;
    uint64_t creation_utc_ns;
} elite_config;
/* A grant is a trusted, constructed-object control-plane message, not a
 * capability made trustworthy by parsing arbitrary bytes. Do NOT serialize
 * this native structure directly; define your application's control encoding. */
typedef struct elite_grant {
    char name[ELITE_NAME_BYTES];
    uint32_t constructed;
    uint64_t segment_bytes;
    uint32_t header_crc32;
    uint32_t layout_profile;
    uint32_t atomic_abi_id;
    uint32_t endpoint_index;
    uint32_t role;
    uint64_t authority_epoch;
    uint64_t grant_epoch;
    uint8_t session_id[16];
    uint8_t host_instance_id[16];
    uint8_t authority_instance_id[16];
    uint8_t endpoint_id[16];
    uint8_t process_incarnation_id[16];
} elite_grant;
typedef struct elite_cleanup_receipt {
    uint8_t session_id[16];
    uint8_t endpoint_id[16];
    uint8_t process_incarnation_id[16];
    uint32_t endpoint_index;
    uint32_t local_cleanup_complete;
    uint32_t owns_grant_claim; /* Failed duplicate attempts cannot release another holder. */
} elite_cleanup_receipt;

/* Manager operations are serialized by their application owner. Its identity
 * and host identity must never be reused over unresolved reference domains. */
elite_result elite_authority_create(const uint8_t host_id[16],
    const uint8_t authority_id[16], uint64_t max_backing_bytes,
    elite_authority **out);
elite_result elite_authority_destroy(elite_authority **authority);
/** Create one fresh, exclusively named object within the authority budget.
 * On partial OS failure *out can remain non-NULL with a cleanup obligation.
 * config and endpoint definitions must be validated; no existing object is reset. */
elite_result elite_create(elite_authority *authority, const elite_config *config,
    const elite_endpoint_definition *endpoints, elite_object **out);
/** Mark this fully initialized object as the authority current service target.
 * At most one active object is admitted by this authority. */
elite_result elite_object_activate(elite_object *object);
/** Register self or a still-owned child BEFORE exposing this endpoint grant.
 * Do not register a guessed/recycled PID or reap the child with another owner. */
elite_result elite_object_register_process(elite_object *object,
    uint32_t endpoint_index, pid_t owned_child_or_self);
/** Issue the named endpoint grant once. Grant bytes are control metadata,
 * not authentication. Do not expose them to unregistered descendants. */
elite_result elite_object_grant(elite_object *object, uint32_t endpoint_index,
    elite_grant *out);
/** Resolve a holder from its trusted complete cleanup receipt.
 * A duplicate failed attempt with owns_grant_claim=0 cannot acknowledge
 * the original holder. Receipt acknowledgement is separate from detachment. */
elite_result elite_object_ack_cleanup(elite_object *object,
    const elite_cleanup_receipt *receipt);
/* waitpid(WNOHANG) only on registered owned children. Exit resolves all that
 * child's grants in this authority; a stopped child is NOT fenced. */
elite_result elite_authority_reap_child(elite_authority *authority, pid_t child,
    int *terminal_wait_status);
/** Close new enrollment/work admission. Existing authorized calls may finish.
 * This is not proof of quiescence and never revokes a payload pointer. */
elite_result elite_object_retire(elite_object *object, uint64_t failure_reason);
/** Charge old storage to the bounded quarantine budget. No live bytes
 * are reused and no lost token is reconstructed from a phase word. */
elite_result elite_object_quarantine(elite_object *object);
/* Requires every issued grant's cleanup acknowledgment or confirmed fencing.
 * The attachment count is never used as the sole reclamation criterion. */
elite_result elite_object_destroy(elite_object **object);
elite_result elite_object_info(elite_object *object,
    struct elite_immutable_header *out);

/* Connections are single-owner/nonreentrant. Caller serializes operations
 * and shutdown on each handle. Different endpoints execute concurrently.
 * NULL is checked; arbitrary dangling/unmapped C pointers are never safe. */
/** Consume a trusted, already-constructed, one-shot grant.
 * On error *out may hold a cleanup-only or enrolled handle. Preserve and detach
 * it; an error status does not erase a mapping or reference-count obligation. */
elite_result elite_attach(const elite_grant *trusted_live_grant,
    elite_connection **out);
/** Copy cached immutable metadata into an appropriately aligned C object.
 * FFI clients use elite_binding_info instead; never overlay foreign atomics. */
elite_result elite_get_info(elite_connection *connection,
    struct elite_immutable_header *out);
/** Reserve one writable payload block, returning capacity and opaque lease.
 * No payload copy is performed. Failure may report unavailable capacity.
 * End every alias before commit/abort; do not retain a raw span afterward. */
elite_result elite_write_reserve(elite_connection *connection,
    elite_lease *out_lease, elite_write_span *out_span);
/** Publish a completely constructed valid-length payload.
 * PUBLISHED is irrevocable even if supplemental notification status is non-OK.
 * Never resend on status alone. Length/type/ID belong to this lease only. */
elite_result elite_write_commit(elite_connection *connection,
    const elite_lease *lease, uint32_t length, uint32_t message_type,
    uint64_t message_id);
/** Cancel a still-unpublished, exclusively owned write after all views end.
 * Does not undo publication. NCQ returns its own token once or retains it
 * on a terminal condition rather than attempting an unsafe rollback. */
elite_result elite_write_abort(elite_connection *connection,
    const elite_lease *lease);
/** Claim one ready record; returns only its valid read-only prefix.
 * Payload and metadata may be inspected only while this lease is owned.
 * A consumer completing later does not change dequeue linearization order. */
elite_result elite_read_borrow(elite_connection *connection,
    elite_lease *out_lease, elite_read_span *out_span);
/** End the owned read after every alias has ended, enabling safe reuse.
 * No mutable descriptor/payload access is permitted after RETURNED. */
elite_result elite_read_release(elite_connection *connection,
    const elite_lease *lease);
/* Wrappers use these for tracked aliases. Raw span aliases MUST be ended by
 * the caller before commit/abort/release/detach; C cannot revoke pointers. */
/** Pin the current native lease for a wrapper-managed view.
 * Does not discover arbitrary aliases. Paired elite_view_end is required. */
elite_result elite_view_retain(elite_connection *connection, const elite_lease *lease);
/** End one explicitly retained native view BEFORE token transfer.
 * A later use through an untracked raw pointer violates the caller contract. */
elite_result elite_view_end(elite_connection *connection, const elite_lease *lease);
/** SPSC scheduling hint with finite nanosecond budget, not an ownership grant.
 * Always retry borrow. Unsupported for the NCQ profile. */
elite_result elite_wait_data(elite_connection *connection, uint64_t timeout_ns);
/** SPSC scheduling hint with finite nanosecond budget, not a reserved slot.
 * Always retry reserve; timeout does not identify or revoke an owner. */
elite_result elite_wait_space(elite_connection *connection, uint64_t timeout_ns);
/** Request retirement using an existing live connection.
 * Counts, epochs and fences cannot replace all-holder quiescence. */
elite_result elite_retire(elite_connection *connection, uint64_t failure_reason);
/* Explicit old-generation drain only in RETIRE_REQUESTED, never QUIESCING. */
elite_result elite_enable_drain(elite_connection *connection);
/* End all raw aliases first. Retired token is retained in the old generation,
 * NOT returned/reclaimed. Enables honest detach after an integrity failure. */
elite_result elite_abandon_retained(elite_connection *connection,
    const elite_lease *lease);
/** Detach only after all calls, leases and views end. BUSY retains the handle.
 * On success *connection becomes NULL and out_receipt must reach the authority.
 * A cleanup error may retain an obligation; never discard a non-NULL handle. */
elite_result elite_detach(elite_connection **connection,
    elite_cleanup_receipt *out_receipt);

/* Stable-byte parser/CRC helpers; parser never interprets random live atomics. */
uint32_t elite_crc32(const void *bytes, size_t length);
uint64_t elite_crc64(const void *bytes, size_t length);
uint32_t elite_header_crc32(const struct elite_immutable_header *header);
elite_result elite_validate_prefix(const void *bytes, size_t available,
    uint64_t backing_bytes, struct elite_immutable_header *out);
elite_result elite_platform_admit(void);
const char *elite_status_string(uint32_t status);


/** Runtime semantic version of the loaded native library. No allocation. */
const char *elite_version_string(void);
/** Packed release version: major<<16 | minor<<8 | patch. Not the wire ABI. */
uint32_t elite_version_number(void);
/** Non-atomic local ABI introspection. type/member are listed in
 * bindings/python/elite_ringbuffer/_native.py; member UINT32_MAX gives sizeof.
 * Invalid input returns SIZE_MAX. Never reads a mapped object. */
size_t elite_local_layout(uint32_t type, uint32_t member);
/** Copy selected immutable fields into non-overaligned local scalars for FFI. */
elite_result elite_binding_info(elite_connection *connection, elite_object *object,
    uint64_t *capacity, uint32_t *max_payload, uint32_t *profile,
    uint8_t session_id[16]);
#ifdef __cplusplus
}
#endif
#endif
