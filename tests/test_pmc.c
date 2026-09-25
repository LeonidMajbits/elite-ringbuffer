#include "elite_pmc.h"
#include "bench_identity.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while (0)
static elite_pmc_group_result fixture(void)
{
    elite_pmc_group_result g; memset(&g,0,sizeof(g));
    const uint64_t b[7]={2,10,10,100,17,200,19};
    const uint64_t a[7]={2,110,110,270,19,150,17};
    memcpy(g.before,b,sizeof(b));memcpy(g.after,a,sizeof(a));
    g.ids[0]=17;g.ids[1]=19;g.before_valid=true;g.after_valid=true;return g;
}
static void *wrong_thread(void *v)
{
    int *bad=((void **)v)[1];elite_pmc_context *c=((void **)v)[0];
    *bad=elite_pmc_start(c);return NULL;
}
int main(void)
{
    elite_pmc_group_result g=fixture();CHECK(elite_pmc_decode_group(&g)==0);
    CHECK(g.status==ELITE_PMC_OK&&g.raw_delta[0]==50&&g.raw_delta[1]==70&&g.time_running_ns==100);
    g=fixture();g.after[2]=60;CHECK(elite_pmc_decode_group(&g)==0&&g.status==ELITE_PMC_MULTIPLEXED);
    g=fixture();g.after[2]=10;g.after[3]=200;g.after[5]=100;
    CHECK(elite_pmc_decode_group(&g)==0&&g.status==ELITE_PMC_NEVER_SCHEDULED);
    for(unsigned i=0;i<8;++i){g=fixture();switch(i){
        case 0:g.before_valid=false;break;case 1:g.after[0]=1;break;
        case 2:g.ids[1]=17;break;case 3:g.after[6]=88;break;
        case 4:g.after[1]=9;break;case 5:g.after[2]=111;break;
        case 6:g.after[5]=99;break;default:g.before[2]=9;g.after[2]=110;break;}
        CHECK(elite_pmc_decode_group(&g)==-1&&g.status==ELITE_PMC_INVALID_READING);}
    CHECK(elite_pmc_decode_group(NULL)==-1);
    CHECK(elite_pmc_classify_open_error(EACCES,3)==ELITE_PMC_RESTRICTED_PARANOID);
    CHECK(elite_pmc_classify_open_error(EPERM,2)==ELITE_PMC_RESTRICTED_ACCESS);
    CHECK(elite_pmc_classify_open_error(EPERM,-999)==ELITE_PMC_RESTRICTED_ACCESS);
    CHECK(elite_pmc_classify_open_error(ENOENT,3)==ELITE_PMC_UNSUPPORTED_EVENT);
    CHECK(elite_pmc_classify_open_error(EMFILE,0)==ELITE_PMC_OPEN_ERROR);
    elite_pmc_context *c=elite_pmc_create(false);CHECK(c!=NULL);
    CHECK(elite_pmc_stop(c)==-1);int result=0;void *args[2]={c,&result};pthread_t t;
    CHECK(pthread_create(&t,NULL,wrong_thread,args)==0&&pthread_join(t,NULL)==0&&result==-1);
    CHECK(elite_pmc_start(c)==0&&elite_pmc_start(c)==-1&&elite_pmc_stop(c)==0&&elite_pmc_stop(c)==-1);
    CHECK(elite_pmc_get_result(c)->groups[0].status==ELITE_PMC_NOT_REQUESTED);
    elite_pmc_destroy(c);elite_pmc_destroy(NULL);
    elite_cpu_snapshot snap;CHECK(elite_cpu_thread_snapshot(&snap)==0&&snap.error==0);
    CHECK(elite_cpu_process_snapshot(&snap)==0);
    char h[65];bench_sha256_bytes("",0,h);CHECK(strcmp(h,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")==0);
    bench_sha256_bytes("abc",3,h);CHECK(strcmp(h,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")==0);
    unsigned char *a=malloc(1000000);CHECK(a!=NULL);memset(a,'a',1000000);
    bench_sha256_bytes(a,1000000,h);free(a);CHECK(strcmp(h,"cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0")==0);
    puts("PASS PMC decoder/errors/lifecycle/thread ownership, CPU snapshots, SHA256 empty/abc/million-a");return 0;
}
