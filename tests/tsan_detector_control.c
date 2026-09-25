/* Deliberately racy detector activation control. Build/run ONLY in TSan lane.
 * Its required data-race report is not a production alternative implementation. */
#include <pthread.h>
#include <stdio.h>
static int value=0;
static void *racy(void *unused) { (void)unused; for(int i=0;i<10000;++i) ++value; return NULL; }
int main(void) {
    pthread_t a,b;
    if(pthread_create(&a,NULL,racy,NULL)!=0 || pthread_create(&b,NULL,racy,NULL)!=0) return 2;
    if(pthread_join(a,NULL)!=0 || pthread_join(b,NULL)!=0) return 2;
    (void)printf("deliberately raced value=%d\n",value);return 0;
}
