/* NCQ-SC64: original implementation of the frozen project specification.
 * Algorithmic attribution: Ruslan Nikolaev, DISC 2019, Figure 5 (NCQ).
 * Identity-index mapping; 128-byte cells; all queue metadata SC; strong CAS.
 * There is no claim-first ready ticket, FAA allocator, or timeout theft.
 */
#include "elite_internal.h"

elite_result el_ncq_enqueue(elite_connection *c,bool free_queue,uint64_t block)
{
    _Atomic uint64_t *tail=free_queue?&c->ncq->qf_tail.value:&c->ncq->qr_tail.value;
    struct elite_ncq_entry_cell *entries=free_queue?c->qf:c->qr;
    const uint64_t n=c->info.capacity, mask=n-1, limit=c->info.ticket_ceiling;
    for(;;) {
        uint64_t t=atomic_load_explicit(tail,memory_order_seq_cst);
        if(t>=limit) return el_fail(c,ELITE_COUNTER_LIMIT,ELITE_FAILURE_COUNTER);
        uint64_t cycle=t&~mask;
        _Atomic uint64_t *slot=&entries[t&mask].cycle_index;
        uint64_t old=atomic_load_explicit(slot,memory_order_seq_cst);
        uint64_t old_cycle=old&~mask;
        if(old_cycle==cycle) {
            uint64_t expected=t;
            (void)atomic_compare_exchange_strong_explicit(tail,&expected,t+1,
                memory_order_seq_cst,memory_order_seq_cst);
            continue; /* Tail helper: fresh observation set on every attempt. */
        }
        if(cycle<n || old_cycle!=cycle-n) continue;
        EL_HOOK(c,free_queue?ELITE_HOOK_QF_BEFORE_INSTALL:ELITE_HOOK_QR_BEFORE_INSTALL,t,block);
        /* The caller's unique outside token proves this is not a full queue.
         * Counter ceiling was checked BEFORE this irreversible publication. */
        if(!atomic_compare_exchange_strong_explicit(slot,&old,cycle|block,
            memory_order_seq_cst,memory_order_seq_cst)) continue;
        /* ENTRY CAS IS LP. Neither this function nor its caller may now access
         * the transferred mutable descriptor/payload. Hooks use saved values. */
        EL_HOOK(c,free_queue?ELITE_HOOK_QF_AFTER_INSTALL:ELITE_HOOK_QR_AFTER_INSTALL,t,block);
        uint64_t expected=t;
        (void)atomic_compare_exchange_strong_explicit(tail,&expected,t+1,
            memory_order_seq_cst,memory_order_seq_cst);
        return el_result(ELITE_OK,free_queue?ELITE_RETURNED:ELITE_PUBLISHED,0);
    }
}

elite_result el_ncq_dequeue(elite_connection *c,bool free_queue,uint64_t *block)
{
    _Atomic uint64_t *head=free_queue?&c->ncq->qf_head.value:&c->ncq->qr_head.value;
    struct elite_ncq_entry_cell *entries=free_queue?c->qf:c->qr;
    const uint64_t n=c->info.capacity,mask=n-1,limit=c->info.ticket_ceiling;
    for(;;) {
        uint64_t h=atomic_load_explicit(head,memory_order_seq_cst);
        if(h>limit) return el_fail(c,ELITE_INTEGRITY,ELITE_FAILURE_PROTOCOL);
        uint64_t cycle=h&~mask;
        uint64_t entry=atomic_load_explicit(&entries[h&mask].cycle_index,memory_order_seq_cst);
        uint64_t entry_cycle=entry&~mask;
        if(entry_cycle==cycle) {
            if(h>=limit) return el_fail(c,ELITE_COUNTER_LIMIT,ELITE_FAILURE_COUNTER);
            EL_HOOK(c,ELITE_HOOK_HEAD_OBSERVED,h,entry&mask);
            uint64_t expected=h;
            if(!atomic_compare_exchange_strong_explicit(head,&expected,h+1,
                memory_order_seq_cst,memory_order_seq_cst)) continue;
            *block=entry&mask; /* Only this winner owns the captured block. */
            EL_HOOK(c,free_queue?ELITE_HOOK_QF_CLAIM:ELITE_HOOK_QR_CLAIM,h,*block);
            return el_result(ELITE_OK,ELITE_NONE,0);
        }
        if(cycle>=n && entry_cycle==cycle-n)
            return el_result(free_queue?ELITE_NO_CAPACITY_OBSERVED:ELITE_NO_DATA_OBSERVED,ELITE_NONE,0);
        /* Future cycle: saved head stale. NOT an empty result. */
    }
}

elite_result el_ncq_reserve(elite_connection *c,elite_lease *l,elite_write_span *span)
{
    uint64_t b=0;
    elite_result r=el_ncq_dequeue(c,true,&b);
    if(r.status!=ELITE_OK) return r;
    el_begin_token(c,b,0,ELITE_OWN_WRITE); *l=c->lease;
    struct elite_slot_descriptor *d=&c->descriptors[b];
    uint64_t status=elite_load_acquire_u64(&d->status_word);
    uint64_t epoch=status>>2;
    if((status&3)!=ELITE_EMPTY || d->epoch!=epoch || epoch>c->info.epoch_ceiling)
        return el_fail(c,ELITE_INTEGRITY,ELITE_FAILURE_INTEGRITY);
    if(epoch>=c->info.epoch_ceiling) return el_fail(c,ELITE_COUNTER_LIMIT,ELITE_FAILURE_COUNTER);
    ++epoch; d->epoch=epoch; el_set_epoch(c,epoch); *l=c->lease;
    elite_store_release_u64(&d->status_word,(epoch<<2)|ELITE_RESERVED);
    EL_HOOK(c,ELITE_HOOK_RESERVED,0,b);
    span->data=el_payload(c,b); span->capacity=c->info.max_payload_bytes;
    return el_result(ELITE_OK,ELITE_WRITE_OWNED,0);
}

elite_result el_ncq_commit(elite_connection *c,uint32_t length,uint32_t type,uint64_t id)
{
    struct elite_slot_descriptor *d=&c->descriptors[c->block];
    if(d->epoch!=c->epoch || elite_load_acquire_u64(&d->status_word)!=((c->epoch<<2)|ELITE_RESERVED))
        return el_fail(c,ELITE_INTEGRITY,ELITE_FAILURE_INTEGRITY);
    d->payload_length=length; d->message_type=type; d->message_id=id;
    d->checksum=c->info.payload_checksum_mode==ELITE_CHECKSUM_CRC64 ?
        elite_crc64(el_payload(c,c->block),length):0;
    EL_HOOK(c,ELITE_HOOK_CHECKSUM_READY,0,c->block);
    elite_store_release_u64(&d->status_word,(c->epoch<<2)|ELITE_COMMITTED);
    EL_HOOK(c,ELITE_HOOK_COMMITTED,0,c->block);
    elite_result r=el_ncq_enqueue(c,false,c->block);
    if(r.outcome==ELITE_PUBLISHED) el_end_token(c);
    return r;
}
elite_result el_ncq_borrow(elite_connection *c,elite_lease *l,elite_read_span *span)
{
    uint64_t b=0;
    elite_result r=el_ncq_dequeue(c,false,&b);
    if(r.status!=ELITE_OK) return r;
    el_begin_token(c,b,0,ELITE_OWN_READ); *l=c->lease;
    return el_read_metadata(c,l,span);
}
static elite_result el_ncq_return(elite_connection *c,uint64_t expected_phase)
{
    struct elite_slot_descriptor *d=&c->descriptors[c->block];
    if(d->epoch!=c->epoch || elite_load_acquire_u64(&d->status_word)!=((c->epoch<<2)|expected_phase))
        return el_fail(c,ELITE_INTEGRITY,ELITE_FAILURE_INTEGRITY);
    elite_store_release_u64(&d->status_word,c->epoch<<2);
    elite_result r=el_ncq_enqueue(c,true,c->block);
    if(r.outcome==ELITE_RETURNED) el_end_token(c);
    return r;
}
elite_result el_ncq_release(elite_connection *c) { return el_ncq_return(c,ELITE_CONSUMED); }
elite_result el_ncq_abort(elite_connection *c) { return el_ncq_return(c,ELITE_RESERVED); }
