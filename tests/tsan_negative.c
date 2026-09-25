/* INTENTIONALLY RACY detector control. NEVER link this into production. */
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
static volatile unsigned raced;
static _Atomic unsigned start;
static void *race(void *arg)
{
    (void)arg;while(atomic_load_explicit(&start,memory_order_acquire)==0){}
    for(unsigned i=0;i<100000;++i)raced=raced+1;return NULL;
}
int main(void)
{
    pthread_t a,b;atomic_init(&start,0);
    if(pthread_create(&a,NULL,race,NULL)||pthread_create(&b,NULL,race,NULL))return 2;
    atomic_store_explicit(&start,1,memory_order_release);
    if(pthread_join(a,NULL)||pthread_join(b,NULL))return 2;
    return raced==0?3:0; /* Under TSan the expected outcome is a race report/66. */
}
