/* External C11/RC11 checker input, not the production implementation.
 * Ordinary payload; no checker annotation supplies missing synchronization.
 * MUT_PUB=1 removes publication release; MUT_REC=1 removes return release.
 * N=2 is retained. Conditional try-paths avoid unbounded spin unrolling.
 */
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#ifndef MUT_PUB
#define MUT_PUB 0
#endif
#ifndef MUT_REC
#define MUT_REC 0
#endif
static _Atomic uint64_t published, reclaimed;
static uint64_t payload[2][2];
static void *producer(void *unused)
{
    (void)unused;
    payload[0][0]=11; payload[0][1]=11;
    atomic_store_explicit(&published,1,MUT_PUB?memory_order_relaxed:memory_order_release);
    payload[1][0]=22; payload[1][1]=22;
    atomic_store_explicit(&published,2,MUT_PUB?memory_order_relaxed:memory_order_release);
    if(atomic_load_explicit(&reclaimed,memory_order_acquire)>=1){
        payload[0][0]=33; payload[0][1]=33;
        atomic_store_explicit(&published,3,MUT_PUB?memory_order_relaxed:memory_order_release);
    }
    return 0;
}
static void *consumer(void *unused)
{
    (void)unused;
    if(atomic_load_explicit(&published,memory_order_acquire)>=1){
        assert(payload[0][0]==11); assert(payload[0][1]==11);
        atomic_store_explicit(&reclaimed,1,MUT_REC?memory_order_relaxed:memory_order_release);
        if(atomic_load_explicit(&published,memory_order_acquire)>=2){
            assert(payload[1][0]==22); assert(payload[1][1]==22);
            atomic_store_explicit(&reclaimed,2,MUT_REC?memory_order_relaxed:memory_order_release);
        }
    }
    return 0;
}
int main(void)
{
    pthread_t p,c;
    atomic_init(&published,0);atomic_init(&reclaimed,0);
    if(pthread_create(&p,0,producer,0)||pthread_create(&c,0,consumer,0))return 2;
    if(pthread_join(p,0)||pthread_join(c,0))return 2;
    return 0;
}
