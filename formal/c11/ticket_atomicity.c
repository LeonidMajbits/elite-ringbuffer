/* Positive control: relaxed strong CAS still provides atomic uniqueness. */
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
static _Atomic uint64_t head;
static unsigned won[2];
static void *claim(void *arg)
{
    unsigned i=*(unsigned *)arg; uint64_t expected=0;
    won[i]=(unsigned)atomic_compare_exchange_strong_explicit(&head,&expected,1,
            memory_order_relaxed,memory_order_relaxed);
    return 0;
}
int main(void)
{
    pthread_t a,b;unsigned i=0,j=1;atomic_init(&head,0);
    if(pthread_create(&a,0,claim,&i)||pthread_create(&b,0,claim,&j))return 2;
    if(pthread_join(a,0)||pthread_join(b,0))return 2;
    assert(won[0]+won[1]==1);return 0;
}
