/* Supplemental process-local FFI introspection; no shared-format changes. */
#include "elite_ringbuffer.h"
#include <string.h>
const char *elite_version_string(void) { return ELITE_VERSION_STRING; }
uint32_t elite_version_number(void) { return UINT32_C(0x00010001); }
size_t elite_local_layout(uint32_t type, uint32_t member)
{
    switch (type) {
    case 1:
        if (member == UINT32_MAX) return sizeof(elite_result);
        switch (member) {
        case 0: return offsetof(elite_result, status);
        case 1: return offsetof(elite_result, outcome);
        case 2: return offsetof(elite_result, os_error);
        case 3: return offsetof(elite_result, reserved);
        default: return SIZE_MAX;
        }
    case 2:
        if (member == UINT32_MAX) return sizeof(elite_lease);
        switch (member) {
        case 0: return offsetof(elite_lease, opaque);
        default: return SIZE_MAX;
        }
    case 3:
        if (member == UINT32_MAX) return sizeof(elite_write_span);
        switch (member) {
        case 0: return offsetof(elite_write_span, data);
        case 1: return offsetof(elite_write_span, capacity);
        default: return SIZE_MAX;
        }
    case 4:
        if (member == UINT32_MAX) return sizeof(elite_read_span);
        switch (member) {
        case 0: return offsetof(elite_read_span, data);
        case 1: return offsetof(elite_read_span, length);
        case 2: return offsetof(elite_read_span, message_type);
        case 3: return offsetof(elite_read_span, message_id);
        case 4: return offsetof(elite_read_span, epoch);
        default: return SIZE_MAX;
        }
    case 5:
        if (member == UINT32_MAX) return sizeof(elite_endpoint_definition);
        switch (member) {
        case 0: return offsetof(elite_endpoint_definition, endpoint_id);
        case 1: return offsetof(elite_endpoint_definition, process_incarnation_id);
        case 2: return offsetof(elite_endpoint_definition, role);
        default: return SIZE_MAX;
        }
    case 6:
        if (member == UINT32_MAX) return sizeof(elite_config);
        switch (member) {
        case 0: return offsetof(elite_config, layout_profile);
        case 1: return offsetof(elite_config, wait_mode);
        case 2: return offsetof(elite_config, payload_checksum_mode);
        case 3: return offsetof(elite_config, max_payload_bytes);
        case 4: return offsetof(elite_config, capacity);
        case 5: return offsetof(elite_config, producer_endpoints);
        case 6: return offsetof(elite_config, consumer_endpoints);
        case 7: return offsetof(elite_config, creation_utc_ns);
        default: return SIZE_MAX;
        }
    case 7:
        if (member == UINT32_MAX) return sizeof(elite_grant);
        switch (member) {
        case 0: return offsetof(elite_grant, name);
        case 1: return offsetof(elite_grant, constructed);
        case 2: return offsetof(elite_grant, segment_bytes);
        case 3: return offsetof(elite_grant, header_crc32);
        case 4: return offsetof(elite_grant, layout_profile);
        case 5: return offsetof(elite_grant, atomic_abi_id);
        case 6: return offsetof(elite_grant, endpoint_index);
        case 7: return offsetof(elite_grant, role);
        case 8: return offsetof(elite_grant, authority_epoch);
        case 9: return offsetof(elite_grant, grant_epoch);
        case 10: return offsetof(elite_grant, session_id);
        case 11: return offsetof(elite_grant, host_instance_id);
        case 12: return offsetof(elite_grant, authority_instance_id);
        case 13: return offsetof(elite_grant, endpoint_id);
        case 14: return offsetof(elite_grant, process_incarnation_id);
        default: return SIZE_MAX;
        }
    case 8:
        if (member == UINT32_MAX) return sizeof(elite_cleanup_receipt);
        switch (member) {
        case 0: return offsetof(elite_cleanup_receipt, session_id);
        case 1: return offsetof(elite_cleanup_receipt, endpoint_id);
        case 2: return offsetof(elite_cleanup_receipt, process_incarnation_id);
        case 3: return offsetof(elite_cleanup_receipt, endpoint_index);
        case 4: return offsetof(elite_cleanup_receipt, local_cleanup_complete);
        case 5: return offsetof(elite_cleanup_receipt, owns_grant_claim);
        default: return SIZE_MAX;
        }
    default: return SIZE_MAX;
    }
}
elite_result elite_binding_info(elite_connection *connection, elite_object *object,
    uint64_t *capacity, uint32_t *max_payload, uint32_t *profile,
    uint8_t session_id[16])
{
    elite_result invalid = {ELITE_INVALID_ARGUMENT, ELITE_NONE, 0, 0};
    if (!capacity || !max_payload || !profile || !session_id ||
        ((connection == NULL) == (object == NULL))) return invalid;
    struct elite_immutable_header h;
    elite_result r = connection ? elite_get_info(connection, &h) : elite_object_info(object, &h);
    if (r.status != ELITE_OK) return r;
    *capacity=h.capacity; *max_payload=h.max_payload_bytes; *profile=h.layout_profile;
    memcpy(session_id,h.session_id,16);
    return r;
}
