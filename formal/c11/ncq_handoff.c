/* First QR publication and claim only, matching the ordinary epoch read BEFORE
 * the acquire status read in el_read_metadata. This is a handoff skeleton,
 * not a substitute model of the two complete internal queues.
 */
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#ifndef MUT_HANDOFF
#define MUT_HANDOFF 0
#endif
static _Atomic uint64_t entry,head,status;
static uint64_t epoch,payload;
static void *producer(void *unused)
{
    uint64_t old=0;(void)unused;
    epoch=1;payload=7;
    atomic_store_explicit(&status,6,memory_order_release);
    (void)atomic_compare_exchange_strong_explicit(&entry,&old,2,
        MUT_HANDOFF?memory_order_relaxed:memory_order_seq_cst,
        MUT_HANDOFF?memory_order_relaxed:memory_order_seq_cst);
    return 0;
}
static void *consumer(void *unused)
{
    uint64_t expected=2;(void)unused;
    if(atomic_load_explicit(&entry,memory_order_seq_cst)==2 &&
       atomic_compare_exchange_strong_explicit(&head,&expected,3,memory_order_seq_cst,memory_order_seq_cst)){
        uint64_t observed_epoch=epoch;
        uint64_t observed_status=atomic_load_explicit(&status,memory_order_acquire);
        assert(observed_epoch==1);assert(observed_status==6);assert(payload==7);
    }
    return 0;
}
int main(void)
{
    pthread_t p,c;
    atomic_init(&entry,0);atomic_init(&head,2);atomic_init(&status,0);
    if(pthread_create(&p,0,producer,0)||pthread_create(&c,0,consumer,0))return 2;
    if(pthread_join(p,0)||pthread_join(c,0))return 2;
    return 0;
}
