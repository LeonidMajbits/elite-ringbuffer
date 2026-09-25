/* Native, fully checked request/reply latency. Two independently mapped POSIX
 * rings carry all payloads. Timers and raw logs do not touch shared ABI padding.
 * Count is round trips: count=100M publishes 200M directional messages.
 * NCQ here is 1P/1C per direction, not a contended multi-requester result. */
#include "bench_common.h"
#include <sys/mman.h>

static void prefault(void *memory,size_t bytes)
{
    volatile unsigned char *p=memory;
    for(size_t i=0;i<bytes;i+=4096)p[i]=0;
    if(bytes!=0)p[bytes-1]=0;
}
static int worker(void)
{
    struct bench_config cfg;bench_read_all(STDIN_FILENO,&cfg,sizeof(cfg));bench_process_init(cfg.directory);
    B_CHECK(cfg.grants==2&&cfg.index<2&&cfg.count>0&&cfg.count<=BENCH_MAX_ITEMS);
    struct bench_result r;memset(&r,0,sizeof(r));r.index=cfg.index;bench_clock_init(&r.clock);
    bench_placement_apply(cfg.requested_cpu,cfg.affinity_tag,cfg.qos,&r.placement);
    elite_connection *connections[2]={NULL,NULL};B_OK(elite_attach(&cfg.grant[0],&connections[0]));B_OK(elite_attach(&cfg.grant[1],&connections[1]));
    bool origin=cfg.index==0;
    elite_connection *send=connections[origin?0:1],*receive=connections[origin?1:0];
    uint64_t *starts=NULL,*elapsed=NULL,*before=NULL,*after=NULL;
    if(origin){
        B_CHECK(cfg.count<=SIZE_MAX/sizeof(uint64_t)&&cfg.calibration>0&&cfg.calibration<=1000000);
        size_t bytes=(size_t)cfg.count*sizeof(uint64_t),calbytes=(size_t)cfg.calibration*sizeof(uint64_t);
        starts=malloc(bytes);elapsed=malloc(bytes);before=malloc(calbytes);after=malloc(calbytes);
        B_CHECK(starts!=NULL&&elapsed!=NULL&&before!=NULL&&after!=NULL);
        prefault(starts,bytes);prefault(elapsed,bytes);prefault(before,calbytes);prefault(after,calbytes);
        bench_calibrate(before,cfg.calibration,&r.calibration_before);
    }
    bench_worker_event(&r,B_READY);uint64_t command;bench_worker_command(&command);B_CHECK(command==1);
    uint64_t polls=0;
    for(uint64_t id=0;id<cfg.warmup;++id){
        if(origin){bench_send(send,id,0,id,BENCH_WARM_SEED,&polls);bench_receive_exact(receive,id,0,id,BENCH_WARM_SEED^BENCH_REPLY_SEED,&polls);}
        else {bench_receive_exact(receive,id,0,id,BENCH_WARM_SEED,&polls);bench_send(send,id,0,id,BENCH_WARM_SEED^BENCH_REPLY_SEED,&polls);}
    }
    r.count=cfg.warmup;bench_worker_event(&r,B_WARM_DONE);bench_worker_command(&command);bench_wait_start(command);
    B_CHECK(getrusage(RUSAGE_SELF,&r.usage_before)==0);polls=0;
    if(origin){
        for(uint64_t id=0;id<cfg.count;++id){
            uint64_t start=bench_ordered_tick();
            bench_send(send,id,0,id,BENCH_SEED,&polls);
            bench_receive_exact(receive,id,0,id,BENCH_SEED^BENCH_REPLY_SEED,&polls);
            uint64_t stop=bench_ordered_tick();B_CHECK(stop>=start);
            /* Both leases ended: only process-private storage is written. */
            starts[id]=start;elapsed[id]=stop-start;
        }
        r.start_tick=starts[0];r.end_tick=starts[cfg.count-1]+elapsed[cfg.count-1];
    }else{
        r.start_tick=bench_ordered_tick();
        for(uint64_t id=0;id<cfg.count;++id){bench_receive_exact(receive,id,0,id,BENCH_SEED,&polls);bench_send(send,id,0,id,BENCH_SEED^BENCH_REPLY_SEED,&polls);}
        r.end_tick=bench_ordered_tick();
    }
    B_CHECK(getrusage(RUSAGE_SELF,&r.usage_after)==0);r.count=cfg.count;r.empty_polls=polls;
    bench_placement_finish(&r.placement);bench_worker_event(&r,B_MEASURE_DONE);
    bench_worker_command(&command);B_CHECK(command==0); /* All payload work stopped. */
    B_OK(elite_detach(&connections[0],&r.receipts[0]));B_OK(elite_detach(&connections[1],&r.receipts[1]));
    if(origin){
        bench_calibrate(after,cfg.calibration,&r.calibration_after);
        bench_save(cfg.directory,"start_ticks.u64le",starts,(size_t)cfg.count*8);
        bench_save(cfg.directory,"rtt_ticks.u64le",elapsed,(size_t)cfg.count*8);
        bench_save(cfg.directory,"clock_before.u64le",before,(size_t)cfg.calibration*8);
        bench_save(cfg.directory,"clock_after.u64le",after,(size_t)cfg.calibration*8);
        /* Preserve original order BEFORE sort; quantiles are exact ranks. */
        bench_statistics(elapsed,cfg.count,&r.latency);
        bench_statistics(before,cfg.calibration,&r.calibration_before);
        bench_statistics(after,cfg.calibration,&r.calibration_after);
    }
    bench_worker_event(&r,B_FINAL);free(starts);free(elapsed);free(before);free(after);return 0;
}
static void trial(const char *executable,const struct bench_options *o,uint32_t trial_index)
{
    char name[80],directory[BENCH_PATH];(void)snprintf(name,sizeof(name),"trial-%03"PRIu32,trial_index);bench_path(directory,o->output,name);bench_mkdir(directory);
    struct bench_cohort b;bench_cohort_init(&b,directory,2,o->timeout_seconds);
    /* Separate managers: two independent transports, neither pretending to be
     * an in-place successor of the other. Four endpoints and two workers. */
    bench_cohort_create(&b,0,o->mode,1,1,o->capacity);bench_cohort_create(&b,1,o->mode,1,1,o->capacity);
    for(uint32_t i=0;i<2;++i){
        bench_spawn(&b,i,executable);struct bench_config cfg;memset(&cfg,0,sizeof(cfg));
        cfg.grants=2;cfg.index=i;cfg.count=o->count;cfg.warmup=o->warmup;cfg.calibration=o->calibration;
        cfg.requested_cpu=o->cpu_count?o->cpus[i%o->cpu_count]:-1;cfg.affinity_tag=o->affinity_tag;cfg.qos=o->qos;
        strcpy(cfg.directory,directory);strcpy(cfg.control_path,b.control_path);
        bench_issue(&b,0,i==0?0:1,i,&cfg.grant[0]);bench_issue(&b,1,i==0?1:0,i,&cfg.grant[1]);
        bench_write_all(b.child[i].command_fd,&cfg,sizeof(cfg));
    }
    struct bench_result r[2];for(uint32_t i=0;i<2;++i)bench_get_event(&b,i,B_READY,&r[i]);
    uint64_t command=1;for(uint32_t i=0;i<2;++i)bench_write_all(b.child[i].command_fd,&command,sizeof(command));
    for(uint32_t i=0;i<2;++i){bench_get_event(&b,i,B_WARM_DONE,&r[i]);B_CHECK(r[i].count==o->warmup);}
    bench_reconcile(&b.first[0],o->warmup,directory,"warmup-request.json");bench_reconcile(&b.first[1],o->warmup,directory,"warmup-reply.json");
    struct bench_clock clock;bench_clock_init(&clock);command=bench_future_tick(&clock,100000000);
    uint64_t scheduled_start=command;
    for(uint32_t i=0;i<2;++i)bench_write_all(b.child[i].command_fd,&command,sizeof(command));
    for(uint32_t i=0;i<2;++i){bench_get_event(&b,i,B_MEASURE_DONE,&r[i]);B_CHECK(r[i].count==o->count);}
    command=0;for(uint32_t i=0;i<2;++i)bench_write_all(b.child[i].command_fd,&command,sizeof(command));
    for(uint32_t i=0;i<2;++i){bench_get_event(&b,i,B_FINAL,&r[i]);for(uint32_t j=0;j<2;++j)B_OK(elite_object_ack_cleanup(b.object[j],&r[i].receipts[j]));}
    bench_reconcile(&b.first[0],o->count+o->warmup,directory,"final-request.json");bench_reconcile(&b.first[1],o->count+o->warmup,directory,"final-reply.json");
    bench_cohort_finish(&b);
    B_CHECK(r[0].clock.numer==clock.numer&&r[0].clock.denom==clock.denom&&r[0].start_tick>=scheduled_start);
    FILE *f=bench_json_open(directory,"result.json");
    (void)fputs("{\"hardware_topology\":",f);bench_hardware_topology_json(f);fprintf(f,",\"schema\":\"elite-bench-rtt-v1\",\"status\":\"PASS_WITHIN_SCOPE\",\"mode\":\"%s\",\"roundtrips\":%"PRIu64",\"directional_records\":%"PRIu64",\"warmup_roundtrips\":%"PRIu64",\"bytes_per_direction\":64,\"workers\":2,\"endpoints\":4,\"outstanding_roundtrips\":1,\"capacity_per_ring\":%"PRIu64",\"clock\":\"%s\",\"timebase_numer\":%u,\"timebase_denom\":%u,\"reported_resolution_ns\":%"PRIu64",\"scheduled_start_tick\":%"PRIu64",\"origin_start_tick\":%"PRIu64",\"origin_end_tick\":%"PRIu64",\"one_way_certification\":\"NOT_MEASURED\",\"absolute_uncertainty\":\"NOT_QUALIFIED\",\"sampling_profiler_enabled\":false,\"full_payload_and_id_check\":true,\"all_tokens_returned\":true,\"latency\":",o->mode==ELITE_SPSC?"SPSC":"NCQ-SC64",o->count,2*o->count,o->warmup,o->capacity,
#ifdef __APPLE__
        "mach_absolute_time",
#else
        "CLOCK_MONOTONIC_RAW",
#endif
        clock.numer,clock.denom,clock.reported_resolution_ns,scheduled_start,r[0].start_tick,r[0].end_tick);
    bench_stats_json(f,&r[0].latency,&clock);(void)fprintf(f,",\"calibration_before\":");bench_stats_json(f,&r[0].calibration_before,&clock);(void)fprintf(f,",\"calibration_after\":");bench_stats_json(f,&r[0].calibration_after,&clock);
    (void)fprintf(f,",\"workers_info\":[");
    for(uint32_t i=0;i<2;++i){(void)fprintf(f,"%s{\"index\":%u,\"empty_polls\":%"PRIu64",\"placement\":",i?",":"",i,r[i].empty_polls);bench_placement_json(f,&r[i].placement);(void)fprintf(f,",\"resources\":");bench_usage_json(f,&r[i].usage_before,&r[i].usage_after);(void)fprintf(f,"}");}
    (void)fprintf(f,"]}\n");bench_json_close(f);
    printf("PASS RTT %s count=%"PRIu64" p50_ns_ceiling=%"PRIu64" p99_ns_ceiling=%"PRIu64" (raw instrumented round trip, NOT one-way)\n",o->mode==ELITE_SPSC?"SPSC":"NCQ-SC64",o->count,bench_ns_ceil(&clock,r[0].latency.q[0]),bench_ns_ceil(&clock,r[0].latency.q[2]));
}
int main(int argc,char **argv)
{
    if(argc==2&&strcmp(argv[1],"--worker")==0)return worker();
    struct bench_options options;bench_parse(argc,argv,true,&options);
    for(uint32_t i=0;i<options.trials;++i)trial(argv[0],&options,i);
    return 0;
}
