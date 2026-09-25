#include "elite_internal.h"
#include <errno.h>
#include <stdlib.h>

void *el_alloc_aligned(size_t bytes)
{
    void *p = NULL;
    int e = posix_memalign(&p, ELITE_ISOLATION, bytes);
    if (e != 0) { errno = e; return NULL; }
    memset(p, 0, bytes);
    return p;
}

bool el_zero(const void *bytes, size_t n)
{
    const unsigned char *p = bytes;
    for (size_t i = 0; i < n; ++i) if (p[i] != 0) return false;
    return true;
}

elite_result el_enter(elite_connection *c, uint32_t role)
{
    if (c == NULL || c->magic != ELITE_CONN_MAGIC || c->detached != 0)
        return el_result(ELITE_INVALID_ARGUMENT, ELITE_NONE, 0);
    if (c->in_call != 0) return el_result(ELITE_BUSY, ELITE_NONE, 0);
    if (role != 0 && c->role != role) return el_result(ELITE_INVALID_ARGUMENT, ELITE_NONE, 0);
    c->in_call = 1;
    return el_result(ELITE_OK, ELITE_NONE, 0);
}

elite_result el_ready(elite_connection *c, bool read_drain)
{
    if (c->poisoned) return el_result(ELITE_RETIRED, c->owned ? ELITE_RETAINED : ELITE_NONE, 0);
    uint64_t gate = elite_load_acquire_u64(c->gate);
    if (!el_gate_valid(gate, c->info.endpoint_count))
        return el_fail(c, ELITE_INTEGRITY, ELITE_FAILURE_PROTOCOL);
    if (el_state(gate) != ELITE_READY &&
        !(read_drain && c->drain != 0 && el_state(gate) == ELITE_RETIRE_REQUESTED))
        return el_result(ELITE_RETIRED, c->owned ? ELITE_RETAINED : ELITE_NONE, 0);
    if (el_state(gate) == ELITE_RETIRE_REQUESTED) {
        uint64_t f = elite_load_acquire_u64(c->failure);
        if (f != 0 && f != ELITE_FAILURE_PEER && f != ELITE_FAILURE_AUTHORITY)
            return el_result(ELITE_RETIRED, c->owned ? ELITE_RETAINED : ELITE_NONE, 0);
    }
    return el_result(ELITE_OK, ELITE_NONE, 0);
}

elite_result el_retire_raw(_Atomic uint64_t *gate, _Atomic uint64_t *failure,
    uint32_t k, uint64_t reason)
{
    if (reason > ELITE_FAILURE_AUTHORITY) return el_result(ELITE_INVALID_ARGUMENT, ELITE_NONE, 0);
    if (reason != 0) {
        uint64_t expected = 0;
        (void)atomic_compare_exchange_strong_explicit(failure, &expected, reason,
            memory_order_seq_cst, memory_order_seq_cst);
    }
    for (;;) {
        uint64_t old = atomic_load_explicit(gate, memory_order_seq_cst);
        if (!el_gate_valid(old, k)) return el_result(ELITE_INTEGRITY, ELITE_NONE, 0);
        if (el_state(old) >= ELITE_RETIRE_REQUESTED) return el_result(ELITE_OK, ELITE_NONE, 0);
        if (el_state(old) != ELITE_READY) return el_result(ELITE_NOT_READY, ELITE_NONE, 0);
        uint64_t desired = (old & UINT64_C(0xffffffff00000000)) | ELITE_RETIRE_REQUESTED;
        if (atomic_compare_exchange_strong_explicit(gate, &old, desired,
            memory_order_seq_cst, memory_order_seq_cst)) return el_result(ELITE_OK, ELITE_NONE, 0);
    }
}

elite_result el_fail(elite_connection *c, uint32_t status, uint64_t reason)
{
    c->poisoned = 1;
    (void)el_retire_raw(c->gate, c->failure, c->info.endpoint_count, reason);
    if (c->info.wait_mode == ELITE_PARKABLE_SPSC) {
        (void)el_notify_connection(c, true);
        (void)el_notify_connection(c, false);
    }
    return el_result(status, c->owned ? ELITE_RETAINED : ELITE_NONE, 0);
}

bool el_return_allowed(elite_connection *c)
{
    uint64_t gate = elite_load_acquire_u64(c->gate);
    uint64_t f = elite_load_acquire_u64(c->failure);
    return !c->poisoned && el_gate_valid(gate, c->info.endpoint_count) &&
        el_state(gate) >= ELITE_READY && el_state(gate) <= ELITE_QUIESCING &&
        (f == 0 || f == ELITE_FAILURE_PEER || f == ELITE_FAILURE_AUTHORITY);
}

void el_begin_token(elite_connection *c, uint64_t block, uint64_t epoch, uint32_t kind)
{
    ++c->serial; /* Guarded before taking ownership. */
    memset(&c->lease, 0, sizeof(c->lease));
    memcpy(&c->lease.opaque[0], c->info.session_id, 16);
    memcpy(&c->lease.opaque[2], c->receipt.endpoint_id, 16);
    c->lease.opaque[4] = c->serial;
    c->lease.opaque[5] = epoch;
    c->lease.opaque[6] = block;
    c->lease.opaque[7] = kind;
    memcpy(&c->lease.opaque[8], c->info.authority_instance_id, 16);
    c->block = block; c->epoch = epoch; c->owned = kind;
}
void el_set_epoch(elite_connection *c, uint64_t epoch)
{ c->epoch = epoch; c->lease.opaque[5] = epoch; }
void el_end_token(elite_connection *c)
{
    /* LOCAL writes only: the block may already belong to another endpoint. */
    c->owned = ELITE_OWN_NONE; c->views = 0;
    memset(&c->lease, 0, sizeof(c->lease));
}

elite_result el_read_metadata(elite_connection *c, elite_lease *l, elite_read_span *span)
{
    struct elite_slot_descriptor *d = &c->descriptors[c->block];
    uint64_t epoch = d->epoch; /* Ownership precedes this ordinary read. */
    el_set_epoch(c, epoch);
    *l = c->lease;
    if (epoch == 0 || epoch > c->info.epoch_ceiling || d->payload_length > c->info.max_payload_bytes)
        return el_fail(c, ELITE_INTEGRITY, ELITE_FAILURE_INTEGRITY);
    if (c->info.layout_profile == ELITE_NCQ) {
        uint64_t s = elite_load_acquire_u64(&d->status_word);
        if (s != ((epoch << 2) | ELITE_COMMITTED))
            return el_fail(c, ELITE_INTEGRITY, ELITE_FAILURE_INTEGRITY);
    }
    if ((c->info.payload_checksum_mode == ELITE_CHECKSUM_NONE && d->checksum != 0) ||
        (c->info.payload_checksum_mode == ELITE_CHECKSUM_CRC64 &&
         d->checksum != elite_crc64(el_payload(c, c->block), d->payload_length)))
        return el_fail(c, ELITE_INTEGRITY, ELITE_FAILURE_INTEGRITY);
    if (c->info.layout_profile == ELITE_NCQ)
        elite_store_release_u64(&d->status_word, (epoch << 2) | ELITE_CONSUMED);
    span->data = el_payload(c, c->block);
    span->length = d->payload_length; span->message_type = d->message_type;
    span->message_id = d->message_id; span->epoch = epoch;
    return el_result(ELITE_OK, ELITE_READ_OWNED, 0);
}

elite_result elite_write_reserve(elite_connection *c, elite_lease *l, elite_write_span *s)
{
    if (l == NULL || s == NULL) return el_result(ELITE_INVALID_ARGUMENT, ELITE_NONE, 0);
    memset(l, 0, sizeof(*l)); s->data = NULL; s->capacity = 0;
    elite_result r = el_enter(c, ELITE_PRODUCER);
    if (r.status != ELITE_OK) return r;
    if (c->owned != ELITE_OWN_NONE) return el_end(c, el_result(ELITE_BUSY, ELITE_RETAINED, 0));
    r = el_ready(c, false);
    if (r.status != ELITE_OK) return el_end(c, r);
    if (c->serial == UINT64_MAX) return el_end(c, el_fail(c, ELITE_COUNTER_LIMIT, ELITE_FAILURE_COUNTER));
    r = c->spsc != NULL ? el_spsc_reserve(c,l,s) : el_ncq_reserve(c,l,s);
    return el_end(c,r);
}

elite_result elite_read_borrow(elite_connection *c, elite_lease *l, elite_read_span *s)
{
    if (l == NULL || s == NULL) return el_result(ELITE_INVALID_ARGUMENT, ELITE_NONE, 0);
    memset(l, 0, sizeof(*l)); memset(s, 0, sizeof(*s));
    elite_result r = el_enter(c, ELITE_CONSUMER);
    if (r.status != ELITE_OK) return r;
    if (c->owned != ELITE_OWN_NONE) return el_end(c, el_result(ELITE_BUSY, ELITE_RETAINED, 0));
    r = el_ready(c, true);
    if (r.status != ELITE_OK) return el_end(c,r);
    if (c->serial == UINT64_MAX) return el_end(c, el_fail(c, ELITE_COUNTER_LIMIT, ELITE_FAILURE_COUNTER));
    r = c->spsc != NULL ? el_spsc_borrow(c,l,s) : el_ncq_borrow(c,l,s);
    return el_end(c,r);
}

elite_result elite_write_commit(elite_connection *c, const elite_lease *l,
    uint32_t length, uint32_t type, uint64_t id)
{
    elite_result r = el_enter(c, ELITE_PRODUCER);
    if (r.status != ELITE_OK) return r;
    if (!el_lease_matches(c,l) || c->owned != ELITE_OWN_WRITE)
        return el_end(c, el_result(ELITE_INVALID_LEASE, ELITE_NONE, 0));
    if (c->views != 0) return el_end(c, el_result(ELITE_BUSY, ELITE_RETAINED, 0));
    if (length > c->info.max_payload_bytes)
        return el_end(c, el_result(ELITE_INVALID_ARGUMENT, ELITE_RETAINED, 0));
    if (c->poisoned) return el_end(c, el_result(ELITE_RETIRED, ELITE_RETAINED, 0));
    r = el_ready(c,false);
    if (r.status != ELITE_OK) return el_end(c,r);
    r = c->spsc != NULL ? el_spsc_commit(c,length,type,id) : el_ncq_commit(c,length,type,id);
    return el_end(c,r);
}

static elite_result el_release_public(elite_connection *c, const elite_lease *l, bool abort_write)
{
    elite_result r = el_enter(c, abort_write ? ELITE_PRODUCER : ELITE_CONSUMER);
    if (r.status != ELITE_OK) return r;
    if (!el_lease_matches(c,l) || c->owned != (abort_write ? ELITE_OWN_WRITE : ELITE_OWN_READ))
        return el_end(c, el_result(ELITE_INVALID_LEASE, ELITE_NONE, 0));
    if (c->views != 0) return el_end(c, el_result(ELITE_BUSY, ELITE_RETAINED, 0));
    if (!el_return_allowed(c)) return el_end(c, el_result(ELITE_RETIRED, ELITE_RETAINED, 0));
    if (abort_write) r = c->spsc != NULL ? el_spsc_abort(c) : el_ncq_abort(c);
    else r = c->spsc != NULL ? el_spsc_release(c) : el_ncq_release(c);
    return el_end(c,r);
}
elite_result elite_write_abort(elite_connection *c, const elite_lease *l)
{ return el_release_public(c,l,true); }
elite_result elite_read_release(elite_connection *c, const elite_lease *l)
{ return el_release_public(c,l,false); }

elite_result elite_view_retain(elite_connection *c, const elite_lease *l)
{
    elite_result r=el_enter(c,0); if(r.status!=ELITE_OK) return r;
    if(!el_lease_matches(c,l) || c->poisoned) return el_end(c,el_result(ELITE_INVALID_LEASE,ELITE_NONE,0));
    if(c->views==UINT32_MAX) return el_end(c,el_result(ELITE_BUSY,ELITE_RETAINED,0));
    ++c->views; return el_end(c,el_result(ELITE_OK,ELITE_RETAINED,0));
}
elite_result elite_view_end(elite_connection *c, const elite_lease *l)
{
    elite_result r=el_enter(c,0); if(r.status!=ELITE_OK) return r;
    if(!el_lease_matches(c,l) || c->views==0) return el_end(c,el_result(ELITE_INVALID_LEASE,ELITE_NONE,0));
    --c->views; return el_end(c,el_result(ELITE_OK,ELITE_RETAINED,0));
}
elite_result elite_retire(elite_connection *c, uint64_t reason)
{
    elite_result r=el_enter(c,0); if(r.status!=ELITE_OK) return r;
    r=el_retire_raw(c->gate,c->failure,c->info.endpoint_count,reason);
    if(r.status==ELITE_OK && c->info.wait_mode==ELITE_PARKABLE_SPSC) {
        int e=el_notify_connection(c,true); int f=el_notify_connection(c,false);
        if(e!=0 || f!=0) r=el_result(ELITE_OS_ERROR,ELITE_NONE,e!=0?e:f);
    }
    return el_end(c,r);
}
elite_result elite_enable_drain(elite_connection *c)
{
    elite_result r=el_enter(c,ELITE_CONSUMER); if(r.status!=ELITE_OK) return r;
    if(c->owned!=0) return el_end(c,el_result(ELITE_BUSY,ELITE_RETAINED,0));
    c->drain=1; return el_end(c,el_result(ELITE_OK,ELITE_NONE,0));
}
elite_result elite_abandon_retained(elite_connection *c, const elite_lease *l)
{
    elite_result r=el_enter(c,0); if(r.status!=ELITE_OK) return r;
    if(!el_lease_matches(c,l)) return el_end(c,el_result(ELITE_INVALID_LEASE,ELITE_NONE,0));
    if(c->views!=0) return el_end(c,el_result(ELITE_BUSY,ELITE_RETAINED,0));
    uint64_t g=elite_load_acquire_u64(c->gate);
    if(!el_gate_valid(g,c->info.endpoint_count) || el_state(g)<ELITE_RETIRE_REQUESTED)
        return el_end(c,el_result(ELITE_BUSY,ELITE_RETAINED,0));
    /* No attempt to return this token. Storage stays in the old generation. */
    el_end_token(c); c->poisoned=1;
    return el_end(c,el_result(ELITE_OK,ELITE_RETAINED,0));
}
elite_result elite_get_info(elite_connection *c, struct elite_immutable_header *out)
{
    if(out==NULL) return el_result(ELITE_INVALID_ARGUMENT,ELITE_NONE,0);
    elite_result r=el_enter(c,0); if(r.status!=ELITE_OK) return r;
    *out=c->info; return el_end(c,el_result(ELITE_OK,ELITE_NONE,0));
}
elite_result elite_wait_data(elite_connection *c,uint64_t ns) { return el_wait(c,ns,true); }
elite_result elite_wait_space(elite_connection *c,uint64_t ns) { return el_wait(c,ns,false); }
const char *elite_status_string(uint32_t s)
{
    static const char *const names[]={"OK","NO_DATA_OBSERVED","NO_CAPACITY_OBSERVED","RETIRED","BUSY","BAD_ABI","BAD_LAYOUT","BAD_IDENTITY","NOT_READY","COUNTER_LIMIT","INTEGRITY","OS_ERROR","OUTCOME_UNCERTAIN","AUTHORITY_REQUIRED","UNSUPPORTED","INVALID_LEASE","INVALID_ARGUMENT","CREATE_CONFLICT"};
    return s<sizeof(names)/sizeof(names[0])?names[s]:"UNKNOWN_STATUS";
}
#ifdef ELITE_TESTING
elite_result elite_test_set_hook(elite_connection *c, elite_test_hook hook, void *arg)
{
    elite_result r=el_enter(c,0); if(r.status!=ELITE_OK) return r;
    c->hook=hook; c->hook_context=arg; return el_end(c,el_result(ELITE_OK,ELITE_NONE,0));
}
#endif
