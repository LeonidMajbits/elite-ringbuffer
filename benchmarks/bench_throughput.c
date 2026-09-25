/* Validated drained-cohort goodput. P/C worker processes attach independently.
 * Per-consumer private bitmaps establish exact finite-run membership. Their
 * hot-loop cost is included; file exports and union checks occur after drain.
 * The 1/2/4/8 sweep is tools/run_benchmarks.py; no SPSC is disguised as MPMC. */
#include "bench_common.h"

static uint64_t run_phase(elite_connection *c,const struct bench_config *cfg,
    struct bench_control *control,unsigned char *bitmap,uint32_t phase,uint64_t *polls)
{
    bool producer=cfg->index<cfg->producers;
    uint64_t total=phase==1?cfg->warmup:cfg->count,quota=total/cfg->producers;
    uint64_t seed=phase==1?BENCH_WARM_SEED:BENCH_SEED,base=phase==1?BENCH_MAX_ITEMS:0;
    uint64_t count=0;
    if(producer){
        for(uint64_t seq=0;seq<quota;++seq){uint64_t id=(uint64_t)cfg->index*quota+seq;bench_send(c,base+id,cfg->index,seq,seed,polls);++count;}
    }else{
        for(;;){
            elite_lease lease;elite_read_span span;elite_result r=elite_read_borrow(c,&lease,&span);
            if(r.status==ELITE_NO_DATA_OBSERVED){
                ++*polls;
                if(atomic_load_explicit(&control->cell[cfg->index].drain,memory_order_acquire)==phase){
                    /* This acquire observes all producer-completion receipts.
                     * Recheck AFTER it: an earlier empty snapshot may be stale. */
                    r=elite_read_borrow(c,&lease,&span);
                    if(r.status==ELITE_NO_DATA_OBSERVED)break;
                }else{bench_relax();continue;}
            }
            B_CHECK(r.status==ELITE_OK&&span.length==64&&span.message_type==BENCH_TYPE&&span.message_id>=base);
            uint64_t id=span.message_id-base;B_CHECK(id<total&&quota!=0);
            B_CHECK(bench_payload_check(span.data,base+id,id/quota,id%quota,seed));
            size_t byte=(size_t)(id/8);unsigned char bit=(unsigned char)(1u<<(unsigned)(id%8));
            B_CHECK((bitmap[byte]&bit)==0);bitmap[byte]|=bit;
            r=elite_read_release(c,&lease);B_CHECK(r.status==ELITE_OK&&r.outcome==ELITE_RETURNED);++count;
        }
    }
    return count;
}
static int worker(void)
{
    struct bench_config cfg;bench_read_all(STDIN_FILENO,&cfg,sizeof(cfg));bench_process_init(cfg.directory);
    B_CHECK(cfg.grants==1&&cfg.count>0&&cfg.count<=BENCH_MAX_ITEMS&&cfg.producers>0);
    struct bench_result r;memset(&r,0,sizeof(r));r.index=cfg.index;bench_clock_init(&r.clock);
    bench_placement_apply(cfg.requested_cpu,cfg.affinity_tag,cfg.qos,&r.placement);
    elite_connection *c=NULL;B_OK(elite_attach(&cfg.grant[0],&c));
    struct bench_control *control=bench_control_open(cfg.control_path);
    bool producer=cfg.index<cfg.producers;uint64_t largest=cfg.count>cfg.warmup?cfg.count:cfg.warmup;
    size_t bytes=(size_t)((largest+7)/8);unsigned char *bitmap=producer?NULL:malloc(bytes);
    B_CHECK(producer||bitmap!=NULL);
    if(!producer){volatile unsigned char *v=bitmap;for(size_t i=0;i<bytes;i+=4096)v[i]=0;memset(bitmap,0,bytes);}
    bench_worker_event(&r,B_READY);uint64_t command;bench_worker_command(&command);B_CHECK(command==1);
    uint64_t polls=0;r.count=run_phase(c,&cfg,control,bitmap,1,&polls);
    if(!producer)memset(bitmap,0,bytes);
    bench_worker_event(&r,B_WARM_DONE);bench_worker_command(&command);bench_wait_start(command);
    B_CHECK(getrusage(RUSAGE_SELF,&r.usage_before)==0);r.start_tick=bench_ordered_tick();polls=0;
    r.count=run_phase(c,&cfg,control,bitmap,2,&polls);
    r.end_tick=bench_ordered_tick();B_CHECK(getrusage(RUSAGE_SELF,&r.usage_after)==0);
    r.empty_polls=polls;r.bitmap_bytes=producer?0:(cfg.count+7)/8;
    bench_placement_finish(&r.placement);bench_worker_event(&r,B_MEASURE_DONE);
    bench_worker_command(&command);B_CHECK(command==0);
    B_OK(elite_detach(&c,&r.receipts[0]));bench_control_close(control);
    if(!producer){char name[80];(void)snprintf(name,sizeof(name),"consumer-%02"PRIu32".bitmap",cfg.index-cfg.producers);bench_save(cfg.directory,name,bitmap,(size_t)r.bitmap_bytes);}
    bench_worker_event(&r,B_FINAL);free(bitmap);return 0;
}
static void drain_after_producers(struct bench_cohort *b,uint32_t producers,uint32_t phase,uint32_t event,struct bench_result results[BENCH_MAX_WORKERS])
{
    for(uint32_t i=0;i<producers;++i)bench_get_event(b,i,event,&results[i]);
    for(uint32_t i=producers;i<b->workers;++i)atomic_store_explicit(&b->control->cell[i].drain,phase,memory_order_release);
    for(uint32_t i=producers;i<b->workers;++i)bench_get_event(b,i,event,&results[i]);
}
static void verify_bitmaps(const char *directory,uint64_t count,uint32_t producers,uint32_t consumers,const struct bench_result *r)
{
    size_t bytes=(size_t)((count+7)/8);unsigned char *combined=calloc(bytes,1),*one=malloc(bytes);B_CHECK(combined!=NULL&&one!=NULL);
    for(uint32_t i=0;i<consumers;++i){
        char name[80],path[BENCH_PATH];(void)snprintf(name,sizeof(name),"consumer-%02"PRIu32".bitmap",i);bench_path(path,directory,name);
        FILE *f=fopen(path,"rb");B_CHECK(f!=NULL);B_CHECK(fread(one,1,bytes,f)==bytes&&fgetc(f)==EOF&&!ferror(f));B_CHECK(fclose(f)==0);
        uint64_t pop=0;for(size_t j=0;j<bytes;++j){B_CHECK((combined[j]&one[j])==0);combined[j]|=one[j];unsigned char v=one[j];while(v){pop+=v&1u;v>>=1;}}
        B_CHECK(pop==r[producers+i].count&&r[producers+i].bitmap_bytes==bytes);
    }
    for(uint64_t i=0;i<count;++i)B_CHECK((combined[i/8]&(1u<<(unsigned)(i%8)))!=0);
    if(count%8!=0){unsigned char unused=(unsigned char)(0xffu<<(unsigned)(count%8));B_CHECK((combined[bytes-1]&unused)==0);}
    bench_save(directory,"union.bitmap",combined,bytes);free(combined);free(one);
}
static void trial(const char *executable,const struct bench_options *o,uint32_t index)
{
    char name[80],directory[BENCH_PATH];(void)snprintf(name,sizeof(name),"trial-%03"PRIu32,index);bench_path(directory,o->output,name);bench_mkdir(directory);
    uint32_t k=o->producers+o->consumers;struct bench_cohort b;bench_cohort_init(&b,directory,k,o->timeout_seconds);bench_cohort_create(&b,0,o->mode,o->producers,o->consumers,o->capacity);
    for(uint32_t i=0;i<k;++i){
        bench_spawn(&b,i,executable);struct bench_config cfg;memset(&cfg,0,sizeof(cfg));cfg.grants=1;cfg.index=i;cfg.producers=o->producers;cfg.consumers=o->consumers;cfg.count=o->count;cfg.warmup=o->warmup;
        cfg.requested_cpu=o->cpu_count?o->cpus[i%o->cpu_count]:-1;cfg.affinity_tag=o->affinity_tag;cfg.qos=o->qos;
        strcpy(cfg.directory,directory);strcpy(cfg.control_path,b.control_path);bench_issue(&b,0,i,i,&cfg.grant[0]);bench_write_all(b.child[i].command_fd,&cfg,sizeof(cfg));
    }
    struct bench_result r[BENCH_MAX_WORKERS];memset(r,0,sizeof(r));for(uint32_t i=0;i<k;++i)bench_get_event(&b,i,B_READY,&r[i]);
    uint64_t command=1;for(uint32_t i=0;i<k;++i)bench_write_all(b.child[i].command_fd,&command,sizeof(command));
    drain_after_producers(&b,o->producers,1,B_WARM_DONE,r);
    uint64_t received=0;for(uint32_t i=0;i<k;++i){if(i<o->producers)B_CHECK(r[i].count==o->warmup/o->producers);else received+=r[i].count;}B_CHECK(received==o->warmup);
    bench_reconcile(&b.first[0],o->warmup,directory,"warmup-reconcile.json");
    struct bench_clock clock;bench_clock_init(&clock);command=bench_future_tick(&clock,100000000);uint64_t scheduled_start=command;
    for(uint32_t i=0;i<k;++i)bench_write_all(b.child[i].command_fd,&command,sizeof(command));
    drain_after_producers(&b,o->producers,2,B_MEASURE_DONE,r);
    uint64_t drain_end=0;received=0;
    for(uint32_t i=0;i<k;++i){
        B_CHECK(r[i].clock.numer==clock.numer&&r[i].clock.denom==clock.denom&&r[i].start_tick>=scheduled_start&&r[i].end_tick>=r[i].start_tick);
        if(i<o->producers)B_CHECK(r[i].count==o->count/o->producers);
        else{received+=r[i].count;if(r[i].end_tick>drain_end)drain_end=r[i].end_tick;}
    }
    B_CHECK(received==o->count&&drain_end>scheduled_start);
    command=0;for(uint32_t i=0;i<k;++i)bench_write_all(b.child[i].command_fd,&command,sizeof(command));
    for(uint32_t i=0;i<k;++i){bench_get_event(&b,i,B_FINAL,&r[i]);B_OK(elite_object_ack_cleanup(b.object[0],&r[i].receipts[0]));}
    verify_bitmaps(directory,o->count,o->producers,o->consumers,r);
    bench_reconcile(&b.first[0],o->warmup+o->count,directory,"final-reconcile.json");bench_cohort_finish(&b);
    long double seconds=bench_ns(&clock,drain_end-scheduled_start)/1000000000.0L;
    long double rate=(long double)o->count/seconds,gb=rate*64.0L/1000000000.0L;
    FILE *f=bench_json_open(directory,"result.json");
    (void)fputs("{\"hardware_topology\":",f);bench_hardware_topology_json(f);fprintf(f,",\"schema\":\"elite-bench-throughput-v1\",\"status\":\"PASS_WITHIN_SCOPE\",\"mode\":\"%s\",\"messages\":%"PRIu64",\"warmup_messages\":%"PRIu64",\"producers\":%u,\"consumers\":%u,\"capacity\":%"PRIu64",\"bytes_per_message\":64,\"scheduled_start_tick\":%"PRIu64",\"drain_end_tick\":%"PRIu64",\"timebase_numer\":%u,\"timebase_denom\":%u,\"elapsed_ns_ceiling\":%"PRIu64",\"validated_messages_per_second\":%.6Lf,\"validated_payload_GB_per_second_decimal\":%.9Lf,\"bandwidth_meaning\":\"delivered application payload, not DRAM traffic\",\"exact_membership\":true,\"all_tokens_returned\":true,\"designated_cell_isolation_64_128\":true,\"universal_no_false_sharing\":false,\"per_message_timestamps\":false,\"coherence_counters\":\"NOT_COLLECTED\",\"workers_info\":[",o->mode==ELITE_SPSC?"SPSC":"NCQ-SC64",o->count,o->warmup,o->producers,o->consumers,o->capacity,scheduled_start,drain_end,clock.numer,clock.denom,bench_ns_ceil(&clock,drain_end-scheduled_start),rate,gb);
    for(uint32_t i=0;i<k;++i){
        (void)fprintf(f,"%s{\"index\":%u,\"count\":%"PRIu64",\"empty_polls\":%"PRIu64",\"start_tick\":%"PRIu64",\"end_tick\":%"PRIu64",\"placement\":",i?",":"",i,r[i].count,r[i].empty_polls,r[i].start_tick,r[i].end_tick);
        bench_placement_json(f,&r[i].placement);(void)fprintf(f,",\"resources\":");bench_usage_json(f,&r[i].usage_before,&r[i].usage_after);(void)fprintf(f,"}");
    }
    (void)fprintf(f,"]}\n");bench_json_close(f);
    printf("PASS GOODPUT %s %uP/%uC messages=%"PRIu64" msg/s=%.0Lf payload_GB/s=%.6Lf exact_membership=yes tokens_returned=yes\n",o->mode==ELITE_SPSC?"SPSC":"NCQ-SC64",o->producers,o->consumers,o->count,rate,gb);
}
int main(int argc,char **argv)
{
    if(argc==2&&strcmp(argv[1],"--worker")==0)return worker();
    struct bench_options o;bench_parse(argc,argv,false,&o);
    for(uint32_t i=0;i<o.trials;++i)trial(argv[0],&o,i);
    return 0;
}
