/* Cache-line interference CONTROL, not an IPC throughput/latency benchmark.
 * v2 preserves the denominator, exact counters, binary and per-thread evidence.
 * CPU PMCs count only in the optional diagnostic run; CNTVCT is only a timer. */
#include "bench_common.h"
#include "bench_identity.h"
#include "bench_ratio.h"
#include "elite_pmc.h"
#include <pthread.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <time.h>
#ifdef __APPLE__
#include <sys/wait.h>
#endif

struct virtual_probe {
    const char *status;
    int error, signal_number;
    uint64_t cntfrq_hz, sysctl_hz;
    int sysctl_error;
    elite_virtual_sample first,last;
};
struct cache_worker {
    _Alignas(128) _Atomic uint64_t *counter;
    uint64_t iterations,start_tick,end_tick,window_start,window_end,final_value;
    _Atomic uint64_t *start;
    _Atomic uint32_t *ready;
    int cpu,tag,qos;
    bool pmc_enabled,virtual_enabled;
    struct bench_placement placement;
    elite_pmc_result pmc;
    elite_cpu_snapshot usage_before,usage_after;
    elite_virtual_sample timer_before,timer_after;
};
struct watchdog { _Atomic bool done; uint64_t deadline; };
static void *watch_run(void *argument)
{
    struct watchdog *w=argument;
    while(!atomic_load_explicit(&w->done,memory_order_acquire)) {
        B_CHECK(!bench_cancelled() && bench_watch_ns()<w->deadline);
        struct timespec delay={0,10000000};(void)nanosleep(&delay,NULL);
    }
    return NULL;
}
static struct virtual_probe virtual_probe_run(bool requested)
{
    struct virtual_probe p;memset(&p,0,sizeof(p));p.status="NOT_REQUESTED";
    if(!requested)return p;
#if defined(__APPLE__) && defined(__aarch64__)
    /* Standalone benchmark preflight BEFORE any pthread is created. No signal
     * recovery inside the measuring thread. Only the owned child is killed. */
    int fds[2];B_CHECK(pipe(fds)==0);pid_t child=fork();B_CHECK(child>=0);
    if(child==0){
        (void)close(fds[0]);struct virtual_probe v;memset(&v,0,sizeof(v));
        if(elite_pmc_sysctl_tbfrequency(&v.sysctl_hz)!=0)v.sysctl_error=errno;
        if(elite_pmc_virtual_frequency(&v.cntfrq_hz)!=0 || elite_pmc_virtual_read(&v.first)!=0)_exit(2);
        struct timespec pause={0,10000000};(void)nanosleep(&pause,NULL);
        if(elite_pmc_virtual_read(&v.last)!=0)_exit(3);
        ssize_t written=write(fds[1],&v,sizeof(v));_exit(written==(ssize_t)sizeof(v)?0:4);
    }
    (void)close(fds[1]);int status=0;uint64_t deadline=bench_watch_ns()+UINT64_C(5000000000);
    for(;;){pid_t got=waitpid(child,&status,WNOHANG);if(got==child)break;B_CHECK(got>=0 || errno==EINTR);
        if(bench_watch_ns()>=deadline){(void)kill(child,SIGKILL);while(waitpid(child,&status,0)<0&&errno==EINTR){}p.error=ETIMEDOUT;break;}
        struct timespec delay={0,1000000};(void)nanosleep(&delay,NULL);}
    if(WIFEXITED(status)&&WEXITSTATUS(status)==0){struct virtual_probe v;ssize_t n=read(fds[0],&v,sizeof(v));
        if(n==(ssize_t)sizeof(v)&&v.cntfrq_hz!=0&&v.last.count>v.first.count){p=v;p.status="PROBE_SUCCEEDED_TIMER_ONLY";}
        else{p.status="PROBE_INVALID";p.error=EPROTO;}}
    else{p.status="PROBE_FAILED";if(WIFSIGNALED(status))p.signal_number=WTERMSIG(status);}
    (void)close(fds[0]);
#else
    p.status="UNSUPPORTED_TARGET";p.error=ENOTSUP;
#endif
    return p;
}
static void *run_counter(void *argument)
{
    struct cache_worker *w=argument;
    bench_placement_apply(w->cpu,w->tag,w->qos,&w->placement);
    elite_pmc_context *pmc=elite_pmc_create(w->pmc_enabled);B_CHECK(pmc!=NULL);
    (void)atomic_fetch_add_explicit(w->ready,1,memory_order_release);
    uint64_t start;
    do{start=atomic_load_explicit(w->start,memory_order_acquire);bench_relax();}while(start==0);
    bench_wait_start(start);
    w->window_start=bench_ordered_tick();
    (void)elite_cpu_thread_snapshot(&w->usage_before);
    if(w->virtual_enabled)B_CHECK(elite_pmc_virtual_read(&w->timer_before)==0);
    B_CHECK(elite_pmc_start(pmc)==0);
    w->start_tick=bench_ordered_tick();
    for(uint64_t i=0;i<w->iterations;++i)
        (void)atomic_fetch_add_explicit(w->counter,1,memory_order_relaxed);
    w->end_tick=bench_ordered_tick();
    B_CHECK(elite_pmc_stop(pmc)==0);
    if(w->virtual_enabled)B_CHECK(elite_pmc_virtual_read(&w->timer_after)==0);
    (void)elite_cpu_thread_snapshot(&w->usage_after);
    w->window_end=bench_ordered_tick();
    w->pmc=*elite_pmc_get_result(pmc);elite_pmc_destroy(pmc);
    w->final_value=atomic_load_explicit(w->counter,memory_order_relaxed);
    bench_placement_finish(&w->placement);
    return NULL;
}
static void usage_json(FILE *f,const elite_cpu_snapshot *s)
{
    (void)fprintf(f,"{\"error\":%d,\"user_us\":%"PRIu64",\"system_us\":%"PRIu64",\"minor_faults\":%"PRId64",\"major_faults\":%"PRId64",\"voluntary_switches\":%"PRId64",\"involuntary_switches\":%"PRId64",\"instant_usage\":%d,\"instant_usage_scale\":%d}",s->error,s->user_us,s->system_us,s->minor_faults,s->major_faults,s->voluntary_switches,s->involuntary_switches,s->instant_usage,s->instant_usage_scale);
}
static void timer_json(FILE *f,const elite_virtual_sample *s)
{(void)fprintf(f,"{\"mach_before\":%"PRIu64",\"count\":%"PRIu64",\"mach_after\":%"PRIu64"}",s->mach_before,s->count,s->mach_after);}
static void frame_json(FILE *f,bool valid,const uint64_t *v)
{
    if(!valid){(void)fputs("null",f);return;}
    (void)fputc('[',f);for(unsigned i=0;i<ELITE_PMC_FRAME_WORDS;++i)(void)fprintf(f,"%s%"PRIu64,i?",":"",v[i]);(void)fputc(']',f);
}
static bool measured(const elite_pmc_group_result *r)
{return r->status==ELITE_PMC_OK || r->status==ELITE_PMC_MULTIPLEXED;}
static long double scaled(const elite_pmc_group_result *r,unsigned j)
{return (long double)r->raw_delta[j]*(long double)r->time_enabled_ns/(long double)r->time_running_ns;}
static void pmc_json(FILE *f,const elite_pmc_result *p)
{
    (void)fprintf(f,"{\"requested\":%s,\"owner_thread_id\":%"PRIu64",\"paranoid\":",p->requested?"true":"false",p->owner_thread_id);
    if(p->paranoid==-999)(void)fputs("null",f);else(void)fprintf(f,"%d",p->paranoid);
    (void)fputs(",\"scope\":\"calling_thread_user_only_no_inherit\",\"sampling\":false,\"access_denial_cause\":\"errno_and_paranoid_are_evidence_not_unique_cause\",\"groups\":[",f);
    for(unsigned g=0;g<ELITE_PMC_GROUPS;++g){const elite_pmc_group_result *r=&p->groups[g];
        (void)fprintf(f,"%s{\"group_id\":%u,\"status\":\"%s\",\"os_error\":%d,\"before\":",g?",":"",g,elite_pmc_status_name(r->status),r->os_error);frame_json(f,r->before_valid,r->before);
        (void)fputs(",\"after\":",f);frame_json(f,r->after_valid,r->after);
        (void)fprintf(f,",\"time_enabled_ns\":%"PRIu64",\"time_running_ns\":%"PRIu64",\"events\":[",r->time_enabled_ns,r->time_running_ns);
        for(unsigned j=0;j<2;++j){(void)fprintf(f,"%s{\"name\":\"%s\",\"type\":%u,\"config\":%"PRIu64",\"id\":%"PRIu64",\"raw_delta\":",j?",":"",elite_pmc_event_name(2*g+j),r->type[j],r->config[j],r->ids[j]);
            if(measured(r)){(void)fprintf(f,"%"PRIu64",\"scaled_estimate\":",r->raw_delta[j]);
                B_CHECK(bench_json_ratio(f,r->raw_delta[j],r->time_enabled_ns,r->time_running_ns,1)==0);}
            else(void)fputs("null,\"scaled_estimate\":null",f);
            (void)fputc('}',f);}
        (void)fputs("]}",f);}
    (void)fputs("]}",f);
}
static void metrics_json(FILE *f,const struct cache_worker *w,uint32_t n,uint64_t operations)
{
    long double total[6]={0};bool valid[3]={true,true,true};
    for(uint32_t i=0;i<n;++i)for(unsigned g=0;g<3;++g){const elite_pmc_group_result *r=&w[i].pmc.groups[g];
        if(!measured(r))valid[g]=false;else for(unsigned j=0;j<2;++j)total[2*g+j]+=scaled(r,j);}
    (void)fputs("{\"instructions_per_cycle\":",f);
    if(valid[0]&&total[0]>0)(void)fprintf(f,"%.17g",(double)(total[1]/total[0]));else(void)fputs("null",f);
    (void)fputs(",\"l1d_read_misses_per_million_rmw\":",f);
    if(valid[1])(void)fprintf(f,"%.17g",(double)(total[3]*1000000.0L/(long double)operations));else(void)fputs("null",f);
    (void)fputs(",\"llc_read_misses_per_million_rmw\":",f);
    if(valid[2])(void)fprintf(f,"%.17g",(double)(total[5]*1000000.0L/(long double)operations));else(void)fputs("null",f);
    (void)fputs(",\"l1d_read_miss_fraction\":",f);
    if(valid[1]&&total[2]>0)(void)fprintf(f,"%.17g",(double)(total[3]/total[2]));else(void)fputs("null",f);
    (void)fputs(",\"llc_read_miss_fraction\":",f);
    if(valid[2]&&total[4]>0)(void)fprintf(f,"%.17g",(double)(total[5]/total[4]));else(void)fputs("null",f);
    (void)fputs(",\"coherence_invalidations\":null,\"coherence_status\":\"UNSUPPORTED_GENERIC_EVENT\",\"causal_attribution\":\"NOT_ESTABLISHED\"}",f);
}
static const char *overall_pmc(const struct cache_worker *w,uint32_t n,bool enabled)
{
    if(!enabled)return "NOT_REQUESTED";
    unsigned ok=0;elite_pmc_status first=w[0].pmc.groups[0].status;bool same=true;
    for(uint32_t i=0;i<n;++i)for(unsigned g=0;g<3;++g){if(measured(&w[i].pmc.groups[g]))++ok;if(w[i].pmc.groups[g].status!=first)same=false;}
    if(ok==n*3u)return "COLLECTED";
    if(ok!=0)return "PARTIAL";
    return same?elite_pmc_status_name(first):"UNAVAILABLE_MIXED";
}
static void run_layout(const struct bench_options *o,size_t stride,uint32_t trial,bool enabled,
                       const struct virtual_probe *probe,const char *binary,char hash[65],uint64_t binary_bytes)
{
    uint32_t workers=o->producers;void *storage=NULL,*records=NULL;
    B_CHECK(posix_memalign(&storage,128,(size_t)workers*128)==0);
    B_CHECK(posix_memalign(&records,128,(size_t)workers*sizeof(struct cache_worker))==0);
    memset(storage,0,(size_t)workers*128);memset(records,0,(size_t)workers*sizeof(struct cache_worker));
    struct cache_worker *w=records;pthread_t threads[BENCH_MAX_WORKERS];
    _Alignas(128) _Atomic uint64_t start;_Alignas(128) _Atomic uint32_t ready;atomic_init(&start,0);atomic_init(&ready,0);
    struct bench_clock clock;bench_clock_init(&clock);
    struct watchdog wd;atomic_init(&wd.done,false);wd.deadline=bench_watch_ns()+(uint64_t)o->timeout_seconds*UINT64_C(1000000000);
    pthread_t watcher;B_CHECK(pthread_create(&watcher,NULL,watch_run,&wd)==0);
    for(uint32_t i=0;i<workers;++i){
        w[i].counter=(_Atomic uint64_t *)((unsigned char *)storage+(size_t)i*stride);atomic_init(w[i].counter,0);B_CHECK(atomic_is_lock_free(w[i].counter));
        w[i].iterations=o->count;w[i].start=&start;w[i].ready=&ready;w[i].cpu=o->cpu_count?o->cpus[i%o->cpu_count]:-1;w[i].tag=o->affinity_tag;w[i].qos=o->qos;w[i].pmc_enabled=enabled;
        w[i].virtual_enabled=strcmp(probe->status,"PROBE_SUCCEEDED_TIMER_ONLY")==0;
        B_CHECK(pthread_create(&threads[i],NULL,run_counter,&w[i])==0);
    }
    while(atomic_load_explicit(&ready,memory_order_acquire)!=workers)bench_relax();
    elite_cpu_snapshot process_before,process_after;elite_process_counts counts_before,counts_after;
    uint64_t process_start=bench_ordered_tick();(void)elite_cpu_process_snapshot(&process_before);
    (void)elite_pmc_process_counts(&counts_before);
    uint64_t t0=bench_future_tick(&clock,100000000);atomic_store_explicit(&start,t0,memory_order_release);
    uint64_t end=0;
    for(uint32_t i=0;i<workers;++i){B_CHECK(pthread_join(threads[i],NULL)==0);B_CHECK(w[i].final_value==o->count);
        B_CHECK(w[i].window_start>=t0&&w[i].start_tick>=w[i].window_start&&w[i].end_tick>=w[i].start_tick&&w[i].window_end>=w[i].end_tick);
        if(w[i].end_tick>end)end=w[i].end_tick;}
    (void)elite_pmc_process_counts(&counts_after);(void)elite_cpu_process_snapshot(&process_after);uint64_t process_end=bench_ordered_tick();
    atomic_store_explicit(&wd.done,true,memory_order_release);B_CHECK(pthread_join(watcher,NULL)==0);
    char final_hash[65];uint64_t final_bytes;B_CHECK(bench_sha256_file(binary,final_hash,&final_bytes)==0&&strcmp(final_hash,hash)==0&&final_bytes==binary_bytes);
    B_CHECK(end>t0&&o->count<=UINT64_MAX/workers);uint64_t delta=end-t0,ops=o->count*workers;
    long double ns=bench_ns(&clock,delta);double elapsed=(double)ns;double rate=(double)((long double)ops*1000000000.0L/ns);
    B_CHECK(isfinite(elapsed)&&elapsed>0&&isfinite(rate));
    char name[96];(void)snprintf(name,sizeof(name),"cache-%u-workers-stride-%zu-trial-%u.json",workers,stride,trial);
    FILE *f=bench_json_open(o->output,name);
    (void)fputs("{\"hardware_topology\":",f);bench_hardware_topology_json(f);fprintf(f,",\"schema\":\"elite-cache-control-v2\",\"status\":\"PASS_COUNTER_RECONCILIATION_ONLY\",\"trial\":%u,\"layout_order\":%u,\"workers\":%u,\"counter_stride\":%zu,\"allocation_alignment\":128,\"iterations_per_worker\":%"PRIu64",\"total_operations\":%"PRIu64",\"t0\":%"PRIu64",\"end\":%"PRIu64",\"delta_ticks\":%"PRIu64",",trial,(unsigned)((trial%2==0)==(stride==8)?0:1),workers,stride,o->count,ops,t0,end,delta);
    bench_clock_json(f,&clock);
    (void)fputs(",\"elapsed_ns\":",f);B_CHECK(bench_json_ratio(f,delta,clock.numer,clock.denom,1)==0);
    (void)fputs(",\"operations_per_second\":",f);B_CHECK(ops<=UINT64_MAX/UINT64_C(1000000000));
    B_CHECK(bench_json_ratio(f,ops*UINT64_C(1000000000),clock.denom,delta,clock.numer)==0);
    (void)fprintf(f,",\"ipc_measurement\":false,\"pmc_mode\":\"%s\",\"pmc_status\":\"%s\",\"interval\":\"scheduled_t0_to_latest_worker_rmw_end\",\"warmup_iterations\":0,\"sampling_interrupts_enabled\":false,\"timestamp_ordering\":\"inherited_ordered_clock_boundaries\",\"binary\":{\"path\":",enabled?"count":"off",overall_pmc(w,workers,enabled));
    bench_json_string(f,binary);(void)fprintf(f,",\"sha256\":\"%s\",\"bytes\":%"PRIu64",\"rehash_after_run\":true},\"build\":",hash,binary_bytes);bench_build_identity_json(f);
    (void)fprintf(f,",\"virtual_timer_probe\":{\"status\":\"%s\",\"error\":%d,\"signal\":%d,\"cntfrq_hz\":%"PRIu64",\"sysctl_tbfrequency_hz\":%"PRIu64",\"sysctl_error\":%d,\"kind\":\"SYSTEM_TIMER_NOT_CPU_CYCLES\",\"first\":",probe->status,probe->error,probe->signal_number,probe->cntfrq_hz,probe->sysctl_hz,probe->sysctl_error);
    timer_json(f,&probe->first);(void)fputs(",\"last\":",f);timer_json(f,&probe->last);(void)fputs("},\"per_worker\":[",f);
    for(uint32_t i=0;i<workers;++i){if(i)(void)fputc(',',f);
        (void)fprintf(f,"{\"worker_id\":%u,\"requested_cpu\":%d,\"verified_cpu\":%d,\"affinity_tag\":%d,\"qos\":%d,\"counter_offset\":%zu,\"start_tick\":%"PRIu64",\"end_tick\":%"PRIu64",\"worker_delta_ticks\":%"PRIu64",\"window_start_tick\":%"PRIu64",\"window_end_tick\":%"PRIu64",\"final_counter_value\":%"PRIu64",\"placement\":",i,w[i].cpu,w[i].placement.verified_cpu,w[i].tag,w[i].qos,(size_t)i*stride,w[i].start_tick,w[i].end_tick,w[i].end_tick-w[i].start_tick,w[i].window_start,w[i].window_end,w[i].final_value);
        bench_placement_json(f,&w[i].placement);(void)fputs(",\"pmc\":",f);pmc_json(f,&w[i].pmc);
        (void)fputs(",\"cpu_before\":",f);usage_json(f,&w[i].usage_before);(void)fputs(",\"cpu_after\":",f);usage_json(f,&w[i].usage_after);
        (void)fputs(",\"cpu_utilization_percent\":",f);
        if(!w[i].usage_before.error&&!w[i].usage_after.error&&w[i].window_end>w[i].window_start&&w[i].usage_after.user_us>=w[i].usage_before.user_us&&w[i].usage_after.system_us>=w[i].usage_before.system_us)
        {
            uint64_t du=w[i].usage_after.user_us-w[i].usage_before.user_us;
            uint64_t ds=w[i].usage_after.system_us-w[i].usage_before.system_us;
            B_CHECK(du<=UINT64_MAX-ds);
            B_CHECK(bench_json_ratio(f,du+ds,UINT64_C(100000)*clock.denom,
                                    w[i].window_end-w[i].window_start,clock.numer)==0);
        }
        else(void)fputs("null",f);
        (void)fputs(",\"virtual_before\":",f);if(w[i].virtual_enabled)timer_json(f,&w[i].timer_before);else(void)fputs("null",f);
        (void)fputs(",\"virtual_after\":",f);if(w[i].virtual_enabled)timer_json(f,&w[i].timer_after);else(void)fputs("null",f);(void)fputc('}',f);
    }
    (void)fprintf(f,"],\"process_envelope\":{\"start_tick\":%"PRIu64",\"end_tick\":%"PRIu64",\"scope\":\"all_threads_including_coordinator_watchdog_and_future_start_wait\",\"cpu_before\":",process_start,process_end);
    usage_json(f,&process_before);(void)fputs(",\"cpu_after\":",f);usage_json(f,&process_after);
    (void)fprintf(f,",\"os_accounted_before\":{\"error\":%d,\"cycles\":%"PRIu64",\"instructions\":%"PRIu64"},\"os_accounted_after\":{\"error\":%d,\"cycles\":%"PRIu64",\"instructions\":%"PRIu64"}},\"metrics\":",counts_before.error,counts_before.cycles,counts_before.instructions,counts_after.error,counts_after.cycles,counts_after.instructions);
    metrics_json(f,w,workers,ops);(void)fputs("}\n",f);bench_json_close(f);
    printf("CACHE v2 workers=%u stride=%zu trial=%u RMW/s=%.3f pmc=%s (not IPC)\n",workers,stride,trial,rate,overall_pmc(w,workers,enabled));
    free(records);free(storage);
}
int main(int argc,char **argv)
{
    bool enabled=false,vct=false;int write_index=1;
    for(int i=1;i<argc;++i){
        if(strcmp(argv[i],"--pmc")==0 || strcmp(argv[i],"--virtual-counter")==0){
            bool is_pmc=strcmp(argv[i],"--pmc")==0;B_CHECK(i+1<argc);const char *value=argv[++i];
            if(is_pmc){B_CHECK(strcmp(value,"off")==0||strcmp(value,"count")==0);enabled=strcmp(value,"count")==0;}
            else{B_CHECK(strcmp(value,"off")==0||strcmp(value,"probe")==0);vct=strcmp(value,"probe")==0;}}
        else argv[write_index++]=argv[i];
    }
    argc=write_index;argv[argc]=NULL;
    struct bench_options o;bench_parse(argc,argv,false,&o);
    B_CHECK(o.mode==ELITE_NCQ&&o.producers==o.consumers&&o.producers<=16);
    char binary[4096],hash[65];uint64_t bytes;B_CHECK(bench_executable_path(binary,sizeof(binary))==0&&bench_sha256_file(binary,hash,&bytes)==0);
    struct virtual_probe probe=virtual_probe_run(vct);
    for(uint32_t i=0;i<o.trials;++i){run_layout(&o,i%2?128:8,i,enabled,&probe,binary,hash,bytes);run_layout(&o,i%2?8:128,i,enabled,&probe,binary,hash,bytes);}
    FILE *f=bench_json_open(o.output,"cache-campaign.json");
    (void)fprintf(f,"{\"schema\":\"elite-cache-campaign-v2\",\"status\":\"COMPLETE\",\"trials\":%u,\"expected_records\":%u,\"workers\":%u,\"iterations_per_worker\":%"PRIu64",\"binary_sha256\":\"%s\",\"pmc_mode\":\"%s\"}\n",o.trials,o.trials*2,o.producers,o.count,hash,enabled?"count":"off");bench_json_close(f);return 0;
}
