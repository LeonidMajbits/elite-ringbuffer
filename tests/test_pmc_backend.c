/* Synthetic perf syscall transport, NOT measurements. Link-time wrapping
 * exercises the actual native setup/enable/read/disable/close implementation. */
#define _GNU_SOURCE 1
#include "elite_pmc.h"
#include <errno.h>
#include <linux/perf_event.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
long __wrap_syscall(long number,...);
int __wrap_ioctl(int fd,unsigned long request,...);
ssize_t __wrap_read(int fd,void *buffer,size_t count);
int __wrap_close(int fd);
ssize_t __real_read(int fd,void *buffer,size_t count);
int __real_close(int fd);
static int opened[6],reads[6],next_fd,fail_mode;
static void reset(int m){memset(opened,0,sizeof(opened));memset(reads,0,sizeof(reads));next_fd=0;fail_mode=m;}
long __wrap_syscall(long number,...)
{
    if(number==SYS_gettid)return 1234;
    CHECK(number==SYS_perf_event_open);va_list ap;va_start(ap,number);
    struct perf_event_attr *a=va_arg(ap,struct perf_event_attr *);
    int pid=va_arg(ap,int),cpu=va_arg(ap,int),group=va_arg(ap,int);unsigned long flags=va_arg(ap,unsigned long);va_end(ap);
    CHECK(pid==0&&cpu==-1&&flags==PERF_FLAG_FD_CLOEXEC);
    CHECK(a->sample_period==0&&a->inherit==0&&a->exclude_kernel&&a->exclude_hv);
    CHECK(a->read_format==(PERF_FORMAT_GROUP|PERF_FORMAT_TOTAL_TIME_ENABLED|PERF_FORMAT_TOTAL_TIME_RUNNING|PERF_FORMAT_ID));
    int i=next_fd++;CHECK(i<6);CHECK(group==(i%2?100+i-1:-1));CHECK(a->disabled==(unsigned)(i%2==0));
    if(fail_mode==1&&i==1){errno=EACCES;return -1;}
    if(fail_mode==2){/* leader fails: the next attempt is another group leader. */
        next_fd=i+2;errno=ENOENT;return -1;}
    opened[i]=1;return 100+i;
}
int __wrap_ioctl(int fd,unsigned long request,...)
{
    CHECK(fd>=100&&fd<106&&opened[fd-100]);va_list ap;va_start(ap,request);
    if(request==PERF_EVENT_IOC_ID){uint64_t *id=va_arg(ap,uint64_t *);*id=(uint64_t)fd+900;va_end(ap);return 0;}
    /* Third ioctl argument is the integer flag macro, not a pointer. */
    unsigned int flag=va_arg(ap,unsigned int);va_end(ap);CHECK(flag==PERF_IOC_FLAG_GROUP);
    if((fail_mode==3&&request==PERF_EVENT_IOC_ENABLE)||(fail_mode==4&&request==PERF_EVENT_IOC_DISABLE)) {errno=EIO;return -1;}
    CHECK(request==PERF_EVENT_IOC_RESET||request==PERF_EVENT_IOC_ENABLE||request==PERF_EVENT_IOC_DISABLE);return 0;
}
ssize_t __wrap_read(int fd,void *buffer,size_t count)
{
    if(fd<100||fd>=106)return __real_read(fd,buffer,count);
    CHECK(opened[fd-100]&&count==7*sizeof(uint64_t));int r=reads[fd-100]++;
    if(fail_mode==5&&r==1)return 8;
    uint64_t *v=buffer;v[0]=2;v[1]=r?200:0;v[2]=r?(fail_mode==6?100:200):0;
    v[3]=r?100:0;v[4]=(uint64_t)fd+900;v[5]=r?200:0;v[6]=(uint64_t)fd+901;
    if(fail_mode==7&&r==1)v[6]=v[4];
    return (ssize_t)count;
}
int __wrap_close(int fd)
{if(fd<100||fd>=106)return __real_close(fd);CHECK(opened[fd-100]);opened[fd-100]=0;return 0;}
int main(void)
{
    for(int m=0;m<8;++m){reset(m);elite_pmc_context *c=elite_pmc_create(true);CHECK(c!=NULL);
        CHECK(elite_pmc_start(c)==0&&elite_pmc_stop(c)==0);const elite_pmc_result *p=elite_pmc_get_result(c);
        elite_pmc_status expected=ELITE_PMC_OK;
        if(m==1){CHECK(p->groups[0].status==ELITE_PMC_RESTRICTED_ACCESS||p->groups[0].status==ELITE_PMC_RESTRICTED_PARANOID);}
        if(m==2)expected=ELITE_PMC_UNSUPPORTED_EVENT;
        if(m==3)expected=ELITE_PMC_START_ERROR;
        if(m==4)expected=ELITE_PMC_STOP_ERROR;
        if(m==5)expected=ELITE_PMC_READ_ERROR;
        if(m==6)expected=ELITE_PMC_MULTIPLEXED;
        if(m==7)expected=ELITE_PMC_INVALID_READING;
        for(unsigned g=m==1?1u:0u;g<3;++g){CHECK(p->groups[g].status==expected);
            if(expected==ELITE_PMC_OK||expected==ELITE_PMC_MULTIPLEXED)CHECK(p->groups[g].raw_delta[0]==100&&p->groups[g].raw_delta[1]==200);}
        elite_pmc_destroy(c);for(unsigned i=0;i<6;++i)CHECK(opened[i]==0);}
    puts("PASS synthetic perf backend: 8 setup/read/enable/disable/denial/multiplex/error cases; no fake hardware evidence");return 0;
}
