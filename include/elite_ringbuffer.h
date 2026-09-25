/* ELITEIPC LE128-V1. Copyright (c) 2026 Leonid Majbits.
 * Source implementation of the Turn 4 ABI. See docs/API.md for the managed
 * bootstrap and raw-borrow obligations. This header is intentionally C11.
 * C++ clients include elite_api.h instead; Python uses the ctypes wrapper.
 * Wrappers call this C implementation and never overlay foreign atomics.
 */
#ifndef ELITE_RINGBUFFER_H
#define ELITE_RINGBUFFER_H
#ifdef __cplusplus
/* C++ sees only opaque API types, never foreign shared atomics. */
#include "elite_api.h"
#else
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <stdalign.h>
#include <stdatomic.h>
#include <sys/types.h>

#include "elite_api.h"

struct elite_cell32 {
    alignas(128) _Atomic uint32_t value;
    uint8_t reserved[124];
};
struct elite_cell64 {
    alignas(128) _Atomic uint64_t value;
    uint8_t reserved[120];
};
struct elite_immutable_header {
    alignas(128) uint8_t magic[8]; /* 0 */
    uint32_t abi_version; /* 8 */
    uint32_t header_bytes; /* 12 */
    uint32_t immutable_bytes; /* 16 */
    uint32_t format_flags; /* 20 */
    uint32_t layout_profile; /* 24 */
    uint32_t endian_tag; /* 28 */
    uint32_t isolation_bytes; /* 32 */
    uint32_t mapping_quantum; /* 36 */
    uint32_t atomic_abi_id; /* 40 */
    uint32_t wait_mode; /* 44 */
    uint32_t payload_checksum_mode; /* 48 */
    uint32_t header_crc32; /* 52 */
    uint64_t segment_bytes; /* 56 */
    uint64_t capacity; /* 64 */
    uint32_t max_payload_bytes; /* 72 */
    uint32_t endpoint_count; /* 76 */
    uint32_t descriptor_stride; /* 80 */
    uint32_t queue_entry_stride; /* 84 */
    uint64_t payload_stride; /* 88 */
    uint64_t participants_offset; /* 96 */
    uint32_t participant_stride; /* 104 */
    uint32_t lifecycle_profile; /* 108 */
    uint64_t descriptors_offset; /* 112 */
    uint64_t payloads_offset; /* 120 */
    uint64_t qf_entries_offset; /* 128 */
    uint64_t qr_entries_offset; /* 136 */
    uint64_t ticket_ceiling; /* 144 */
    uint64_t epoch_ceiling; /* 152 */
    uint8_t session_id[16]; /* 160 */
    uint8_t host_instance_id[16]; /* 176 */
    uint8_t authority_instance_id[16]; /* 192 */
    uint64_t authority_epoch; /* 208 */
    uint8_t predecessor_session_id[16]; /* 216 */
    uint64_t creation_utc_ns; /* 232 */
    uint64_t max_wait_slice_ns; /* 240 */
    uint64_t max_backing_bytes; /* 248 */
    uint32_t producer_endpoints; /* 256 */
    uint32_t consumer_endpoints; /* 260 */
    uint32_t max_backing_objects; /* 264 */
    uint32_t max_quarantined_objects; /* 268 */
    uint8_t reserved_110[240]; /* 272 */
};
struct elite_spsc_ring_header {
    struct elite_immutable_header immutable;
    struct elite_cell64 admission;
    struct elite_cell64 failure;
    struct elite_cell64 published;
    struct elite_cell64 reclaimed;
    struct elite_cell32 data_wait;
    struct elite_cell32 space_wait;
    uint8_t reserved_500[768];
};
struct elite_mpmc_ncq_header {
    struct elite_immutable_header immutable;
    struct elite_cell64 admission;
    struct elite_cell64 failure;
    struct elite_cell64 qf_head;
    struct elite_cell64 qf_tail;
    struct elite_cell64 qr_head;
    struct elite_cell64 qr_tail;
    struct elite_cell32 reserved_data_wait;
    struct elite_cell32 reserved_space_wait;
    uint8_t reserved_600[512];
};
struct elite_slot_descriptor {
    alignas(128) uint64_t epoch;
    _Atomic uint64_t status_word;
    uint32_t payload_length;
    uint32_t message_type;
    uint64_t checksum;
    uint64_t message_id;
    uint8_t reserved_028[88];
};
struct elite_ncq_entry_cell {
    alignas(128) _Atomic uint64_t cycle_index;
    uint8_t reserved[120];
};
struct elite_participant_record {
    alignas(128) uint8_t endpoint_id[16];
    uint8_t process_incarnation_id[16];
    uint32_t endpoint_index;
    uint32_t role;
    uint32_t max_outstanding_tokens;
    uint32_t flags;
    uint64_t grant_epoch;
    uint8_t reserved_038[72];
    struct elite_cell64 attachment;
};

#define ELITE_OFFSET(t,m,o) _Static_assert(offsetof(struct t,m)==(o), #t "." #m)
#define ELITE_SIZE_ALIGN(t,s,a) \
    _Static_assert(sizeof(struct t)==(s), #t " size"); \
    _Static_assert(alignof(struct t)==(a), #t " alignment")
_Static_assert(CHAR_BIT == 8, "eight-bit bytes required");
_Static_assert(sizeof(uint32_t)==4 && alignof(uint32_t)==4, "U32 ABI");
_Static_assert(sizeof(uint64_t)==8 && alignof(uint64_t)==8, "U64 ABI");
_Static_assert(sizeof(_Atomic uint32_t)==4 && alignof(_Atomic uint32_t)==4, "AU32 ABI");
_Static_assert(sizeof(_Atomic uint64_t)==8 && alignof(_Atomic uint64_t)==8, "AU64 ABI");
ELITE_SIZE_ALIGN(elite_cell32,128,128);
ELITE_SIZE_ALIGN(elite_cell64,128,128);
ELITE_SIZE_ALIGN(elite_immutable_header,512,128);
ELITE_SIZE_ALIGN(elite_spsc_ring_header,2048,128);
ELITE_SIZE_ALIGN(elite_mpmc_ncq_header,2048,128);
ELITE_SIZE_ALIGN(elite_slot_descriptor,128,128);
ELITE_SIZE_ALIGN(elite_ncq_entry_cell,128,128);
ELITE_SIZE_ALIGN(elite_participant_record,256,128);
ELITE_OFFSET(elite_cell32,value,0);
ELITE_OFFSET(elite_cell32,reserved,4);
ELITE_OFFSET(elite_cell64,value,0);
ELITE_OFFSET(elite_cell64,reserved,8);
ELITE_OFFSET(elite_immutable_header,magic,0);
ELITE_OFFSET(elite_immutable_header,abi_version,8);
ELITE_OFFSET(elite_immutable_header,header_bytes,12);
ELITE_OFFSET(elite_immutable_header,immutable_bytes,16);
ELITE_OFFSET(elite_immutable_header,format_flags,20);
ELITE_OFFSET(elite_immutable_header,layout_profile,24);
ELITE_OFFSET(elite_immutable_header,endian_tag,28);
ELITE_OFFSET(elite_immutable_header,isolation_bytes,32);
ELITE_OFFSET(elite_immutable_header,mapping_quantum,36);
ELITE_OFFSET(elite_immutable_header,atomic_abi_id,40);
ELITE_OFFSET(elite_immutable_header,wait_mode,44);
ELITE_OFFSET(elite_immutable_header,payload_checksum_mode,48);
ELITE_OFFSET(elite_immutable_header,header_crc32,52);
ELITE_OFFSET(elite_immutable_header,segment_bytes,56);
ELITE_OFFSET(elite_immutable_header,capacity,64);
ELITE_OFFSET(elite_immutable_header,max_payload_bytes,72);
ELITE_OFFSET(elite_immutable_header,endpoint_count,76);
ELITE_OFFSET(elite_immutable_header,descriptor_stride,80);
ELITE_OFFSET(elite_immutable_header,queue_entry_stride,84);
ELITE_OFFSET(elite_immutable_header,payload_stride,88);
ELITE_OFFSET(elite_immutable_header,participants_offset,96);
ELITE_OFFSET(elite_immutable_header,participant_stride,104);
ELITE_OFFSET(elite_immutable_header,lifecycle_profile,108);
ELITE_OFFSET(elite_immutable_header,descriptors_offset,112);
ELITE_OFFSET(elite_immutable_header,payloads_offset,120);
ELITE_OFFSET(elite_immutable_header,qf_entries_offset,128);
ELITE_OFFSET(elite_immutable_header,qr_entries_offset,136);
ELITE_OFFSET(elite_immutable_header,ticket_ceiling,144);
ELITE_OFFSET(elite_immutable_header,epoch_ceiling,152);
ELITE_OFFSET(elite_immutable_header,session_id,160);
ELITE_OFFSET(elite_immutable_header,host_instance_id,176);
ELITE_OFFSET(elite_immutable_header,authority_instance_id,192);
ELITE_OFFSET(elite_immutable_header,authority_epoch,208);
ELITE_OFFSET(elite_immutable_header,predecessor_session_id,216);
ELITE_OFFSET(elite_immutable_header,creation_utc_ns,232);
ELITE_OFFSET(elite_immutable_header,max_wait_slice_ns,240);
ELITE_OFFSET(elite_immutable_header,max_backing_bytes,248);
ELITE_OFFSET(elite_immutable_header,producer_endpoints,256);
ELITE_OFFSET(elite_immutable_header,consumer_endpoints,260);
ELITE_OFFSET(elite_immutable_header,max_backing_objects,264);
ELITE_OFFSET(elite_immutable_header,max_quarantined_objects,268);
ELITE_OFFSET(elite_immutable_header,reserved_110,272);
ELITE_OFFSET(elite_spsc_ring_header,immutable,0);
ELITE_OFFSET(elite_spsc_ring_header,admission,512);
ELITE_OFFSET(elite_spsc_ring_header,failure,640);
ELITE_OFFSET(elite_spsc_ring_header,published,768);
ELITE_OFFSET(elite_spsc_ring_header,reclaimed,896);
ELITE_OFFSET(elite_spsc_ring_header,data_wait,1024);
ELITE_OFFSET(elite_spsc_ring_header,space_wait,1152);
ELITE_OFFSET(elite_spsc_ring_header,reserved_500,1280);
ELITE_OFFSET(elite_mpmc_ncq_header,immutable,0);
ELITE_OFFSET(elite_mpmc_ncq_header,admission,512);
ELITE_OFFSET(elite_mpmc_ncq_header,failure,640);
ELITE_OFFSET(elite_mpmc_ncq_header,qf_head,768);
ELITE_OFFSET(elite_mpmc_ncq_header,qf_tail,896);
ELITE_OFFSET(elite_mpmc_ncq_header,qr_head,1024);
ELITE_OFFSET(elite_mpmc_ncq_header,qr_tail,1152);
ELITE_OFFSET(elite_mpmc_ncq_header,reserved_data_wait,1280);
ELITE_OFFSET(elite_mpmc_ncq_header,reserved_space_wait,1408);
ELITE_OFFSET(elite_mpmc_ncq_header,reserved_600,1536);
ELITE_OFFSET(elite_slot_descriptor,epoch,0);
ELITE_OFFSET(elite_slot_descriptor,status_word,8);
ELITE_OFFSET(elite_slot_descriptor,payload_length,16);
ELITE_OFFSET(elite_slot_descriptor,message_type,20);
ELITE_OFFSET(elite_slot_descriptor,checksum,24);
ELITE_OFFSET(elite_slot_descriptor,message_id,32);
ELITE_OFFSET(elite_slot_descriptor,reserved_028,40);
ELITE_OFFSET(elite_ncq_entry_cell,cycle_index,0);
ELITE_OFFSET(elite_ncq_entry_cell,reserved,8);
ELITE_OFFSET(elite_participant_record,endpoint_id,0);
ELITE_OFFSET(elite_participant_record,process_incarnation_id,16);
ELITE_OFFSET(elite_participant_record,endpoint_index,32);
ELITE_OFFSET(elite_participant_record,role,36);
ELITE_OFFSET(elite_participant_record,max_outstanding_tokens,40);
ELITE_OFFSET(elite_participant_record,flags,44);
ELITE_OFFSET(elite_participant_record,grant_epoch,48);
ELITE_OFFSET(elite_participant_record,reserved_038,56);
ELITE_OFFSET(elite_participant_record,attachment,128);
#undef ELITE_OFFSET
#undef ELITE_SIZE_ALIGN

/* Use only for live, correctly typed objects in admitted storage. */
static inline uint64_t elite_load_acquire_u64(const _Atomic uint64_t *p)
{ return atomic_load_explicit(p, memory_order_acquire); }
static inline void elite_store_release_u64(_Atomic uint64_t *p, uint64_t v)
{ atomic_store_explicit(p, v, memory_order_release); }

#ifdef ELITE_TESTING
/* Verification-only hooks are absent from release binaries and shared ABI. */
#define ELITE_HOOK_QF_CLAIM 1u
#define ELITE_HOOK_RESERVED 2u
#define ELITE_HOOK_COMMITTED 3u
#define ELITE_HOOK_QR_BEFORE_INSTALL 4u
#define ELITE_HOOK_QR_AFTER_INSTALL 5u
#define ELITE_HOOK_QR_CLAIM 6u
#define ELITE_HOOK_QF_BEFORE_INSTALL 7u
#define ELITE_HOOK_QF_AFTER_INSTALL 8u
#define ELITE_HOOK_HEAD_OBSERVED 9u
#define ELITE_HOOK_CHECKSUM_READY 10u
#define ELITE_HOOK_SPSC_PUBLISHED 11u
#define ELITE_HOOK_NOTIFY_CHANGED 12u
#define ELITE_HOOK_JOINED 13u
#define ELITE_HOOK_DETACH_DECREMENTED 14u
typedef void (*elite_test_hook)(void *, uint32_t, uint64_t, uint64_t);
elite_result elite_test_set_hook(elite_connection *, elite_test_hook, void *);
#endif
#endif /* native C11 layout branch */
#endif /* ELITE_RINGBUFFER_H */
