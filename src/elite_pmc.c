#define _GNU_SOURCE 1
#include "elite_pmc.h"
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/resource.h>
#include <unistd.h>
#ifdef __linux__
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#endif
#ifdef __APPLE__
#include <libproc.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <sys/sysctl.h>
#endif

struct elite_pmc_context {
    elite_pmc_result result;
    pthread_t owner;
    int fd[ELITE_PMC_EVENTS];
    unsigned state;
};
static const char *const names[ELITE_PMC_EVENTS] = {
    "cpu_cycles", "instructions", "l1d_read_access", "l1d_read_miss",
    "llc_read_access", "llc_read_miss"
};
const char *elite_pmc_event_name(unsigned index)
{ return index < ELITE_PMC_EVENTS ? names[index] : "INVALID_EVENT"; }
const char *elite_pmc_status_name(elite_pmc_status s)
{
    static const char *const states[] = {"NOT_REQUESTED","READY","OK","MULTIPLEXED",
        "RESTRICTED_PARANOID","RESTRICTED_ACCESS","UNSUPPORTED_EVENT","UNSUPPORTED_PLATFORM",
        "OPEN_ERROR","START_ERROR","STOP_ERROR","READ_ERROR","NEVER_SCHEDULED","INVALID_READING"};
    return (unsigned)s < sizeof(states)/sizeof(states[0]) ? states[s] : "INVALID_STATUS";
}
elite_pmc_status elite_pmc_classify_open_error(int e, int paranoid)
{
    /* >=2 still permits this user-only, per-thread request on upstream Linux.
     * Some distributions add >2 restrictions. errno cannot exclude an LSM or
     * other access-control cause; the receipt retains that ambiguity. */
    if (e == EACCES || e == EPERM)
        return paranoid > 2 ? ELITE_PMC_RESTRICTED_PARANOID : ELITE_PMC_RESTRICTED_ACCESS;
    if (e == ENOENT || e == EOPNOTSUPP || e == ENODEV || e == EINVAL || e == ENOSYS)
        return ELITE_PMC_UNSUPPORTED_EVENT;
    return ELITE_PMC_OPEN_ERROR;
}
static int invalid(elite_pmc_group_result *g)
{ g->status=ELITE_PMC_INVALID_READING; g->os_error=EPROTO; return -1; }
int elite_pmc_decode_group(elite_pmc_group_result *g)
{
    if (g == NULL) { errno=EINVAL; return -1; }
    if (!g->before_valid || !g->after_valid || g->before[0]!=2 || g->after[0]!=2 ||
        g->ids[0]==g->ids[1] || g->ids[0]==0 || g->ids[1]==0) return invalid(g);
    for (unsigned pass=0; pass<2; ++pass) {
        const uint64_t *v=pass ? g->after : g->before;
        if (v[2]>v[1] || v[4]==v[6] ||
            !((v[4]==g->ids[0] && v[6]==g->ids[1]) ||
              (v[4]==g->ids[1] && v[6]==g->ids[0]))) return invalid(g);
    }
    if (g->after[1]<g->before[1] || g->after[2]<g->before[2]) return invalid(g);
    g->time_enabled_ns=g->after[1]-g->before[1];
    g->time_running_ns=g->after[2]-g->before[2];
    if (g->time_running_ns>g->time_enabled_ns) return invalid(g);
    for (unsigned i=0; i<2; ++i) {
        unsigned b=g->before[4]==g->ids[i] ? 3u : 5u;
        unsigned a=g->after[4]==g->ids[i] ? 3u : 5u;
        if (g->after[a]<g->before[b]) return invalid(g);
        g->raw_delta[i]=g->after[a]-g->before[b];
    }
    if (g->time_running_ns==0) {
        if(g->raw_delta[0]!=0 || g->raw_delta[1]!=0)return invalid(g);
        g->status=ELITE_PMC_NEVER_SCHEDULED;
    }
    else g->status=g->time_running_ns<g->time_enabled_ns ? ELITE_PMC_MULTIPLEXED : ELITE_PMC_OK;
    return 0;
}
#ifdef __linux__
static int read_group(int fd, uint64_t v[ELITE_PMC_FRAME_WORDS])
{
    ssize_t n;
    unsigned tries=0;
    do { n=read(fd,v,ELITE_PMC_FRAME_WORDS*sizeof(*v)); } while(n<0 && errno==EINTR && ++tries<4);
    if (n!=(ssize_t)(ELITE_PMC_FRAME_WORDS*sizeof(*v))) { if(n>=0)errno=EPROTO; return -1; }
    return 0;
}
static void close_pair(elite_pmc_context *c,unsigned group)
{
    for(unsigned j=0;j<2;++j) {
        unsigned i=group*2+j;
        if(c->fd[i]>=0) { (void)close(c->fd[i]); c->fd[i]=-1; }
    }
}
#endif
elite_pmc_context *elite_pmc_create(bool enabled)
{
    elite_pmc_context *c=calloc(1,sizeof(*c));
    if(c==NULL) return NULL;
    c->owner=pthread_self(); c->result.requested=enabled; c->result.paranoid=-999;
    for(unsigned i=0;i<ELITE_PMC_EVENTS;++i)c->fd[i]=-1;
#ifdef __linux__
    c->result.owner_thread_id=(uint64_t)syscall(SYS_gettid);
    FILE *p=fopen("/proc/sys/kernel/perf_event_paranoid","r");
    if(p!=NULL) { int value; if(fscanf(p,"%d",&value)==1)c->result.paranoid=value; (void)fclose(p); }
    for(unsigned g=0;g<ELITE_PMC_GROUPS;++g) {
        elite_pmc_group_result *r=&c->result.groups[g];
        for(unsigned j=0;j<2;++j) {
            r->type[j]=g==0 ? PERF_TYPE_HARDWARE : PERF_TYPE_HW_CACHE;
            r->config[j]=g==0 ? (j==0 ? PERF_COUNT_HW_CPU_CYCLES : PERF_COUNT_HW_INSTRUCTIONS) :
                (uint64_t)(g==1 ? PERF_COUNT_HW_CACHE_L1D : PERF_COUNT_HW_CACHE_LL) |
                ((uint64_t)PERF_COUNT_HW_CACHE_OP_READ<<8) |
                ((uint64_t)(j==0 ? PERF_COUNT_HW_CACHE_RESULT_ACCESS : PERF_COUNT_HW_CACHE_RESULT_MISS)<<16);
        }
        if(!enabled) { r->status=ELITE_PMC_NOT_REQUESTED; continue; }
        r->status=ELITE_PMC_READY;
        for(unsigned j=0;j<2;++j) {
            struct perf_event_attr a; memset(&a,0,sizeof(a));
            a.size=(uint32_t)sizeof(a); a.type=r->type[j]; a.config=r->config[j];
            a.disabled=j==0 ? 1u : 0u; a.exclude_kernel=1; a.exclude_hv=1;
            a.inherit=0; a.sample_period=0;
            a.read_format=PERF_FORMAT_GROUP|PERF_FORMAT_TOTAL_TIME_ENABLED|PERF_FORMAT_TOTAL_TIME_RUNNING|PERF_FORMAT_ID;
            long fd=syscall(SYS_perf_event_open,&a,0,-1,j==0 ? -1 : c->fd[g*2],PERF_FLAG_FD_CLOEXEC);
            if(fd<0) { r->os_error=errno; r->status=elite_pmc_classify_open_error(errno,c->result.paranoid); close_pair(c,g); break; }
            if(fd>INT_MAX) { (void)close((int)fd); r->os_error=EOVERFLOW; r->status=ELITE_PMC_OPEN_ERROR; close_pair(c,g); break; }
            c->fd[g*2+j]=(int)fd;
            if(ioctl((int)fd,PERF_EVENT_IOC_ID,&r->ids[j])<0) {
                r->os_error=errno; r->status=ELITE_PMC_OPEN_ERROR; close_pair(c,g); break;
            }
        }
    }
#else
    for(unsigned g=0;g<ELITE_PMC_GROUPS;++g)c->result.groups[g].status=enabled ? ELITE_PMC_UNSUPPORTED_PLATFORM : ELITE_PMC_NOT_REQUESTED;
#ifdef __APPLE__
    (void)pthread_threadid_np(NULL,&c->result.owner_thread_id);
#endif
#endif
    return c;
}
int elite_pmc_start(elite_pmc_context *c)
{
    if(c==NULL || c->state!=0 || !pthread_equal(c->owner,pthread_self())) { errno=EINVAL; return -1; }
    c->state=1;
#ifdef __linux__
    for(unsigned g=0;g<ELITE_PMC_GROUPS;++g) {
        elite_pmc_group_result *r=&c->result.groups[g];
        if(r->status!=ELITE_PMC_READY)continue;
        if(ioctl(c->fd[g*2],PERF_EVENT_IOC_RESET,PERF_IOC_FLAG_GROUP)<0 ||
           read_group(c->fd[g*2],r->before)<0) {
            r->status=ELITE_PMC_START_ERROR; r->os_error=errno; close_pair(c,g); continue;
        }
        r->before_valid=true;
        if(ioctl(c->fd[g*2],PERF_EVENT_IOC_ENABLE,PERF_IOC_FLAG_GROUP)<0) {
            r->status=ELITE_PMC_START_ERROR; r->os_error=errno; close_pair(c,g);
        }
    }
#endif
    return 0;
}
int elite_pmc_stop(elite_pmc_context *c)
{
    if(c==NULL || c->state!=1 || !pthread_equal(c->owner,pthread_self())) { errno=EINVAL; return -1; }
    c->state=2;
#ifdef __linux__
    for(unsigned g=0;g<ELITE_PMC_GROUPS;++g) {
        elite_pmc_group_result *r=&c->result.groups[g];
        if(r->status!=ELITE_PMC_READY)continue;
        if(ioctl(c->fd[g*2],PERF_EVENT_IOC_DISABLE,PERF_IOC_FLAG_GROUP)<0) {
            r->status=ELITE_PMC_STOP_ERROR; r->os_error=errno; close_pair(c,g); continue;
        }
        if(read_group(c->fd[g*2],r->after)<0) { r->status=ELITE_PMC_READ_ERROR; r->os_error=errno; close_pair(c,g); continue; }
        r->after_valid=true; (void)elite_pmc_decode_group(r); close_pair(c,g);
    }
#endif
    return 0;
}
const elite_pmc_result *elite_pmc_get_result(const elite_pmc_context *c)
{ return c==NULL ? NULL : &c->result; }
void elite_pmc_destroy(elite_pmc_context *c)
{
    if(c==NULL)return;
    for(unsigned i=0;i<ELITE_PMC_EVENTS;++i)if(c->fd[i]>=0)(void)close(c->fd[i]);
    free(c);
}
static int usage_snapshot(int who,elite_cpu_snapshot *s)
{
    struct rusage r; memset(s,0,sizeof(*s));
    if(getrusage(who,&r)!=0){s->error=errno;return -1;}
    if(r.ru_utime.tv_sec<0 || r.ru_stime.tv_sec<0){s->error=ERANGE;return -1;}
    s->user_us=(uint64_t)r.ru_utime.tv_sec*UINT64_C(1000000)+(uint64_t)r.ru_utime.tv_usec;
    s->system_us=(uint64_t)r.ru_stime.tv_sec*UINT64_C(1000000)+(uint64_t)r.ru_stime.tv_usec;
    s->minor_faults=r.ru_minflt; s->major_faults=r.ru_majflt;
    s->voluntary_switches=r.ru_nvcsw; s->involuntary_switches=r.ru_nivcsw;
    return 0;
}
int elite_cpu_process_snapshot(elite_cpu_snapshot *s)
{ if(s==NULL){errno=EINVAL;return -1;} return usage_snapshot(RUSAGE_SELF,s); }
int elite_cpu_thread_snapshot(elite_cpu_snapshot *s)
{
    if(s==NULL){errno=EINVAL;return -1;}
#ifdef __linux__
    return usage_snapshot(RUSAGE_THREAD,s);
#elif defined(__APPLE__)
    memset(s,0,sizeof(*s));
    thread_basic_info_data_t info; mach_msg_type_number_t n=THREAD_BASIC_INFO_COUNT;
    kern_return_t k=thread_info(pthread_mach_thread_np(pthread_self()),THREAD_BASIC_INFO,(thread_info_t)&info,&n);
    if(k!=KERN_SUCCESS || n!=THREAD_BASIC_INFO_COUNT) { s->error=(int)k; if(!s->error)s->error=EINVAL; return -1; }
    s->user_us=(uint64_t)info.user_time.seconds*UINT64_C(1000000)+(uint64_t)info.user_time.microseconds;
    s->system_us=(uint64_t)info.system_time.seconds*UINT64_C(1000000)+(uint64_t)info.system_time.microseconds;
    s->instant_usage=info.cpu_usage; s->instant_usage_scale=TH_USAGE_SCALE;
    s->minor_faults=-1;s->major_faults=-1;s->voluntary_switches=-1;s->involuntary_switches=-1;
    return 0;
#else
    memset(s,0,sizeof(*s));s->error=ENOTSUP;return -1;
#endif
}
int elite_pmc_process_counts(elite_process_counts *c)
{
    if(c==NULL){errno=EINVAL;return -1;}memset(c,0,sizeof(*c));
#ifdef __APPLE__
    struct rusage_info_v4 r;memset(&r,0,sizeof(r));
    if(proc_pid_rusage(getpid(),RUSAGE_INFO_V4,(rusage_info_t *)&r)!=0) {c->error=errno?errno:EIO;return -1;}
    c->cycles=r.ri_cycles;c->instructions=r.ri_instructions;return 0;
#else
    c->error=ENOTSUP;return -1;
#endif
}
int elite_pmc_virtual_read(elite_virtual_sample *s)
{
    if(s==NULL){errno=EINVAL;return -1;}memset(s,0,sizeof(*s));
#if defined(__APPLE__) && defined(__aarch64__)
    __asm__ __volatile__("isb" ::: "memory");
    s->mach_before=mach_absolute_time();
    __asm__ __volatile__("isb\n\tmrs %0, cntvct_el0\n\tisb" : "=r"(s->count) :: "memory");
    s->mach_after=mach_absolute_time();
    return s->mach_after>=s->mach_before ? 0 : -1;
#else
    errno=ENOTSUP;return -1;
#endif
}
int elite_pmc_virtual_frequency(uint64_t *hz)
{
    if(hz==NULL){errno=EINVAL;return -1;}*hz=0;
#if defined(__APPLE__) && defined(__aarch64__)
    __asm__ __volatile__("mrs %0, cntfrq_el0" : "=r"(*hz));
    return *hz!=0 ? 0 : -1;
#else
    errno=ENOTSUP;return -1;
#endif
}
int elite_pmc_sysctl_tbfrequency(uint64_t *hz)
{
    if(hz==NULL){errno=EINVAL;return -1;}*hz=0;
#ifdef __APPLE__
    size_t bytes=sizeof(*hz);
    if(sysctlbyname("hw.tbfrequency",hz,&bytes,NULL,0)!=0)return -1;
    if((bytes!=4 && bytes!=8)||*hz==0){errno=EINVAL;return -1;}
    return 0;
#else
    errno=ENOTSUP;return -1;
#endif
}
