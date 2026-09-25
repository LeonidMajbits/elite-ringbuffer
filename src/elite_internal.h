#ifndef ELITE_INTERNAL_H
#define ELITE_INTERNAL_H
#include "elite_ringbuffer.h"
#include <stdbool.h>
#include <string.h>

#define ELITE_CONN_MAGIC UINT64_C(0x454c434f4e4e3031)
#define ELITE_AUTH_MAGIC UINT64_C(0x454c415554483031)
#define ELITE_CONSTRUCTED UINT32_C(0x454c4731)
#define ELITE_OWN_NONE 0u
#define ELITE_OWN_WRITE 1u
#define ELITE_OWN_READ 2u

struct elite_connection {
    struct elite_immutable_header info;
    uint64_t magic;
    void *mapping;
    size_t mapping_bytes;
    int fd;
    struct elite_spsc_ring_header *spsc;
    struct elite_mpmc_ncq_header *ncq;
    _Atomic uint64_t *gate;
    _Atomic uint64_t *failure;
    struct elite_participant_record *participant;
    struct elite_slot_descriptor *descriptors;
    struct elite_ncq_entry_cell *qf;
    struct elite_ncq_entry_cell *qr;
    unsigned char *payloads;
    elite_lease lease;
    uint64_t serial;
    uint64_t cursor;
    uint64_t cached_peer;
    uint64_t block;
    uint64_t epoch;
    uint32_t role;
    uint32_t owned;
    uint32_t views;
    uint32_t in_call;
    uint32_t enrolled;
    uint32_t drain;
    uint32_t poisoned;
    uint32_t detached;
    uint32_t cleanup_uncertain;
    elite_cleanup_receipt receipt;
#ifdef ELITE_TESTING
    elite_test_hook hook;
    void *hook_context;
#endif
};

static inline elite_result el_result(uint32_t s, uint32_t o, int e)
{
    elite_result r = {s, o, (int32_t)e, 0};
    return r;
}
static inline uint32_t el_state(uint64_t gate) { return (uint32_t)(gate & UINT64_C(255)); }
static inline uint32_t el_count(uint64_t gate) { return (uint32_t)(gate >> 32); }
static inline bool el_gate_valid(uint64_t gate, uint32_t k)
{
    return (gate & UINT64_C(0x00000000ffffff00)) == 0 &&
        el_state(gate) <= ELITE_SEALED && el_count(gate) <= k;
}
static inline elite_result el_end(elite_connection *c, elite_result r)
{ c->in_call = 0; return r; }
static inline unsigned char *el_payload(elite_connection *c, uint64_t b)
{ return c->payloads + (size_t)(b * c->info.payload_stride); }
static inline bool el_lease_matches(const elite_connection *c, const elite_lease *l)
{ return l != NULL && c->owned != ELITE_OWN_NONE && memcmp(l, &c->lease, sizeof(*l)) == 0; }
#ifdef ELITE_TESTING
#define EL_HOOK(c,p,t,b) do { if ((c)->hook != NULL) (c)->hook((c)->hook_context,(p),(t),(b)); } while (0)
#else
#define EL_HOOK(c,p,t,b) ((void)0)
#endif

elite_result el_enter(elite_connection *c, uint32_t role);
elite_result el_ready(elite_connection *c, bool read_drain);
elite_result el_fail(elite_connection *c, uint32_t status, uint64_t reason);
elite_result el_retire_raw(_Atomic uint64_t *gate, _Atomic uint64_t *failure,
    uint32_t k, uint64_t reason);
void el_begin_token(elite_connection *c, uint64_t block, uint64_t epoch, uint32_t kind);
void el_set_epoch(elite_connection *c, uint64_t epoch);
void el_end_token(elite_connection *c);
elite_result el_read_metadata(elite_connection *c, elite_lease *lease, elite_read_span *span);
bool el_return_allowed(elite_connection *c);
int el_notify(_Atomic uint32_t *word);
int el_notify_connection(elite_connection *c, bool data);
elite_result el_wait(elite_connection *c, uint64_t timeout_ns, bool data);

elite_result el_spsc_reserve(elite_connection *, elite_lease *, elite_write_span *);
elite_result el_spsc_commit(elite_connection *, uint32_t, uint32_t, uint64_t);
elite_result el_spsc_borrow(elite_connection *, elite_lease *, elite_read_span *);
elite_result el_spsc_release(elite_connection *);
elite_result el_spsc_abort(elite_connection *);
elite_result el_ncq_reserve(elite_connection *, elite_lease *, elite_write_span *);
elite_result el_ncq_commit(elite_connection *, uint32_t, uint32_t, uint64_t);
elite_result el_ncq_borrow(elite_connection *, elite_lease *, elite_read_span *);
elite_result el_ncq_release(elite_connection *);
elite_result el_ncq_abort(elite_connection *);
/* One unique outside token is a mandatory precondition of every enqueue. */
elite_result el_ncq_enqueue(elite_connection *, bool free_queue, uint64_t block);
elite_result el_ncq_dequeue(elite_connection *, bool free_queue, uint64_t *block);
void *el_alloc_aligned(size_t bytes);
bool el_zero(const void *bytes, size_t n);
elite_result el_geometry(struct elite_immutable_header *h);
#endif
