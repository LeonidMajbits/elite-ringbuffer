/* Only this OS scheduling adapter uses Linux/Darwin extensions to POSIX.
 * No wait-word operation occurs in POLL_ONLY. */
#include "elite_internal.h"
#include <errno.h>
#include <time.h>
#include <unistd.h>
#if defined(__linux__)
#include <linux/futex.h>
#include <sys/syscall.h>
#elif defined(__APPLE__)
#include <mach/mach_time.h>
#include <os/os_sync_wait_on_address.h>
#endif

static int now_ns(uint64_t *out)
{
#if defined(__APPLE__)
    mach_timebase_info_data_t tb;
    if(mach_timebase_info(&tb)!=KERN_SUCCESS || tb.numer==0 || tb.denom==0) return EINVAL;
    uint64_t ticks=mach_absolute_time(),q=ticks/tb.denom,r=ticks%tb.denom;
    uint64_t low=(r*tb.numer)/tb.denom; /* two uint32 factors fit in U64 */
    if(q>(UINT64_MAX-low)/tb.numer) return ERANGE;
    *out=q*tb.numer+low; return 0;
#else
    struct timespec ts;
    if(clock_gettime(CLOCK_MONOTONIC,&ts)<0) return errno;
    if(ts.tv_sec<0 || (uint64_t)ts.tv_sec>(UINT64_MAX-(uint64_t)ts.tv_nsec)/UINT64_C(1000000000)) return ERANGE;
    *out=(uint64_t)ts.tv_sec*UINT64_C(1000000000)+(uint64_t)ts.tv_nsec; return 0;
#endif
}
int el_notify(_Atomic uint32_t *word)
{
    uint32_t old=atomic_exchange_explicit(word,0,memory_order_acq_rel);
    if(old!=1) return old==0?0:EINVAL;
#if defined(__linux__)
    long r=syscall(SYS_futex,(void *)word,FUTEX_WAKE,1,NULL,NULL,0);
    return r<0?errno:0;
#elif defined(__APPLE__)
    int r=os_sync_wake_by_address_any((void *)word,4,OS_SYNC_WAKE_BY_ADDRESS_SHARED);
    return r<0 && errno!=ENOENT?errno:0;
#else
    return ENOTSUP;
#endif
}
int el_notify_connection(elite_connection *c,bool data)
{
    if(c->info.wait_mode!=ELITE_PARKABLE_SPSC) return 0;
    _Atomic uint32_t *w=data?&c->spsc->data_wait.value:&c->spsc->space_wait.value;
#ifdef ELITE_TESTING
    /* Hook exactly after RMW, before wake, while preserving same-value RMWs. */
    uint32_t old=atomic_exchange_explicit(w,0,memory_order_acq_rel);
    EL_HOOK(c,ELITE_HOOK_NOTIFY_CHANGED,0,0);
    if(old!=1) return old==0?0:EINVAL;
#if defined(__linux__)
    return syscall(SYS_futex,(void *)w,FUTEX_WAKE,1,NULL,NULL,0)<0?errno:0;
#elif defined(__APPLE__)
    int r=os_sync_wake_by_address_any((void *)w,4,OS_SYNC_WAKE_BY_ADDRESS_SHARED);
    return r<0&&errno!=ENOENT?errno:0;
#else
    return ENOTSUP;
#endif
#else
    return el_notify(w);
#endif
}
static int wait_os(_Atomic uint32_t *w,uint64_t ns)
{
#if defined(__linux__)
    struct timespec t={(time_t)(ns/UINT64_C(1000000000)),(long)(ns%UINT64_C(1000000000))};
    long r=syscall(SYS_futex,(void *)w,FUTEX_WAIT,1,&t,NULL,0);
    return r<0?errno:0;
#elif defined(__APPLE__)
    int r=os_sync_wait_on_address_with_timeout((void *)w,1,4,OS_SYNC_WAIT_ON_ADDRESS_SHARED,
        OS_CLOCK_MACH_ABSOLUTE_TIME,ns);
    return r<0?errno:0;
#else
    (void)w; (void)ns; return ENOTSUP;
#endif
}
static elite_result predicate(elite_connection *c,bool data)
{
    elite_result r=el_ready(c,false); if(r.status!=ELITE_OK) return r;
    uint64_t peer=elite_load_acquire_u64(data?&c->spsc->published.value:&c->spsc->reclaimed.value);
    c->cached_peer=peer;
    if(peer>c->info.ticket_ceiling || (data?(peer<c->cursor || peer-c->cursor>c->info.capacity):(peer>c->cursor)))
        return el_fail(c,ELITE_INTEGRITY,ELITE_FAILURE_PROTOCOL);
    bool ready=data?peer>c->cursor:c->cursor-peer<c->info.capacity;
    return el_result(ready?ELITE_OK:(data?ELITE_NO_DATA_OBSERVED:ELITE_NO_CAPACITY_OBSERVED),ELITE_NONE,0);
}
elite_result el_wait(elite_connection *c,uint64_t timeout_ns,bool data)
{
    elite_result r=el_enter(c,data?ELITE_CONSUMER:ELITE_PRODUCER);
    if(r.status!=ELITE_OK) return r;
    if(c->spsc==NULL || c->info.wait_mode!=ELITE_PARKABLE_SPSC)
        return el_end(c,el_result(ELITE_UNSUPPORTED,ELITE_NONE,0));
    if(c->owned!=0) return el_end(c,el_result(ELITE_BUSY,ELITE_RETAINED,0));
    r=predicate(c,data);
    if(timeout_ns==0 || (r.status!=ELITE_NO_DATA_OBSERVED && r.status!=ELITE_NO_CAPACITY_OBSERVED)) return el_end(c,r);
    uint64_t now=0; int e=now_ns(&now);
    if(e || timeout_ns>UINT64_MAX-now) return el_end(c,el_result(ELITE_INVALID_ARGUMENT,ELITE_NONE,e?e:ERANGE));
    const uint64_t deadline=now+timeout_ns;
    _Atomic uint32_t *w=data?&c->spsc->data_wait.value:&c->spsc->space_wait.value;
    for(;;) {
        e=now_ns(&now);
        if(e) return el_end(c,el_result(ELITE_OS_ERROR,ELITE_NONE,e));
        if(now>=deadline) return el_end(c,predicate(c,data));
        uint32_t prior=atomic_exchange_explicit(w,1,memory_order_acq_rel);
        if(prior>1) { (void)atomic_exchange_explicit(w,0,memory_order_acq_rel); return el_end(c,el_fail(c,ELITE_INTEGRITY,ELITE_FAILURE_PROTOCOL)); }
        r=predicate(c,data);
        if(r.status!=ELITE_NO_DATA_OBSERVED && r.status!=ELITE_NO_CAPACITY_OBSERVED) {
            (void)atomic_exchange_explicit(w,0,memory_order_acq_rel); return el_end(c,r);
        }
        e=now_ns(&now);
        if(e || now>=deadline) {
            (void)atomic_exchange_explicit(w,0,memory_order_acq_rel);
            return el_end(c,e?el_result(ELITE_OS_ERROR,ELITE_NONE,e):predicate(c,data));
        }
        uint64_t slice=deadline-now;
        if(slice>ELITE_MAX_WAIT_SLICE_NS) slice=ELITE_MAX_WAIT_SLICE_NS;
        e=wait_os(w,slice); /* Positive duration; no zero Darwin timeout. */
        (void)atomic_exchange_explicit(w,0,memory_order_acq_rel);
        r=predicate(c,data);
        if(r.status!=ELITE_NO_DATA_OBSERVED && r.status!=ELITE_NO_CAPACITY_OBSERVED) return el_end(c,r);
        if(e!=0 && e!=EAGAIN && e!=EINTR && e!=ETIMEDOUT) return el_end(c,el_result(ELITE_OS_ERROR,ELITE_NONE,e));
    }
}
