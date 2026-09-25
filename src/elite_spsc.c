#include "elite_internal.h"

elite_result el_spsc_reserve(elite_connection *c, elite_lease *l, elite_write_span *span)
{
    const uint64_t p=c->cursor, n=c->info.capacity;
    if(p>=c->info.ticket_ceiling) return el_fail(c,ELITE_COUNTER_LIMIT,ELITE_FAILURE_COUNTER);
    uint64_t reclaimed=c->cached_peer;
    if(reclaimed>p) return el_fail(c,ELITE_INTEGRITY,ELITE_FAILURE_PROTOCOL);
    if(p-reclaimed>=n) {
        reclaimed=elite_load_acquire_u64(&c->spsc->reclaimed.value);
        c->cached_peer=reclaimed;
        if(reclaimed>p) return el_fail(c,ELITE_INTEGRITY,ELITE_FAILURE_PROTOCOL);
        if(p-reclaimed>=n) return el_result(ELITE_NO_CAPACITY_OBSERVED,ELITE_NONE,0);
    }
    uint64_t b=p&(n-1);
    struct elite_slot_descriptor *d=&c->descriptors[b];
    if(d->epoch>=c->info.epoch_ceiling) return el_fail(c,ELITE_COUNTER_LIMIT,ELITE_FAILURE_COUNTER);
    ++d->epoch;
    el_begin_token(c,b,d->epoch,ELITE_OWN_WRITE);
    *l=c->lease; span->data=el_payload(c,b); span->capacity=c->info.max_payload_bytes;
    EL_HOOK(c,ELITE_HOOK_RESERVED,p,b);
    return el_result(ELITE_OK,ELITE_WRITE_OWNED,0);
}

elite_result el_spsc_commit(elite_connection *c,uint32_t length,uint32_t type,uint64_t id)
{
    struct elite_slot_descriptor *d=&c->descriptors[c->block];
    if(d->epoch!=c->epoch) return el_fail(c,ELITE_INTEGRITY,ELITE_FAILURE_INTEGRITY);
    d->payload_length=length; d->message_type=type; d->message_id=id;
    d->checksum=c->info.payload_checksum_mode==ELITE_CHECKSUM_CRC64 ?
        elite_crc64(el_payload(c,c->block),length) : 0;
    EL_HOOK(c,ELITE_HOOK_CHECKSUM_READY,c->cursor,c->block);
    const uint64_t next=c->cursor+1; /* Reserve checked its eventual successor. */
    elite_store_release_u64(&c->spsc->published.value,next); /* publication LP */
    EL_HOOK(c,ELITE_HOOK_SPSC_PUBLISHED,c->cursor,c->block);
    /* No descriptor/payload access after LP, including diagnostic cleanup. */
    c->cursor=next; el_end_token(c);
    int e=el_notify_connection(c,true);
    return el_result(e?ELITE_OS_ERROR:ELITE_OK,ELITE_PUBLISHED,e);
}

elite_result el_spsc_borrow(elite_connection *c,elite_lease *l,elite_read_span *span)
{
    uint64_t published=c->cached_peer, r=c->cursor;
    if(published<=r) {
        published=elite_load_acquire_u64(&c->spsc->published.value);
        c->cached_peer=published;
    }
    if(published<r || published>c->info.ticket_ceiling || published-r>c->info.capacity)
        return el_fail(c,ELITE_INTEGRITY,ELITE_FAILURE_PROTOCOL);
    if(published==r) return el_result(ELITE_NO_DATA_OBSERVED,ELITE_NONE,0);
    if(r>=c->info.ticket_ceiling) return el_fail(c,ELITE_COUNTER_LIMIT,ELITE_FAILURE_COUNTER);
    el_begin_token(c,r&(c->info.capacity-1),0,ELITE_OWN_READ);
    return el_read_metadata(c,l,span);
}

elite_result el_spsc_release(elite_connection *c)
{
    if(c->descriptors[c->block].epoch!=c->epoch)
        return el_fail(c,ELITE_INTEGRITY,ELITE_FAILURE_INTEGRITY);
    uint64_t next=c->cursor+1;
    elite_store_release_u64(&c->spsc->reclaimed.value,next); /* reclamation LP */
    c->cursor=next; el_end_token(c);
    int e=el_notify_connection(c,false);
    return el_result(e?ELITE_OS_ERROR:ELITE_OK,ELITE_RETURNED,e);
}
elite_result el_spsc_abort(elite_connection *c)
{
    if(c->descriptors[c->block].epoch!=c->epoch)
        return el_fail(c,ELITE_INTEGRITY,ELITE_FAILURE_INTEGRITY);
    /* Epoch remains consumed. P and dormant status_word stay unchanged. */
    el_end_token(c);
    return el_result(ELITE_OK,ELITE_RETURNED,0);
}
