/* Instrumented native IPC matrix. Every message is fully checked. Control
 * acknowledgments implement a declared per-producer closed loop; they carry
 * no payload. Scheduled offers never shift after a stall. No core changes. */
#include "bench_common.h"
#include "bench_identity.h"
#include "bench_ratio.h"
#include "elite_topology.h"
#include <sched.h>
#include <limits.h>
struct matrix_options {
    struct bench_options base;
    uint32_t payload,checksum,arrival,burst,os_yield;
    uint64_t period_ns,hold_ns,hold_every,max_bytes;
    char locality[48];
};
struct matrix_config {struct bench_config b;uint32_t payload,arrival,burst,os_yield;uint64_t period_ns,hold_ns,hold_every;};
struct matrix_trace {uint64_t id,a,b;};
_Static_assert(sizeof(struct matrix_trace)==24,"trace ABI");
static uint64_t argnum(const char *p,uint64_t max)
{
    B_CHECK(*p>='0'&&*p<='9');char *end;errno=0;unsigned long long n=strtoull(p,&end,10);B_CHECK(!errno&&!*end&&n<=max);return (uint64_t)n;
}
static uint64_t offset_ticks(const struct bench_clock *clock,uint64_t ns)
{B_CHECK(ns<=UINT64_MAX/clock->denom);uint64_t v=ns*clock->denom;return v/clock->numer+(v%clock->numer!=0?1u:0u);}
static void pattern(void *data,uint32_t len,uint64_t id,bool writing)
{
    unsigned char *p=data;uint64_t value=id^UINT64_C(0xa5a5a5a5a5a5a5a5),bad=0;
    for(uint32_t j=0;j<len;j+=8){if(writing)memcpy(p+j,&value,8);else{uint64_t v;memcpy(&v,p+j,8);bad|=v^value;}}
    B_CHECK(bad==0);
}
static void hold(uint64_t ns,const struct bench_clock *clock)
{if(ns){uint64_t now=bench_tick(),d=offset_ticks(clock,ns);B_CHECK(d<=UINT64_MAX-now);bench_wait_start(now+d);}}
static uint64_t phase(elite_connection *ep,const struct matrix_config *m,struct bench_control *ctrl,
    unsigned phase_no,uint64_t start,struct matrix_trace *trace,unsigned char *bitmap,uint64_t *polls,const struct bench_clock *clock)
{
    bool writer=m->b.index<m->b.producers;uint64_t total=phase_no==1?m->b.warmup:m->b.count;
    uint64_t quota=total/m->b.producers,base=phase_no==1?BENCH_MAX_ITEMS:0,count=0;
    if(writer){for(uint64_t seq=0;seq<quota;++seq){
        uint64_t id=(uint64_t)m->b.index*quota+seq,offer=0;
        if(phase_no==2){
            if(m->arrival==0){while(atomic_load_explicit(&ctrl->cell[m->b.index].drain,memory_order_acquire)!=(uint32_t)seq){++*polls;bench_relax();}offer=bench_ordered_tick();}
            else if(m->arrival==1)offer=bench_ordered_tick();
            else{uint64_t order=seq*m->b.producers+m->b.index,batch=order/(m->arrival==3?m->burst:1u);B_CHECK(batch<=UINT64_MAX/m->period_ns);uint64_t off=offset_ticks(clock,batch*m->period_ns);B_CHECK(start<=UINT64_MAX-off);offer=start+off;bench_wait_start(offer);}
        }
        uint64_t entry=phase_no==2?bench_ordered_tick():0;elite_lease lease;elite_write_span span;elite_result x;
        do{x=elite_write_reserve(ep,&lease,&span);if(x.status==ELITE_NO_CAPACITY_OBSERVED){++*polls;bench_relax();}}while(x.status==ELITE_NO_CAPACITY_OBSERVED);
        B_CHECK(x.status==ELITE_OK&&span.capacity>=m->payload);pattern(span.data,m->payload,base+id,true);
        x=elite_write_commit(ep,&lease,m->payload,BENCH_TYPE,base+id);B_CHECK(x.status==ELITE_OK&&x.outcome==ELITE_PUBLISHED);
        if(phase_no==2){trace[count]=(struct matrix_trace){id,offer,entry};if(m->os_yield&&seq%32==0)B_CHECK(sched_yield()==0);}++count;
    }}else{
        for(;;){elite_lease lease;elite_read_span span;elite_result x=elite_read_borrow(ep,&lease,&span);
            if(x.status==ELITE_NO_DATA_OBSERVED){++*polls;if(atomic_load_explicit(&ctrl->cell[m->b.index].drain,memory_order_acquire)==phase_no){x=elite_read_borrow(ep,&lease,&span);if(x.status==ELITE_NO_DATA_OBSERVED)break;}else{bench_relax();continue;}}
            B_CHECK(x.status==ELITE_OK&&span.length==m->payload&&span.message_type==BENCH_TYPE&&span.message_id>=base);
            uint64_t id=span.message_id-base;B_CHECK(id<total&&quota>0);pattern((void *)span.data,m->payload,span.message_id,false);
            uint64_t read_tick=phase_no==2?bench_ordered_tick():0;
            unsigned char bit=(unsigned char)(1u<<(unsigned)(id%8));B_CHECK(!(bitmap[id/8]&bit));bitmap[id/8]|=bit;
            if(phase_no==2&&m->hold_ns&&id%m->hold_every==0)hold(m->hold_ns,clock);
            x=elite_read_release(ep,&lease);B_CHECK(x.status==ELITE_OK&&x.outcome==ELITE_RETURNED);
            if(phase_no==2){trace[count]=(struct matrix_trace){id,read_tick,bench_ordered_tick()};
                if(m->arrival==0)atomic_store_explicit(&ctrl->cell[id/quota].drain,(uint32_t)(id%quota+1),memory_order_release);
                if(m->os_yield&&id%32==0)B_CHECK(sched_yield()==0);}
            ++count;
        }
    }
    return count;
}
static int worker(void)
{
    struct matrix_config m;bench_read_all(STDIN_FILENO,&m,sizeof(m));struct bench_config *c=&m.b;
    bench_process_init(c->directory);struct bench_result r;memset(&r,0,sizeof(r));r.index=c->index;bench_clock_init(&r.clock);
    bench_placement_apply(c->requested_cpu,c->affinity_tag,c->qos,&r.placement);
    elite_connection *ep=NULL;B_OK(elite_attach(&c->grant[0],&ep));struct bench_control *ctrl=bench_control_open(c->control_path);
    bool writer=c->index<c->producers;uint64_t max=c->count>c->warmup?c->count:c->warmup;
    size_t bytes=(size_t)((max+7)/8);unsigned char *bitmap=writer?NULL:calloc(bytes,1);
    struct matrix_trace *trace=calloc((size_t)(writer?c->count/c->producers:c->count),sizeof(*trace));B_CHECK(trace&&(writer||bitmap));
    /* Touch the full evidence allocation before timing; its later stores are
     * still real workload cost. */
    volatile unsigned char *touch=(unsigned char *)trace;size_t extent=(size_t)(writer?c->count/c->producers:c->count)*sizeof(*trace);
    for(size_t j=0;j<extent;j+=4096)touch[j]=0;
    bench_worker_event(&r,B_READY);uint64_t cmd;bench_worker_command(&cmd);B_CHECK(cmd==1);uint64_t polls=0;
    r.count=phase(ep,&m,ctrl,1,0,trace,bitmap,&polls,&r.clock);if(!writer)memset(bitmap,0,bytes);
    bench_worker_event(&r,B_WARM_DONE);bench_worker_command(&cmd);bench_wait_start(cmd);r.start_tick=bench_ordered_tick();
    B_CHECK(getrusage(RUSAGE_SELF,&r.usage_before)==0);polls=0;r.count=phase(ep,&m,ctrl,2,cmd,trace,bitmap,&polls,&r.clock);
    r.end_tick=bench_ordered_tick();B_CHECK(getrusage(RUSAGE_SELF,&r.usage_after)==0);r.empty_polls=polls;r.bitmap_bytes=writer?0:(c->count+7)/8;
    bench_placement_finish(&r.placement);bench_worker_event(&r,B_MEASURE_DONE);bench_worker_command(&cmd);B_CHECK(cmd==0);
    B_OK(elite_detach(&ep,&r.receipts[0]));bench_control_close(ctrl);
    char name[80];snprintf(name,sizeof(name),"%s-%02u.trace",writer?"producer":"consumer",writer?c->index:c->index-c->producers);bench_save(c->directory,name,trace,(size_t)r.count*sizeof(*trace));
    if(!writer){snprintf(name,sizeof(name),"consumer-%02u.bitmap",c->index-c->producers);bench_save(c->directory,name,bitmap,(size_t)r.bitmap_bytes);}
    free(trace);free(bitmap);bench_worker_event(&r,B_FINAL);return 0;
}
static void drain(struct bench_cohort *c,uint32_t p,uint32_t phase_no,uint32_t event,struct bench_result *r)
{for(uint32_t i=0;i<p;++i)bench_get_event(c,i,event,&r[i]);for(uint32_t i=p;i<c->workers;++i)atomic_store_explicit(&c->control->cell[i].drain,phase_no,memory_order_release);for(uint32_t i=p;i<c->workers;++i)bench_get_event(c,i,event,&r[i]);}
static void loadfile(const char *dir,const char *name,void *out,size_t bytes)
{char path[BENCH_PATH];bench_path(path,dir,name);FILE *f=fopen(path,"rb");B_CHECK(f);B_CHECK(fread(out,1,bytes,f)==bytes&&fgetc(f)==EOF&&!ferror(f));B_CHECK(fclose(f)==0);}
static void summary(const struct matrix_options *o,const char *dir,const struct bench_result *r,struct bench_stats stats[3])
{
    uint64_t n=o->base.count;size_t bytes=(size_t)((n+7)/8);unsigned char *all=calloc(bytes,1),*one=malloc(bytes);uint64_t *offer=calloc((size_t)n,8),*entry=calloc((size_t)n,8),*native=calloc((size_t)n,8),*service=calloc((size_t)n,8),*returned=calloc((size_t)n,8);struct matrix_trace *raw=malloc((size_t)n*sizeof(*raw));B_CHECK(all&&one&&offer&&entry&&native&&service&&returned&&raw);
    uint64_t quota=n/o->base.producers;char name[80];
    for(uint32_t i=0;i<o->base.producers;++i){snprintf(name,sizeof(name),"producer-%02u.trace",i);loadfile(dir,name,raw,(size_t)quota*sizeof(*raw));for(uint64_t j=0;j<quota;++j){uint64_t id=(uint64_t)i*quota+j;B_CHECK(raw[j].id==id&&raw[j].a<=raw[j].b);offer[id]=raw[j].a;entry[id]=raw[j].b;}}
    uint64_t total=0;
    for(uint32_t i=0;i<o->base.consumers;++i){snprintf(name,sizeof(name),"consumer-%02u.bitmap",i);loadfile(dir,name,one,bytes);uint64_t pop=0;for(size_t j=0;j<bytes;++j){B_CHECK(!(all[j]&one[j]));all[j]|=one[j];unsigned char v=one[j];while(v){pop+=v&1u;v>>=1;}}B_CHECK(pop==r[o->base.producers+i].count);
        snprintf(name,sizeof(name),"consumer-%02u.trace",i);loadfile(dir,name,raw,(size_t)pop*sizeof(*raw));
        unsigned char *seen=calloc(bytes,1);B_CHECK(seen);for(uint64_t j=0;j<pop;++j){uint64_t id=raw[j].id;B_CHECK(id<n&&raw[j].a>=entry[id]&&raw[j].b>=raw[j].a);unsigned char bit=(unsigned char)(1u<<(unsigned)(id%8));B_CHECK((one[id/8]&bit)&&!(seen[id/8]&bit));seen[id/8]|=bit;native[id]=raw[j].a-entry[id];service[id]=raw[j].a-offer[id];returned[id]=raw[j].b-entry[id];}B_CHECK(!memcmp(seen,one,bytes));free(seen);total+=pop;
    }
    B_CHECK(total==n);for(uint64_t id=0;id<n;++id)B_CHECK(all[id/8]&(1u<<(unsigned)(id%8)));if(n%8)B_CHECK((all[bytes-1]>>(unsigned)(n%8))==0);
    bench_save(dir,"union.bitmap",all,bytes);bench_statistics(native,n,&stats[0]);bench_statistics(service,n,&stats[1]);bench_statistics(returned,n,&stats[2]);free(all);free(one);free(offer);free(entry);free(native);free(service);free(returned);free(raw);
}
static void trial(const char *exe,const struct matrix_options *m,unsigned trial_id)
{
    char before_path[4096],before_hash[65];uint64_t before_size;B_CHECK(bench_executable_path(before_path,sizeof(before_path))==0&&bench_sha256_file(before_path,before_hash,&before_size)==0);
    const struct bench_options *o=&m->base;char name[80],dir[BENCH_PATH];snprintf(name,sizeof(name),"trial-%03u",trial_id);bench_path(dir,o->output,name);bench_mkdir(dir);
    struct elite_hardware_topology *top=NULL,*after=NULL;B_CHECK(elite_topology_discover(&top)==0);
    uint32_t k=o->producers+o->consumers;struct bench_cohort b;bench_cohort_init(&b,dir,k,o->timeout_seconds);
    bench_cohort_create_ex(&b,0,o->mode,o->producers,o->consumers,o->capacity,m->payload,m->checksum,m->max_bytes);
    struct elite_immutable_header info;B_OK(elite_object_info(b.object[0],&info));
    for(uint32_t i=0;i<k;++i){bench_spawn(&b,i,exe);struct matrix_config c;memset(&c,0,sizeof(c));c.b.grants=1;c.b.index=i;c.b.producers=o->producers;c.b.consumers=o->consumers;c.b.count=o->count;c.b.warmup=o->warmup;c.b.requested_cpu=o->cpu_count?o->cpus[i%o->cpu_count]:-1;c.b.affinity_tag=o->affinity_tag;c.b.qos=o->qos;strcpy(c.b.directory,dir);strcpy(c.b.control_path,b.control_path);c.payload=m->payload;c.arrival=m->arrival;c.burst=m->burst;c.os_yield=m->os_yield;c.period_ns=m->period_ns;c.hold_ns=m->hold_ns;c.hold_every=m->hold_every;bench_issue(&b,0,i,i,&c.b.grant[0]);bench_write_all(b.child[i].command_fd,&c,sizeof(c));}
    struct bench_result r[BENCH_MAX_WORKERS];memset(r,0,sizeof(r));for(uint32_t i=0;i<k;++i)bench_get_event(&b,i,B_READY,&r[i]);uint64_t cmd=1;
    for(uint32_t i=0;i<k;++i)bench_write_all(b.child[i].command_fd,&cmd,sizeof(cmd));
    drain(&b,o->producers,1,B_WARM_DONE,r);
    bench_reconcile(&b.first[0],o->warmup,dir,"warmup-reconcile.json");
    struct bench_clock clock;bench_clock_init(&clock);uint64_t t0=bench_future_tick(&clock,100000000);cmd=t0;for(uint32_t i=0;i<k;++i)bench_write_all(b.child[i].command_fd,&cmd,sizeof(cmd));
    drain(&b,o->producers,2,B_MEASURE_DONE,r);uint64_t end=0,total=0;
    for(uint32_t i=0;i<k;++i){B_CHECK(r[i].start_tick>=t0&&r[i].end_tick>=r[i].start_tick&&r[i].clock.numer==clock.numer&&r[i].clock.denom==clock.denom);if(i<o->producers)B_CHECK(r[i].count==o->count/o->producers);else{total+=r[i].count;if(r[i].end_tick>end)end=r[i].end_tick;}}
    B_CHECK(total==o->count&&end>t0);cmd=0;for(uint32_t i=0;i<k;++i)bench_write_all(b.child[i].command_fd,&cmd,sizeof(cmd));for(uint32_t i=0;i<k;++i){bench_get_event(&b,i,B_FINAL,&r[i]);B_OK(elite_object_ack_cleanup(b.object[0],&r[i].receipts[0]));}
    struct bench_stats stats[3];summary(m,dir,r,stats);bench_reconcile(&b.first[0],o->warmup+o->count,dir,"final-reconcile.json");bench_cohort_finish(&b);B_CHECK(elite_topology_discover(&after)==0);
    char path[4096],hash[65];uint64_t size;B_CHECK(bench_executable_path(path,sizeof(path))==0&&bench_sha256_file(path,hash,&size)==0);
    B_CHECK(size==before_size&&!strcmp(hash,before_hash));
    FILE *f=bench_json_open(dir,"result.json");fprintf(f,"{\"schema\":\"elite-matrix-v1\",\"status\":\"PASS_WITHIN_SCOPE\",\"runtime\":\"native_c\",\"mode\":\"%s\",\"messages\":%"PRIu64",\"warmup_messages\":%"PRIu64",\"producers\":%u,\"consumers\":%u,\"capacity\":%"PRIu64",\"payload_bytes\":%u,\"checksum\":%u,\"arrival\":%u,\"burst\":%u,\"period_ns\":%"PRIu64",\"hold_ns\":%"PRIu64",\"hold_every\":%"PRIu64",\"os_yield\":%u,\"t0\":%"PRIu64",\"end\":%"PRIu64",\"delta_ticks\":%"PRIu64",\"segment_bytes\":%"PRIu64",",o->mode==ELITE_SPSC?"spsc":"ncq",o->count,o->warmup,o->producers,o->consumers,o->capacity,m->payload,m->checksum,m->arrival,m->burst,m->period_ns,m->hold_ns,m->hold_every,m->os_yield,t0,end,end-t0,info.segment_bytes);bench_clock_json(f,&clock);
    fputs(",\"elapsed_ns\":",f);bench_json_ratio(f,end-t0,clock.numer,clock.denom,1);fputs(",\"messages_per_second\":",f);bench_json_ratio(f,o->count,UINT64_C(1000000000)*clock.denom,end-t0,clock.numer);fputs(",\"delivered_GB_per_second\":",f);bench_json_ratio(f,o->count*m->payload,clock.denom,end-t0,clock.numer);
    fputs(",\"locality_request\":",f);bench_json_string(f,m->locality);fputs(",\"placement_claim\":\"REQUIRES_OFFLINE_TOPOLOGY_CHECK\",\"memory_placement\":\"INITIALIZER_FIRST_TOUCH_NOT_VERIFIED\",\"physical_cache_residency\":\"NOT_ESTABLISHED\",\"absolute_clock_uncertainty\":\"NOT_QUALIFIED\",\"hardware_topology\":",f);B_CHECK(elite_topology_json(f,top)==0);fputs(",\"hardware_topology_after\":",f);B_CHECK(elite_topology_json(f,after)==0);
    fputs(",\"binary_sha256\":",f);bench_json_string(f,hash);fprintf(f,",\"binary_bytes\":%"PRIu64",\"build_identity\":",size);bench_build_identity_json(f);
    fputs(",\"native_latency\":",f);bench_stats_json(f,&stats[0],&clock);fputs(",\"offered_latency\":",f);bench_stats_json(f,&stats[1],&clock);fputs(",\"returned_latency\":",f);bench_stats_json(f,&stats[2],&clock);
    fputs(",\"workers_info\":[",f);for(uint32_t i=0;i<k;++i){fprintf(f,"%s{\"index\":%u,\"count\":%"PRIu64",\"empty_polls\":%"PRIu64",\"start_tick\":%"PRIu64",\"end_tick\":%"PRIu64",\"placement\":",i?",":"",i,r[i].count,r[i].empty_polls,r[i].start_tick,r[i].end_tick);bench_placement_json(f,&r[i].placement);fputs(",\"resources\":",f);bench_usage_json(f,&r[i].usage_before,&r[i].usage_after);fputc('}',f);}
    fputs("],\"exact_membership\":true,\"all_tokens_returned\":true,\"no_payload_in_control_channel\":true,\"warmup_same_generation\":true,\"trace_format\":\"little-endian-u64-triples\"}\n",f);bench_json_close(f);elite_topology_free(top);elite_topology_free(after);
    printf("PASS MATRIX %s %u/%u count=%"PRIu64" payload=%u trial=%u\n",o->mode==ELITE_SPSC?"spsc":"ncq",o->producers,o->consumers,o->count,m->payload,trial_id);
}
int main(int argc,char **argv)
{
    if(argc==2&&!strcmp(argv[1],"--worker"))return worker();
    struct matrix_options m;memset(&m,0,sizeof(m));m.payload=64;m.burst=32;m.period_ns=100000;m.hold_every=16;m.max_bytes=UINT64_C(536870912);strcpy(m.locality,"unconstrained");
    char **filtered=calloc((size_t)argc+1,sizeof(char *));B_CHECK(filtered);int count=1;filtered[0]=argv[0];
    for(int i=1;i<argc;++i){const char *s=argv[i];bool special=!strcmp(s,"--payload")||!strcmp(s,"--checksum")||!strcmp(s,"--arrival")||!strcmp(s,"--period-ns")||!strcmp(s,"--burst")||!strcmp(s,"--hold-ns")||!strcmp(s,"--hold-every")||!strcmp(s,"--os-yield")||!strcmp(s,"--max-bytes")||!strcmp(s,"--locality");
        if(!special){filtered[count++]=argv[i];continue;}B_CHECK(i+1<argc);const char *v=argv[++i];
        if(!strcmp(s,"--locality")){B_CHECK(strlen(v)<sizeof(m.locality));strcpy(m.locality,v);}
        else if(!strcmp(s,"--payload"))m.payload=(uint32_t)argnum(v,67108864);
        else if(!strcmp(s,"--checksum"))m.checksum=(uint32_t)argnum(v,1);
        else if(!strcmp(s,"--arrival"))m.arrival=(uint32_t)argnum(v,3);
        else if(!strcmp(s,"--burst"))m.burst=(uint32_t)argnum(v,1024);
        else if(!strcmp(s,"--period-ns"))m.period_ns=argnum(v,UINT64_C(1000000000));
        else if(!strcmp(s,"--hold-ns"))m.hold_ns=argnum(v,UINT64_C(1000000000));
        else if(!strcmp(s,"--hold-every"))m.hold_every=argnum(v,BENCH_MAX_ITEMS);
        else if(!strcmp(s,"--os-yield"))m.os_yield=(uint32_t)argnum(v,1);
        else m.max_bytes=argnum(v,UINT64_C(2147483648));
    }
    B_CHECK(m.payload>=8&&m.payload%8==0&&m.burst&&m.period_ns&&m.hold_every&&m.max_bytes);bench_parse(count,filtered,false,&m.base);free(filtered);
    B_CHECK(m.base.count<=1000000&&m.base.warmup<=1000000); /* explicit bounded raw-trace budget */
    for(unsigned i=0;i<m.base.trials;++i)trial(argv[0],&m,i);
    return 0;
}
