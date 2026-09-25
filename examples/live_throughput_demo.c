/* Interactive request/reply demonstration, NOT a release latency certificate.
 * Two independent POSIX rings; one exec'd responder; one outstanding RTT.
 * All 64 payload bytes and sequential IDs are checked in both directions.
 * Timings include the public API, payload work and ordered timer reads.
 * Console/JSON work occurs between windows (and can perturb later windows).
 */
#include "elite_api.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach/mach_time.h>
#endif
extern char **environ;
#define BILLION UINT64_C(1000000000)
#define STOP_TYPE UINT32_MAX
#define DATA_TYPE UINT32_C(16)
#define SEED UINT64_C(0xa16547c33167bc09)
struct demo_config { elite_grant input,output; unsigned timeout; };
struct demo_done { uint64_t count; elite_cleanup_receipt input,output; };
static volatile sig_atomic_t interrupted=0;
static pid_t child_pid=0;
static char names[2][ELITE_NAME_BYTES];
static elite_authority *managers[2];
static elite_object *objects[2];
static uint32_t numer=1,denom=1;
static uint64_t deadline=0;
static void on_signal(int signo) { interrupted=signo; }
static void cleanup(void)
{
    if(child_pid>0) {
        (void)kill(child_pid,SIGKILL);
        while(waitpid(child_pid,NULL,0)<0 && errno==EINTR) { }
        child_pid=0;
    }
    for(unsigned i=0;i<2;++i) {
        if(objects[i]) (void)elite_object_destroy(&objects[i]);
        /* Only this process's exclusively created names, after killing/reaping
         * its sole registered child. Failure is NOT emitted as COMPLETE. */
        if(names[i][0]) (void)shm_unlink(names[i]);
        if(managers[i]) (void)elite_authority_destroy(&managers[i]);
    }
}
static void fail(const char *what)
{ (void)fprintf(stderr,"demo FAILED: %s (errno=%d)\n",what,errno); exit(1); }
static void check(bool condition,const char *what) { if(!condition) fail(what); }
static void ok(elite_result r,const char *what)
{
    if(r.status!=ELITE_OK) {
        (void)fprintf(stderr,"%s: %s outcome=%u os_error=%d\n",what,elite_status_string(r.status),r.outcome,r.os_error);
        fail("native call");
    }
}
static void clock_init(void)
{
#ifdef __APPLE__
    mach_timebase_info_data_t t;
    check(mach_timebase_info(&t)==KERN_SUCCESS && t.numer && t.denom,"timebase");
    numer=t.numer;denom=t.denom;
#endif
}
static uint64_t tick(void)
{
    atomic_signal_fence(memory_order_seq_cst);
#if defined(__aarch64__)
    __asm__ __volatile__("dsb ishld\n\tisb" ::: "memory");
#elif defined(__x86_64__)
    __asm__ __volatile__("lfence" ::: "memory");
#endif
#ifdef __APPLE__
    uint64_t v=mach_absolute_time();
#else
    struct timespec t;
#ifdef __linux__
    check(clock_gettime(CLOCK_MONOTONIC_RAW,&t)==0,"clock_gettime");
#else
    check(clock_gettime(CLOCK_MONOTONIC,&t)==0,"clock_gettime");
#endif
    check(t.tv_sec>=0 && (uint64_t)t.tv_sec<UINT64_MAX/BILLION,"clock range");
    uint64_t v=(uint64_t)t.tv_sec*BILLION+(uint64_t)t.tv_nsec;
#endif
#if defined(__aarch64__)
    __asm__ __volatile__("isb" ::: "memory");
#elif defined(__x86_64__)
    __asm__ __volatile__("lfence" ::: "memory");
#endif
    atomic_signal_fence(memory_order_seq_cst);
    return v;
}
static uint64_t ns_ceil(uint64_t dt)
{
    uint64_t q=dt/denom,r=dt%denom,low=(r*numer+denom-1)/denom;
    check(q<=(UINT64_MAX-low)/numer,"duration range");
    return q*numer+low;
}
static void set_deadline(unsigned seconds)
{
    uint64_t ns=(uint64_t)seconds*BILLION;
    uint64_t q=ns/numer,r=ns%numer,low=(r*denom+numer-1)/numer;
    check(q<=(UINT64_MAX-low)/denom,"timeout scale overflow");
    uint64_t dt=q*denom+low,t=tick();
    check(dt<=UINT64_MAX-t,"deadline overflow");deadline=t+dt;
}
static void guard(void) { check(tick()<deadline,"cooperative watchdog expired"); }
static void transfer(int fd,void *data,size_t bytes,bool writing)
{
    unsigned char *p=data;
    while(bytes) {
        guard();
        struct pollfd pollfd={fd,writing?POLLOUT:POLLIN,0};
        int rc=poll(&pollfd,1,100);
        if(rc<0 && errno==EINTR) continue;
        check(rc>=0,"pipe poll");
        if(rc==0) continue;
        ssize_t n=writing?write(fd,p,bytes):read(fd,p,bytes);
        if(n<0 && errno==EINTR) continue;
        check(n>0,"control pipe closed or failed");
        p+=(size_t)n;bytes-=(size_t)n;
    }
}
static void payload_write(void *p,uint64_t id,uint64_t seed)
{
    uint64_t *v=p;
    for(unsigned i=0;i<8;++i) v[i]=id ^ (seed+UINT64_C(0x9e3779b97f4a7c15)*i);
}
static void payload_check(const elite_read_span *s,uint64_t id,uint64_t seed)
{
    check(s->length==64 && s->message_type==DATA_TYPE && s->message_id==id,"record metadata");
    const uint64_t *v=s->data;
    for(unsigned i=0;i<8;++i) check(v[i]==(id^(seed+UINT64_C(0x9e3779b97f4a7c15)*i)),"payload mismatch");
}
static void send_record(elite_connection *c,uint64_t id,uint64_t seed,bool stop)
{
    elite_lease l;elite_write_span s;elite_result r;unsigned spins=0;
    for(;;) {
        r=elite_write_reserve(c,&l,&s);
        if(r.status!=ELITE_NO_CAPACITY_OBSERVED) break;
        if(++spins%1024u==0) guard();
    }
    ok(r,"reserve");
    if(!stop) payload_write(s.data,id,seed);
    r=elite_write_commit(c,&l,stop?0u:64u,stop?STOP_TYPE:DATA_TYPE,id);
    ok(r,"commit");check(r.outcome==ELITE_PUBLISHED,"publication outcome");
}
static bool read_record(elite_connection *c,uint64_t id,uint64_t seed,bool allow_stop)
{
    elite_lease l;elite_read_span s;elite_result r;unsigned spins=0;
    for(;;) {
        r=elite_read_borrow(c,&l,&s);
        if(r.status!=ELITE_NO_DATA_OBSERVED) break;
        if(++spins%1024u==0) guard();
    }
    ok(r,"borrow");
    bool stop=s.message_type==STOP_TYPE;
    if(stop) check(allow_stop && s.length==0 && s.message_id==id,"stop record");
    else payload_check(&s,id,seed);
    r=elite_read_release(c,&l);ok(r,"release");check(r.outcome==ELITE_RETURNED,"return outcome");
    return stop;
}
static int worker(void)
{
    clock_init();set_deadline(3600);
    struct demo_config cfg;transfer(STDIN_FILENO,&cfg,sizeof(cfg),false);
    check(cfg.timeout>=1 && cfg.timeout<=3600,"worker timeout");set_deadline(cfg.timeout);
    elite_connection *in=NULL,*out=NULL;
    ok(elite_attach(&cfg.input,&in),"child input attach");
    ok(elite_attach(&cfg.output,&out),"child output attach");
    unsigned char ready=1;transfer(STDOUT_FILENO,&ready,1,true);
    uint64_t id=0;
    for(;;) {
        if(read_record(in,id,SEED,true)) break;
        send_record(out,id,~SEED,false);++id;
    }
    struct demo_done done;memset(&done,0,sizeof(done));done.count=id;
    ok(elite_detach(&in,&done.input),"child input detach");
    ok(elite_detach(&out,&done.output),"child output detach");
    transfer(STDOUT_FILENO,&done,sizeof(done),true);return 0;
}
static int compare_u64(const void *a,const void *b)
{ uint64_t x=*(const uint64_t *)a,y=*(const uint64_t *)b;return (x>y)-(x<y); }
static uint64_t number(const char *s,uint64_t limit)
{
    char *end=NULL;errno=0;
    check(s[0]>='0'&&s[0]<='9',"positive integer expected");
    unsigned long long x=strtoull(s,&end,10);
    check(!errno && end && !*end && x>0 && x<=limit,"argument range");return (uint64_t)x;
}
int main(int argc,char **argv)
{
    struct sigaction action;memset(&action,0,sizeof(action));action.sa_handler=on_signal;
    check(sigemptyset(&action.sa_mask)==0,"signal mask");
    check(sigaction(SIGINT,&action,NULL)==0 && sigaction(SIGTERM,&action,NULL)==0,"signal setup");
    action.sa_handler=SIG_IGN;check(sigaction(SIGPIPE,&action,NULL)==0,"SIGPIPE setup");
    if(argc==2 && strcmp(argv[1],"--worker")==0) return worker();
    uint32_t mode=ELITE_SPSC;unsigned windows=20,timeout=120;size_t batch=100000;
    bool plain=!isatty(STDOUT_FILENO);const char *jsonpath=NULL;
    for(int i=1;i<argc;++i) {
        if(strcmp(argv[i],"--help")==0) {
            (void)puts("usage: live_throughput_demo [--mode spsc|ncq] [--windows 1..10000] [--messages 1..1000000] [--timeout 1..3600] [--plain] [--json NEW_FILE]\nMetrics: checked directional Mmsg/s; exact window p50/p99 RTT ns. One outstanding request. q+Enter or Ctrl-C ends at a completed RTT. NOT a one-way certificate.");return 0;
        }
        if(strcmp(argv[i],"--plain")==0) { plain=true;continue; }
        check(i+1<argc,"missing argument");const char *key=argv[i],*value=argv[++i];
        if(strcmp(key,"--mode")==0) {check(strcmp(value,"spsc")==0||strcmp(value,"ncq")==0,"mode");mode=strcmp(value,"spsc")==0?ELITE_SPSC:ELITE_NCQ;}
        else if(strcmp(key,"--windows")==0) windows=(unsigned)number(value,10000);
        else if(strcmp(key,"--messages")==0) batch=(size_t)number(value,1000000);
        else if(strcmp(key,"--timeout")==0) timeout=(unsigned)number(value,3600);
        else if(strcmp(key,"--json")==0) jsonpath=value;
        else fail("unknown option");
    }
    clock_init();set_deadline(timeout);
    check(atexit(cleanup)==0,"atexit");
    FILE *json=jsonpath?fopen(jsonpath,"wx"):NULL;check(!jsonpath || json!=NULL,"new JSON output");
    uint64_t *raw=calloc(batch,sizeof(*raw)),*sorted=calloc(batch,sizeof(*sorted)),*starts=calloc(batch,sizeof(*starts));
    check(raw && sorted && starts,"sample allocation");
    uint8_t ids[3][16];int random=open("/dev/urandom",O_RDONLY|O_CLOEXEC);check(random>=0,"random identity source");
    size_t off=0;while(off<sizeof(ids)){ssize_t n=read(random,(unsigned char *)ids+off,sizeof(ids)-off);if(n<0&&errno==EINTR)continue;check(n>0,"random read");off+=(size_t)n;}check(close(random)==0,"random close");
    elite_authority **a=managers;elite_object **o=objects;
    elite_config cfg={mode,ELITE_POLL_ONLY,ELITE_CHECKSUM_NONE,64,1024,1,1,0};
    elite_endpoint_definition defs[2]={{{1},{3},ELITE_PRODUCER},{{2},{4},ELITE_CONSUMER}};
    for(unsigned j=0;j<2;++j){ok(elite_authority_create(ids[0],ids[j+1],1048576,&a[j]),"authority");ok(elite_create(a[j],&cfg,defs,&o[j]),"create");ok(elite_object_activate(o[j]),"activate");}
    int to[2],from[2];check(pipe(to)==0 && pipe(from)==0,"pipes");
    for(unsigned j=0;j<2;++j){check(fcntl(to[j],F_SETFD,FD_CLOEXEC)==0,"cloexec");check(fcntl(from[j],F_SETFD,FD_CLOEXEC)==0,"cloexec");}
    posix_spawn_file_actions_t actions;check(posix_spawn_file_actions_init(&actions)==0,"spawn init");
    check(posix_spawn_file_actions_adddup2(&actions,to[0],STDIN_FILENO)==0,"spawn input");
    check(posix_spawn_file_actions_adddup2(&actions,from[1],STDOUT_FILENO)==0,"spawn output");
    char *args[]={argv[0],(char *)"--worker",NULL};
    check(posix_spawnp(&child_pid,argv[0],&actions,NULL,args,environ)==0,"spawn child");
    check(posix_spawn_file_actions_destroy(&actions)==0,"spawn actions destroy");
    check(close(to[0])==0 && close(from[1])==0,"parent pipe close");
    elite_grant grants[2][2];
    for(unsigned j=0;j<2;++j) for(unsigned k=0;k<2;++k){
        pid_t pid=(j==k)?getpid():child_pid;
        ok(elite_object_register_process(o[j],k,pid),"register");ok(elite_object_grant(o[j],k,&grants[j][k]),"grant");
        memcpy(names[j],grants[j][k].name,ELITE_NAME_BYTES);
    }
    struct demo_config wc={grants[0][1],grants[1][0],timeout};transfer(to[1],&wc,sizeof(wc),true);
    elite_connection *p=NULL,*c=NULL;ok(elite_attach(&grants[0][0],&p),"origin output attach");ok(elite_attach(&grants[1][1],&c),"origin input attach");
    unsigned char ready=0;transfer(from[0],&ready,1,false);check(ready==1,"child ready");
    const char *label=mode==ELITE_SPSC?"SPSC":"NCQ-SC64";
    (void)printf("ELITEIPC %s | %s | two processes / two POSIX rings / 64B\nDirectional rate counts 2 records per RTT. p50/p99 are RAW RTT, not one-way.\nNo warmup, no CPU pinning, no latency certification. q+Enter or Ctrl-C to stop.\n",elite_version_string(),label);
    if(json)check(fprintf(json,"{\"schema\":\"elite-live-demo-v1\",\"mode\":\"%s\",\"version\":\"1.1.0\",\"timebase_numer\":%u,\"timebase_denom\":%u,\"metric\":\"RTT\",\"bytes_per_direction\":64,\"origin_pid\":%ld,\"responder_pid\":%ld,\"planned_windows\":%u,\"messages_per_window\":%zu}\n",label,numer,denom,(long)getpid(),(long)child_pid,windows,batch)>0,"JSON header");
    uint64_t total=0;unsigned completed=0;
    for(unsigned w=0;w<windows && !interrupted;++w) {
        guard();size_t n=0;
        uint64_t begin=tick(),end=begin;
        for(;n<batch && !interrupted;++n){
            starts[n]=tick();send_record(p,total,SEED,false);(void)read_record(c,total,~SEED,false);
            end=tick();check(end>=starts[n],"nonmonotonic clock");raw[n]=end-starts[n];++total;
        }
        if(n==0) break;
        check(end>begin,"zero window duration");
        memcpy(sorted,raw,n*sizeof(*raw));qsort(sorted,n,sizeof(*sorted),compare_u64);
        uint64_t p50=ns_ceil(sorted[(n+1)/2-1]),p99=ns_ceil(sorted[(99*n+99)/100-1]);
        double rate=(2.0*(double)n*1e9*(double)denom)/((double)(end-begin)*(double)numer);
        char bar[33];unsigned filled=(unsigned)(((uint64_t)w+1)*32/windows);
        for(unsigned k=0;k<32;++k) { bar[k]=k<filled?'#':'.'; }
        bar[32]='\0';
        (void)printf("%s[%s] %3u/%u  %7.3f Mmsg/s  p50=%"PRIu64" ns  p99=%"PRIu64" ns RTT  checked=%"PRIu64"%s",plain?"":"\r\033[K",bar,w+1,windows,rate/1e6,p50,p99,total,plain?"\n":"");
        check(fflush(stdout)==0,"console write");
        if(json){
            check(fprintf(json,"{\"window\":%u,\"roundtrips\":%zu,\"start_tick\":%"PRIu64",\"end_tick\":%"PRIu64",\"directional_mps\":%.17g,\"p50_rtt_ns\":%"PRIu64",\"p99_rtt_ns\":%"PRIu64",\"samples\":[",w,n,begin,end,rate,p50,p99)>0,"JSON window");
            for(size_t k=0;k<n;++k)check(fprintf(json,"%s[%"PRIu64",%"PRIu64"]",k?",":"",starts[k],raw[k])>0,"JSON sample");
            check(fputs("]}\n",json)>=0 && fflush(json)==0,"JSON flush");
        }
        ++completed;
        if(isatty(STDIN_FILENO)){struct pollfd key={STDIN_FILENO,POLLIN,0};if(poll(&key,1,0)>0){char ch=0;if(read(STDIN_FILENO,&ch,1)==1 && (ch=='q'||ch=='Q'))interrupted=SIGINT;}}
    }
    send_record(p,total,0,true);
    struct demo_done done;transfer(from[0],&done,sizeof(done),false);check(done.count==total,"child final quota");
    ok(elite_object_ack_cleanup(o[0],&done.input),"child input cleanup");ok(elite_object_ack_cleanup(o[1],&done.output),"child output cleanup");
    elite_cleanup_receipt pr,cr;ok(elite_detach(&p,&pr),"origin output detach");ok(elite_detach(&c,&cr),"origin input detach");
    ok(elite_object_ack_cleanup(o[0],&pr),"origin output cleanup");ok(elite_object_ack_cleanup(o[1],&cr),"origin input cleanup");
    int status=0;pid_t waited;do{waited=waitpid(child_pid,&status,0);}while(waited<0&&errno==EINTR);
    check(waited==child_pid,"child wait");child_pid=0;check(WIFEXITED(status)&&WEXITSTATUS(status)==0,"child exit");
    check(close(to[1])==0 && close(from[0])==0,"pipe close");
    for(unsigned j=0;j<2;++j){ok(elite_object_destroy(&o[j]),"destroy");names[j][0]='\0';ok(elite_authority_destroy(&a[j]),"authority destroy");}
    if(!plain)(void)putchar('\n');
    (void)printf("%s roundtrips=%"PRIu64" directional_records=%"PRIu64" checked_windows=%u leases_ended=yes objects_destroyed=yes\n",interrupted?"CANCELLED_CLEAN":"COMPLETE",total,2*total,completed);
    if(json){check(fprintf(json,"{\"status\":\"%s\",\"roundtrips\":%"PRIu64",\"directional_records\":%"PRIu64",\"windows\":%u,\"child_checked\":%"PRIu64",\"leases_ended\":true,\"objects_destroyed\":true}\n",interrupted?"CANCELLED_CLEAN":"COMPLETE",total,2*total,completed,done.count)>0,"JSON final");check(fclose(json)==0,"JSON close");}
    free(raw);free(sorted);free(starts);return interrupted?130:0;
}
