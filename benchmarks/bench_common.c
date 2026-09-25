#define _GNU_SOURCE 1
#include "bench_common.h"
#include "elite_topology.h"
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <sched.h>
#include <signal.h>
#include <spawn.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <time.h>
#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/thread_policy.h>
#include <pthread.h>
#include <pthread/qos.h>
#include <sys/sysctl.h>
#endif
extern char **environ;
static struct bench_cohort *active;
static char failure_directory[BENCH_PATH];
static volatile sig_atomic_t interrupted;
static void on_signal(int signum) { interrupted=signum; }
static void cleanup(void)
{
    if(active==NULL) return;
    /* Only unreaped children created by this controller can be signaled.
     * A failure is never turned into a normal token-reconciliation result. */
    for(uint32_t i=0;i<active->workers;++i)
        if(active->child[i].pid>0) (void)kill(active->child[i].pid,SIGKILL);
    for(uint32_t i=0;i<active->workers;++i) if(active->child[i].pid>0) {
        while(waitpid(active->child[i].pid,NULL,0)<0 && errno==EINTR) {}
        active->child[i].pid=0;
    }
    for(uint32_t i=0;i<2;++i) {
        if(active->object[i]!=NULL) (void)elite_object_destroy(&active->object[i]);
        /* If unresolved holders prevented normal destroy, all controlled
         * children are now terminal. Remove only our recorded test name. */
        if(active->first[i].name[0]) (void)shm_unlink(active->first[i].name);
    }
    if(active->control!=NULL) (void)munmap(active->control,BENCH_CTRL_BYTES);
    if(active->control_path[0]) (void)unlink(active->control_path);
}
_Noreturn void bench_fail(const char *file,int line,const char *message)
{
    int saved=errno;
    fprintf(stderr,"HARNESS_FAILED pid=%ld %s:%d %s errno=%d\n",(long)getpid(),file,line,message,saved);
    if(failure_directory[0]) {
        char name[80],path[BENCH_PATH];
        (void)snprintf(name,sizeof(name),"FAILED-%ld.txt",(long)getpid());
        int n=snprintf(path,sizeof(path),"%s/%s",failure_directory,name);
        if(n>0 && (size_t)n<sizeof(path)) {
            FILE *f=fopen(path,"w");
            if(f!=NULL) { (void)fprintf(f,"HARNESS_FAILED\n%s:%d\n%s\nerrno=%d\nNo complete-run success or loss count is certified.\n",file,line,message,saved); (void)fclose(f); }
        }
    }
    exit(1);
}
_Noreturn void bench_elite_fail(const char *file,int line,const char *operation,elite_result r)
{
    fprintf(stderr,"elite status=%s outcome=%"PRIu32" os_error=%"PRId32"\n",elite_status_string(r.status),r.outcome,r.os_error);
    bench_fail(file,line,operation);
}
void bench_process_init(const char *directory)
{
    B_CHECK(strlen(directory)<sizeof(failure_directory));
    strcpy(failure_directory,directory);
    struct sigaction action; memset(&action,0,sizeof(action)); action.sa_handler=on_signal;
    B_CHECK(sigemptyset(&action.sa_mask)==0);
    B_CHECK(sigaction(SIGINT,&action,NULL)==0 && sigaction(SIGTERM,&action,NULL)==0);
    action.sa_handler=SIG_IGN; B_CHECK(sigaction(SIGPIPE,&action,NULL)==0);
}
void bench_path(char out[BENCH_PATH],const char *directory,const char *name)
{ int n=snprintf(out,BENCH_PATH,"%s/%s",directory,name); B_CHECK(n>0 && (size_t)n<BENCH_PATH); }
void bench_mkdir(const char *path) { B_CHECK(mkdir(path,0700)==0); }
static uint64_t number(const char *s,uint64_t low,uint64_t high)
{
    B_CHECK(s!=NULL && s[0]>='0' && s[0]<='9');
    char *end=NULL;errno=0; unsigned long long v=strtoull(s,&end,10);
    B_CHECK(errno==0 && end!=s && *end=='\0' && v>=low && v<=high);return (uint64_t)v;
}
void bench_parse(int argc,char **argv,bool latency,struct bench_options *o)
{
    memset(o,0,sizeof(*o));o->mode=latency?ELITE_SPSC:ELITE_NCQ;o->producers=1;o->consumers=1;
    o->trials=1;o->count=BENCH_MAX_ITEMS;o->warmup=1000000;o->capacity=1024;o->calibration=1000000;o->timeout_seconds=600;
    for(uint32_t i=0;i<BENCH_MAX_WORKERS;++i) o->cpus[i]=-1;
    char default_name[96];(void)snprintf(default_name,sizeof(default_name),"%s-%ld",latency?"latency":"throughput",(long)getpid());
    strcpy(o->output,default_name);
    for(int i=1;i<argc;++i) {
        if(strcmp(argv[i],"--help")==0) {
            printf("usage: %s [--mode spsc|ncq] [--count N] [--warmup N] [--trials N] [--capacity N] [--out NEW_DIRECTORY] [--timeout SECONDS] [--cpus ID,ID,...] [--affinity-tag N] [--qos default|user-initiated|background] [--calibration N]%s\n",argv[0],latency?"":" [--producers N --consumers N]");
            printf("Latency count means completed round trips (twice that many directional records). Darwin tags are REQUESTS, not P-core pinning. Every trial retains raw evidence.\n");exit(0);
        }
        B_CHECK(i+1<argc);const char *key=argv[i],*value=argv[++i];
        if(strcmp(key,"--mode")==0){B_CHECK(strcmp(value,"spsc")==0 || strcmp(value,"ncq")==0);o->mode=strcmp(value,"spsc")==0?ELITE_SPSC:ELITE_NCQ;}
        else if(strcmp(key,"--count")==0)o->count=number(value,1,BENCH_MAX_ITEMS);
        else if(strcmp(key,"--warmup")==0)o->warmup=number(value,0,BENCH_MAX_ITEMS);
        else if(strcmp(key,"--trials")==0)o->trials=(uint32_t)number(value,1,100);
        else if(strcmp(key,"--capacity")==0)o->capacity=number(value,2,65536);
        else if(strcmp(key,"--calibration")==0)o->calibration=number(value,1,1000000);
        else if(strcmp(key,"--timeout")==0)o->timeout_seconds=(unsigned)number(value,1,86400);
        else if(strcmp(key,"--out")==0){B_CHECK(strlen(value)<sizeof(o->output));strcpy(o->output,value);}
        else if(strcmp(key,"--producers")==0){B_CHECK(!latency);o->producers=(uint32_t)number(value,1,16);}
        else if(strcmp(key,"--consumers")==0){B_CHECK(!latency);o->consumers=(uint32_t)number(value,1,16);}
        else if(strcmp(key,"--affinity-tag")==0)o->affinity_tag=(int)number(value,1,INT_MAX);
        else if(strcmp(key,"--qos")==0){B_CHECK(strcmp(value,"default")==0||strcmp(value,"user-initiated")==0||strcmp(value,"background")==0||strcmp(value,"user-interactive")==0);o->qos=strcmp(value,"default")==0?0:(strcmp(value,"user-initiated")==0?1:(strcmp(value,"user-interactive")==0?3:2));}
        else if(strcmp(key,"--cpus")==0){
            char copy[1024];B_CHECK(strlen(value)<sizeof(copy));strcpy(copy,value);
            B_CHECK(copy[0]!=',' && copy[strlen(copy)-1]!=',' && strstr(copy,",,")==NULL);
            char *save=NULL;for(char *s=strtok_r(copy,",",&save);s!=NULL;s=strtok_r(NULL,",",&save)) {
                B_CHECK(o->cpu_count<BENCH_MAX_WORKERS);o->cpus[o->cpu_count++]=(int)number(s,0,1023);
            }
            B_CHECK(o->cpu_count>0);
        } else bench_fail(__FILE__,__LINE__,"unknown option");
    }
    B_CHECK((o->capacity&(o->capacity-1))==0 && o->producers+o->consumers<=BENCH_MAX_WORKERS);
    B_CHECK(o->capacity>=o->producers+o->consumers && o->count%o->producers==0 && o->warmup%o->producers==0);
    B_CHECK(o->mode!=ELITE_SPSC || (o->producers==1 && o->consumers==1));
#ifdef __APPLE__
    B_CHECK(o->cpu_count==0); /* Reject the fiction of a Darwin CPU-number mask. */
#else
    B_CHECK(o->affinity_tag==0 && o->qos==0);
#endif
    if(o->output[0]!='/') {char cwd[BENCH_PATH],absolute[BENCH_PATH];B_CHECK(getcwd(cwd,sizeof(cwd))!=NULL);bench_path(absolute,cwd,o->output);strcpy(o->output,absolute);}
    bench_mkdir(o->output);bench_process_init(o->output);bench_machine_json(o->output);
}
void bench_write_all(int fd,const void *data,size_t bytes)
{
    const unsigned char *p=data;
    while(bytes!=0){ssize_t n=write(fd,p,bytes);if(n<0&&errno==EINTR){B_CHECK(!interrupted);continue;}B_CHECK(n>0);p+=(size_t)n;bytes-=(size_t)n;}
}
void bench_read_all(int fd,void *data,size_t bytes)
{
    unsigned char *p=data;
    while(bytes!=0){ssize_t n=read(fd,p,bytes);if(n<0&&errno==EINTR){B_CHECK(!interrupted);continue;}B_CHECK(n>0);p+=(size_t)n;bytes-=(size_t)n;}
}
void bench_save(const char *directory,const char *name,const void *data,size_t bytes)
{
    char path[BENCH_PATH];bench_path(path,directory,name);
    int fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);B_CHECK(fd>=0);
    bench_write_all(fd,data,bytes);B_CHECK(close(fd)==0);
}
FILE *bench_json_open(const char *directory,const char *name)
{
    char path[BENCH_PATH];bench_path(path,directory,name);
    int fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);B_CHECK(fd>=0);
    FILE *f=fdopen(fd,"w");B_CHECK(f!=NULL);return f;
}
void bench_json_close(FILE *f){B_CHECK(!ferror(f));B_CHECK(fclose(f)==0);}
void bench_id(uint8_t id[16])
{
    int fd=open("/dev/urandom",O_RDONLY);B_CHECK(fd>=0);bench_read_all(fd,id,16);B_CHECK(close(fd)==0);
    unsigned v=0;for(unsigned i=0;i<16;++i)v|=id[i];B_CHECK(v!=0);
}
void bench_clock_init(struct bench_clock *c)
{
#ifdef __APPLE__
    mach_timebase_info_data_t info;B_CHECK(mach_timebase_info(&info)==KERN_SUCCESS && info.numer!=0 && info.denom!=0);
    c->numer=info.numer;c->denom=info.denom;c->reported_resolution_ns=0;
#else
    struct timespec t;B_CHECK(clock_getres(CLOCK_MONOTONIC_RAW,&t)==0);
    c->numer=1;c->denom=1;c->reported_resolution_ns=(uint64_t)t.tv_sec*UINT64_C(1000000000)+(uint64_t)t.tv_nsec;
#endif
}
uint64_t bench_tick(void)
{
#ifdef __APPLE__
    return mach_absolute_time();
#else
    struct timespec t;B_CHECK(clock_gettime(CLOCK_MONOTONIC_RAW,&t)==0 && t.tv_sec>=0);
    B_CHECK((uint64_t)t.tv_sec<=UINT64_MAX/UINT64_C(1000000000));
    return (uint64_t)t.tv_sec*UINT64_C(1000000000)+(uint64_t)t.tv_nsec;
#endif
}
uint64_t bench_ordered_tick(void)
{
    /* Conservative harness boundary, NOT a change to ring atomics. Both
     * calls include these costs. Never subtract them from every sample. */
    atomic_signal_fence(memory_order_seq_cst);
#if defined(__aarch64__)
    __asm__ __volatile__("dsb ishld\n\tisb" ::: "memory");
#elif defined(__x86_64__)
    __asm__ __volatile__("lfence" ::: "memory");
#else
#error "Benchmark timestamp ordering requires a reviewed target adapter"
#endif
    uint64_t t=bench_tick();
#if defined(__aarch64__)
    __asm__ __volatile__("isb" ::: "memory");
#elif defined(__x86_64__)
    __asm__ __volatile__("lfence" ::: "memory");
#endif
    atomic_signal_fence(memory_order_seq_cst);return t;
}
uint64_t bench_watch_ns(void)
{
    struct timespec t;B_CHECK(clock_gettime(CLOCK_MONOTONIC,&t)==0);
    return (uint64_t)t.tv_sec*UINT64_C(1000000000)+(uint64_t)t.tv_nsec;
}
long double bench_ns(const struct bench_clock *c,uint64_t ticks)
{ return (long double)ticks*(long double)c->numer/(long double)c->denom; }
uint64_t bench_ns_ceil(const struct bench_clock *c,uint64_t ticks)
{
    uint64_t q=ticks/c->denom,r=ticks%c->denom;
    B_CHECK(q<=UINT64_MAX/c->numer);uint64_t whole=q*c->numer;
    uint64_t small=r*c->numer;uint64_t fraction=small/c->denom+(small%c->denom!=0?1u:0u);
    B_CHECK(whole<=UINT64_MAX-fraction);return whole+fraction;
}
uint64_t bench_future_tick(const struct bench_clock *c,uint64_t ns)
{
    B_CHECK(ns<=UINT64_MAX/c->denom);uint64_t product=ns*c->denom;
    uint64_t delta=product/c->numer+(product%c->numer!=0?1u:0u),now=bench_tick();
    B_CHECK(now<=UINT64_MAX-delta);return now+delta;
}
void bench_relax(void)
{
#if defined(__x86_64__)
    __asm__ __volatile__("pause");
#elif defined(__aarch64__)
    __asm__ __volatile__("yield");
#endif
}
void bench_wait_start(uint64_t tick)
{
    while(bench_tick()<tick){B_CHECK(!interrupted);bench_relax();}
}
static int compare_u64(const void *a,const void *b)
{uint64_t x=*(const uint64_t *)a,y=*(const uint64_t *)b;return (x>y)-(x<y);}
void bench_statistics(uint64_t *v,uint64_t n,struct bench_stats *s)
{
    B_CHECK(n>0 && n<=BENCH_MAX_ITEMS);memset(s,0,sizeof(*s));s->count=n;
    for(uint64_t i=0;i<n;++i){if(v[i]==0)++s->zeros;else if(s->minimum_nonzero==0||v[i]<s->minimum_nonzero)s->minimum_nonzero=v[i];}
    qsort(v,(size_t)n,sizeof(*v),compare_u64);
    const uint64_t numerator[6]={50000,90000,99000,99900,99990,100000};
    for(unsigned i=0;i<6;++i){uint64_t rank=(n*numerator[i]+99999)/100000;s->q[i]=v[rank-1];}
}
void bench_calibrate(uint64_t *v,uint64_t n,struct bench_stats *s)
{
    for(uint64_t i=0;i<n;++i){uint64_t a=bench_ordered_tick(),b=bench_ordered_tick();B_CHECK(b>=a);v[i]=b-a;}
    /* Caller saves raw order before invoking bench_statistics. */
    memset(s,0,sizeof(*s));s->count=n;
}
void bench_placement_apply(int cpu,int tag,int qos,struct bench_placement *p)
{
    memset(p,0,sizeof(*p));p->requested_cpu=cpu;p->verified_cpu=-1;p->end_verified_cpu=-1;p->affinity_tag=tag;p->qos_requested=qos;p->tag_readback=-1;
#ifdef __APPLE__
    if(qos!=0)p->qos_error=pthread_set_qos_class_self_np(qos==1?QOS_CLASS_USER_INITIATED:(qos==3?QOS_CLASS_USER_INTERACTIVE:QOS_CLASS_BACKGROUND),0);
    if(tag!=0){
        thread_affinity_policy_data_t policy={tag};thread_t t=pthread_mach_thread_np(pthread_self());
        p->tag_set_error=thread_policy_set(t,THREAD_AFFINITY_POLICY,(thread_policy_t)&policy,THREAD_AFFINITY_POLICY_COUNT);
        mach_msg_type_number_t count=THREAD_AFFINITY_POLICY_COUNT;boolean_t get_default=FALSE;
        p->tag_get_error=thread_policy_get(t,THREAD_AFFINITY_POLICY,(thread_policy_t)&policy,&count,&get_default);
        if(p->tag_get_error==KERN_SUCCESS && !get_default)p->tag_readback=policy.affinity_tag;
    }
    /* Even KERN_SUCCESS is only a task-local affinity hint, never P pinning. */
#else
    if(cpu>=0){
        B_CHECK(cpu<CPU_SETSIZE);cpu_set_t mask,actual;CPU_ZERO(&mask);CPU_SET((size_t)cpu,&mask);
        if(sched_setaffinity(0,sizeof(mask),&mask)<0)p->affinity_error=errno;
        else if(sched_getaffinity(0,sizeof(actual),&actual)<0)p->affinity_error=errno;
        else if(CPU_COUNT(&actual)==1 && CPU_ISSET((size_t)cpu,&actual)){p->affinity_enforced=1;p->verified_cpu=cpu;}
        else p->affinity_error=EINVAL;
        B_CHECK(p->affinity_enforced==1);
    }
#endif
}
void bench_placement_finish(struct bench_placement *p)
{
#ifndef __APPLE__
    if(p->affinity_enforced){cpu_set_t actual;B_CHECK(sched_getaffinity(0,sizeof(actual),&actual)==0);B_CHECK(CPU_COUNT(&actual)==1&&CPU_ISSET((size_t)p->verified_cpu,&actual));p->end_verified_cpu=p->verified_cpu;}
#else
    (void)p;
#endif
}
void bench_placement_json(FILE *f,const struct bench_placement *p)
{
    (void)fprintf(f,"{\"requested_cpu\":%d,\"affinity_error\":%d,\"verified_cpu_before\":%d,\"verified_cpu_after\":%d,\"affinity_enforced\":%s,\"darwin_affinity_tag\":%d,\"tag_set_error\":%d,\"tag_get_error\":%d,\"tag_readback\":%d,\"qos_request\":%d,\"qos_error\":%d,\"p_core_pinning\":\"NOT_ESTABLISHED\"}",p->requested_cpu,p->affinity_error,p->verified_cpu,p->end_verified_cpu,p->affinity_enforced?"true":"false",p->affinity_tag,p->tag_set_error,p->tag_get_error,p->tag_readback,p->qos_requested,p->qos_error);
}
void bench_stats_json(FILE *f,const struct bench_stats *s,const struct bench_clock *c)
{
    const char *name[6]={"p50","p90","p99","p99_9","p99_99","max"};
    (void)fprintf(f,"{\"samples\":%"PRIu64",\"zeros\":%"PRIu64",\"minimum_nonzero_ticks\":%"PRIu64",\"raw_tick_quantiles\":{",s->count,s->zeros,s->minimum_nonzero);
    for(unsigned i=0;i<6;++i)(void)fprintf(f,"%s\"%s\":%"PRIu64,i?",":"",name[i],s->q[i]);
    (void)fprintf(f,"},\"nanoseconds_ceiling\":{");
    for(unsigned i=0;i<6;++i)(void)fprintf(f,"%s\"%s\":%"PRIu64,i?",":"",name[i],bench_ns_ceil(c,s->q[i]));
    (void)fprintf(f,"}}");
}
void bench_hardware_topology_json(FILE *file)
{
    struct elite_hardware_topology *t=NULL;
    B_CHECK(elite_topology_discover(&t)==0);
    B_CHECK(elite_topology_json(file,t)==0);
    elite_topology_free(t);
}
void bench_machine_json(const char *directory)
{
    struct utsname u;B_CHECK(uname(&u)==0);FILE *f=bench_json_open(directory,"machine.txt");
    (void)fprintf(f,"system=%s\nrelease=%s\narchitecture=%s\npage_size=%ld\nonline_cpus=%ld\ncompiler=%s\n",u.sysname,u.release,u.machine,sysconf(_SC_PAGESIZE),sysconf(_SC_NPROCESSORS_ONLN),__VERSION__);
#ifdef __APPLE__
    const char *keys[]={"hw.model","machdep.cpu.brand_string","hw.physicalcpu","hw.logicalcpu","hw.perflevel0.physicalcpu","hw.perflevel1.physicalcpu","hw.cachelinesize"};
    for(unsigned i=0;i<sizeof(keys)/sizeof(keys[0]);++i){
        if(i<2){char buf[256];size_t n=sizeof(buf);if(sysctlbyname(keys[i],buf,&n,NULL,0)==0){buf[255]='\0';(void)fprintf(f,"%s=%s\n",keys[i],buf);}else(void)fprintf(f,"%s=UNAVAILABLE errno=%d\n",keys[i],errno);}
        else {uint64_t value=0;size_t n=sizeof(value);if(sysctlbyname(keys[i],&value,&n,NULL,0)==0)(void)fprintf(f,"%s=%"PRIu64" (bytes=%zu)\n",keys[i],value,n);else(void)fprintf(f,"%s=UNAVAILABLE errno=%d\n",keys[i],errno);}
    }
#else
    cpu_set_t mask;if(sched_getaffinity(0,sizeof(mask),&mask)==0){(void)fprintf(f,"allowed_cpus=");for(size_t i=0;i<CPU_SETSIZE;++i)if(CPU_ISSET(i,&mask))(void)fprintf(f,"%zu,",i);(void)fputc('\n',f);}
    FILE *q=fopen("/sys/fs/cgroup/cpu.max","r");if(q!=NULL){char line[128];if(fgets(line,sizeof(line),q)!=NULL)(void)fprintf(f,"cgroup_cpu_max=%s",line);(void)fclose(q);}
#endif
    (void)fprintf(f,"Primary benchmarks enable no periodic profiler or sampling interrupt. OS scheduling and timestamp overhead are not removed.\n");bench_json_close(f);
    f=bench_json_open(directory,"hardware_topology.json");bench_hardware_topology_json(f);fputc('\n',f);bench_json_close(f);
}
void bench_cohort_init(struct bench_cohort *b,const char *directory,uint32_t workers,unsigned timeout)
{
    memset(b,0,sizeof(*b));B_CHECK(active==NULL && workers<=BENCH_MAX_WORKERS);b->workers=workers;
    strcpy(b->directory,directory);bench_process_init(directory);
    for(uint32_t i=0;i<workers;++i){b->child[i].command_fd=-1;b->child[i].result_fd=-1;}
    active=b;static bool installed=false;if(!installed){B_CHECK(atexit(cleanup)==0);installed=true;}
    b->deadline_ns=bench_watch_ns()+(uint64_t)timeout*UINT64_C(1000000000);
    bench_path(b->control_path,directory,"control.bin");int fd=open(b->control_path,O_RDWR|O_CREAT|O_EXCL,0600);B_CHECK(fd>=0);
    B_CHECK(ftruncate(fd,BENCH_CTRL_BYTES)==0);b->control=mmap(NULL,BENCH_CTRL_BYTES,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);B_CHECK(b->control!=MAP_FAILED);B_CHECK(close(fd)==0);
    memset(b->control,0,BENCH_CTRL_BYTES);for(uint32_t i=0;i<BENCH_MAX_WORKERS;++i)atomic_init(&b->control->cell[i].drain,0);
}
void bench_cohort_create(struct bench_cohort *b,uint32_t which,uint32_t mode,uint32_t producers,uint32_t consumers,uint64_t capacity)
{ bench_cohort_create_ex(b,which,mode,producers,consumers,capacity,64,ELITE_CHECKSUM_NONE,UINT64_C(268435456)); }
void bench_cohort_create_ex(struct bench_cohort *b,uint32_t which,uint32_t mode,uint32_t producers,uint32_t consumers,uint64_t capacity,uint32_t payload,uint32_t checksum,uint64_t limit)
{
    B_CHECK(which<2 && b->authority[which]==NULL);uint8_t host[16],auth[16];bench_id(host);bench_id(auth);
    B_OK(elite_authority_create(host,auth,limit,&b->authority[which]));
    elite_endpoint_definition definitions[BENCH_MAX_WORKERS];memset(definitions,0,sizeof(definitions));
    for(uint32_t i=0;i<producers+consumers;++i){bench_id(definitions[i].endpoint_id);bench_id(definitions[i].process_incarnation_id);definitions[i].role=i<producers?ELITE_PRODUCER:ELITE_CONSUMER;}
    elite_config cfg={mode,ELITE_POLL_ONLY,checksum,payload,capacity,producers,consumers,0};
    B_OK(elite_create(b->authority[which],&cfg,definitions,&b->object[which]));B_OK(elite_object_activate(b->object[which]));
    if(b->objects<which+1)b->objects=which+1;
}
void bench_spawn(struct bench_cohort *b,uint32_t i,const char *executable)
{
    B_CHECK(i<b->workers);int input[2],output[2];B_CHECK(pipe(input)==0&&pipe(output)==0);
    for(unsigned j=0;j<2;++j){B_CHECK(fcntl(input[j],F_SETFD,FD_CLOEXEC)==0);B_CHECK(fcntl(output[j],F_SETFD,FD_CLOEXEC)==0);}
    posix_spawn_file_actions_t actions;B_CHECK(posix_spawn_file_actions_init(&actions)==0);
    B_CHECK(posix_spawn_file_actions_adddup2(&actions,input[0],STDIN_FILENO)==0);B_CHECK(posix_spawn_file_actions_adddup2(&actions,output[1],STDOUT_FILENO)==0);
    char *arguments[]={(char *)executable,(char *)"--worker",NULL};
    int error=posix_spawn(&b->child[i].pid,executable,&actions,NULL,arguments,environ);B_CHECK(error==0);
    B_CHECK(posix_spawn_file_actions_destroy(&actions)==0);B_CHECK(close(input[0])==0&&close(output[1])==0);
    b->child[i].command_fd=input[1];b->child[i].result_fd=output[0];
}
void bench_issue(struct bench_cohort *b,uint32_t object,uint32_t endpoint,uint32_t child,elite_grant *g)
{
    B_OK(elite_object_register_process(b->object[object],endpoint,b->child[child].pid));B_OK(elite_object_grant(b->object[object],endpoint,g));
    if(b->first[object].name[0]=='\0')b->first[object]=*g;
}
void bench_get_event(struct bench_cohort *b,uint32_t index,uint32_t event,struct bench_result *r)
{
    unsigned char *p=(unsigned char *)r;size_t left=sizeof(*r);
    while(left!=0){
        B_CHECK(!interrupted && bench_watch_ns()<b->deadline_ns);
        struct pollfd fd={b->child[index].result_fd,POLLIN,0};int n=poll(&fd,1,100);
        if(n<0&&errno==EINTR)continue;
        B_CHECK(n>=0);if(n==0)continue;
        B_CHECK((fd.revents&(POLLIN|POLLHUP))!=0);ssize_t got=read(fd.fd,p,left);
        if(got<0&&errno==EINTR)continue;
        B_CHECK(got>0);p+=(size_t)got;left-=(size_t)got;
    }
    B_CHECK(r->event==event && r->index==index);
}
void bench_cohort_finish(struct bench_cohort *b)
{
    for(uint32_t i=0;i<b->workers;++i){int status=0;pid_t p;do{p=waitpid(b->child[i].pid,&status,0);}while(p<0&&errno==EINTR);B_CHECK(p==b->child[i].pid);b->child[i].pid=0;B_CHECK(WIFEXITED(status)&&WEXITSTATUS(status)==0);B_CHECK(close(b->child[i].command_fd)==0&&close(b->child[i].result_fd)==0);}
    for(uint32_t i=0;i<b->objects;++i){B_OK(elite_object_destroy(&b->object[i]));b->first[i].name[0]='\0';B_OK(elite_authority_destroy(&b->authority[i]));}
    B_CHECK(munmap(b->control,BENCH_CTRL_BYTES)==0);b->control=NULL;B_CHECK(unlink(b->control_path)==0);b->control_path[0]='\0';active=NULL;
}
struct bench_control *bench_control_open(const char *path)
{
    int fd=open(path,O_RDWR);B_CHECK(fd>=0);struct stat s;B_CHECK(fstat(fd,&s)==0&&s.st_size==BENCH_CTRL_BYTES&&s.st_uid==geteuid()&&(s.st_mode&077)==0);
    void *p=mmap(NULL,BENCH_CTRL_BYTES,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);B_CHECK(p!=MAP_FAILED);B_CHECK(close(fd)==0);return p;
}
void bench_control_close(struct bench_control *c){B_CHECK(munmap(c,BENCH_CTRL_BYTES)==0);}
void bench_worker_command(uint64_t *command){bench_read_all(STDIN_FILENO,command,sizeof(*command));}
void bench_worker_event(struct bench_result *r,uint32_t event){r->event=event;bench_write_all(STDOUT_FILENO,r,sizeof(*r));}
void bench_usage_json(FILE *f,const struct rusage *a,const struct rusage *b)
{
    int64_t user=(int64_t)(b->ru_utime.tv_sec-a->ru_utime.tv_sec)*1000000+(int64_t)(b->ru_utime.tv_usec-a->ru_utime.tv_usec);
    int64_t system=(int64_t)(b->ru_stime.tv_sec-a->ru_stime.tv_sec)*1000000+(int64_t)(b->ru_stime.tv_usec-a->ru_stime.tv_usec);
    (void)fprintf(f,"{\"user_us\":%"PRId64",\"system_us\":%"PRId64",\"minor_faults\":%ld,\"major_faults\":%ld,\"voluntary_context_switches\":%ld,\"involuntary_context_switches\":%ld}",user,system,b->ru_minflt-a->ru_minflt,b->ru_majflt-a->ru_majflt,b->ru_nvcsw-a->ru_nvcsw,b->ru_nivcsw-a->ru_nivcsw);
}
void bench_payload_write(void *data,uint64_t id,uint64_t producer,uint64_t sequence,uint64_t seed)
{
    uint64_t *w=data,v=((id<<17)|(id>>47))^seed;
    w[0]=id;w[1]=~id;w[2]=producer;w[3]=sequence;w[4]=seed;w[5]=v;w[6]=~v;w[7]=id^UINT64_C(0x9e3779b97f4a7c15);
}
bool bench_payload_check(const void *data,uint64_t id,uint64_t producer,uint64_t sequence,uint64_t seed)
{
    const uint64_t *w=data;uint64_t v=((id<<17)|(id>>47))^seed;
    return w[0]==id&&w[1]==~id&&w[2]==producer&&w[3]==sequence&&w[4]==seed&&w[5]==v&&w[6]==~v&&w[7]==(id^UINT64_C(0x9e3779b97f4a7c15));
}
void bench_send(elite_connection *c,uint64_t id,uint64_t producer,uint64_t sequence,uint64_t seed,uint64_t *polls)
{
    elite_lease lease;elite_write_span span;elite_result r;
    for(;;){r=elite_write_reserve(c,&lease,&span);if(r.status!=ELITE_NO_CAPACITY_OBSERVED)break;++*polls;bench_relax();}
    B_CHECK(r.status==ELITE_OK&&span.capacity>=64);bench_payload_write(span.data,id,producer,sequence,seed);
    r=elite_write_commit(c,&lease,64,BENCH_TYPE,id);B_CHECK(r.status==ELITE_OK&&r.outcome==ELITE_PUBLISHED);
}
void bench_receive_exact(elite_connection *c,uint64_t id,uint64_t producer,uint64_t sequence,uint64_t seed,uint64_t *polls)
{
    elite_lease lease;elite_read_span span;elite_result r;
    for(;;){r=elite_read_borrow(c,&lease,&span);if(r.status!=ELITE_NO_DATA_OBSERVED)break;++*polls;bench_relax();}
    B_CHECK(r.status==ELITE_OK&&span.length==64&&span.message_type==BENCH_TYPE&&span.message_id==id);
    B_CHECK(bench_payload_check(span.data,id,producer,sequence,seed));r=elite_read_release(c,&lease);B_CHECK(r.status==ELITE_OK&&r.outcome==ELITE_RETURNED);
}
void bench_reconcile(const elite_grant *g,uint64_t publications,const char *directory,const char *name)
{
    /* Called only after every worker is between phases, or detached. This
     * controller is an accounted holder; never scan mutable live payloads. */
    int fd=shm_open(g->name,O_RDWR,0);B_CHECK(fd>=0);void *base=mmap(NULL,(size_t)g->segment_bytes,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);B_CHECK(base!=MAP_FAILED);
    struct elite_immutable_header info;B_OK(elite_validate_prefix(base,512,g->segment_bytes,&info));
    B_CHECK((uintptr_t)base%128==0 && info.descriptor_stride==128 && info.payload_stride%128==0);
    B_CHECK(info.descriptors_offset%128==0&&info.payloads_offset%128==0&&info.participant_stride==256);
    struct elite_slot_descriptor *d=(struct elite_slot_descriptor *)((unsigned char *)base+info.descriptors_offset);
    if(info.layout_profile==ELITE_SPSC){struct elite_spsc_ring_header *h=base;B_CHECK(atomic_load_explicit(&h->published.value,memory_order_acquire)==publications);B_CHECK(atomic_load_explicit(&h->reclaimed.value,memory_order_acquire)==publications);}
    else {
        struct elite_mpmc_ncq_header *h=base;uint64_t n=info.capacity;
        uint64_t head=atomic_load_explicit(&h->qf_head.value,memory_order_seq_cst),tail=atomic_load_explicit(&h->qf_tail.value,memory_order_seq_cst);
        B_CHECK(tail>=head&&tail-head==n && head==publications);
        B_CHECK(atomic_load_explicit(&h->qr_head.value,memory_order_seq_cst)==n+publications && atomic_load_explicit(&h->qr_tail.value,memory_order_seq_cst)==n+publications);
        B_CHECK(info.qf_entries_offset%128==0&&info.qr_entries_offset%128==0&&info.queue_entry_stride==128);
        struct elite_ncq_entry_cell *q=(struct elite_ncq_entry_cell *)((unsigned char *)base+info.qf_entries_offset);unsigned char *seen=calloc((size_t)n,1);B_CHECK(seen!=NULL);
        for(uint64_t t=head;t<tail;++t){uint64_t e=atomic_load_explicit(&q[t&(n-1)].cycle_index,memory_order_seq_cst),index=e&(n-1);B_CHECK((e&~(n-1))==(t&~(n-1))&&!seen[index]);seen[index]=1;uint64_t state=atomic_load_explicit(&d[index].status_word,memory_order_acquire);B_CHECK((state&3)==ELITE_EMPTY&&d[index].epoch==(state>>2));}
        free(seen);
    }
    for(uint64_t i=0;i<info.capacity;++i){B_CHECK((uintptr_t)&d[i]%128==0);for(unsigned j=0;j<88;++j)B_CHECK(d[i].reserved_028[j]==0);}
    FILE *f=bench_json_open(directory,name);
    fprintf(f,"{\"status\":\"PASS_WITHIN_SCOPE\",\"publications\":%"PRIu64",\"capacity\":%"PRIu64",\"segment_bytes\":%"PRIu64",\"all_tokens_returned\":true,\"designated_cell_isolation_64_128\":true,\"universal_no_false_sharing\":false",publications,info.capacity,info.segment_bytes);
    if(info.layout_profile==ELITE_SPSC){struct elite_spsc_ring_header *h=base;fprintf(f,",\"published\":%"PRIu64",\"reclaimed\":%"PRIu64,atomic_load_explicit(&h->published.value,memory_order_acquire),atomic_load_explicit(&h->reclaimed.value,memory_order_acquire));}
    else{struct elite_mpmc_ncq_header *h=base;uint64_t n=info.capacity,head=atomic_load_explicit(&h->qf_head.value,memory_order_seq_cst),tail=atomic_load_explicit(&h->qf_tail.value,memory_order_seq_cst);fprintf(f,",\"qf_head\":%"PRIu64",\"qf_tail\":%"PRIu64",\"qr_head\":%"PRIu64",\"qr_tail\":%"PRIu64",\"free_entries\":[",head,tail,atomic_load_explicit(&h->qr_head.value,memory_order_seq_cst),atomic_load_explicit(&h->qr_tail.value,memory_order_seq_cst));
        struct elite_ncq_entry_cell *q=(struct elite_ncq_entry_cell *)((unsigned char *)base+info.qf_entries_offset);
        for(uint64_t t=head;t<tail;++t){uint64_t e=atomic_load_explicit(&q[t&(n-1)].cycle_index,memory_order_seq_cst),index=e&(n-1);fprintf(f,"%s{\"ticket\":%"PRIu64",\"entry\":%"PRIu64",\"block\":%"PRIu64",\"status_word\":%"PRIu64",\"epoch\":%"PRIu64"}",t==head?"":",",t,e,index,atomic_load_explicit(&d[index].status_word,memory_order_acquire),d[index].epoch);}fputc(']',f);
    }
    fputs("}\n",f);bench_json_close(f);
    B_CHECK(munmap(base,(size_t)g->segment_bytes)==0&&close(fd)==0);
}

/* Turn 11: JSON strings and raw clock metadata, never a guessed CPU frequency. */
bool bench_cancelled(void) { return interrupted!=0; }
void bench_json_string(FILE *f,const char *s)
{
    (void)fputc('"',f);
    for(const unsigned char *p=(const unsigned char *)s;*p;++p) {
        if(*p=='"' || *p=='\\'){(void)fputc('\\',f);(void)fputc(*p,f);}
        else if(*p<32 || *p>=127)(void)fprintf(f,"\\u%04x",(unsigned)*p);
        else (void)fputc(*p,f);
    }
    (void)fputc('"',f);
}
void bench_clock_json(FILE *f,const struct bench_clock *c)
{
#ifdef __APPLE__
    (void)fprintf(f,"\"clock\":\"mach_absolute_time\",\"platform\":\"Darwin\",\"timebase_source\":\"mach_timebase_info\",\"reported_resolution_ns\":null,\"resolution_status\":\"NOT_REPORTED_FOR_THIS_CLOCK\",");
#else
    (void)fprintf(f,"\"clock\":\"CLOCK_MONOTONIC_RAW\",\"platform\":\"Linux\",\"timebase_source\":\"nanosecond_OS_clock\",\"reported_resolution_ns\":%"PRIu64",\"resolution_status\":\"clock_getres_NOT_effective_resolution\",",c->reported_resolution_ns);
#endif
    (void)fprintf(f,"\"timebase_numer\":%"PRIu32",\"timebase_denom\":%"PRIu32",\"nominal_tick_ns\":%.17g",c->numer,c->denom,(double)c->numer/(double)c->denom);
}
