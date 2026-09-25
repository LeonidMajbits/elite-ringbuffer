/* Native spawn/exec IPC integration stress. Exact per-consumer membership,
 * no shared instrumentation on the message path. This is NOT the complete
 * Turn 5 five-restart/warmup/metrology qualification harness. */
#include "test_support.h"
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <spawn.h>
#include <sys/mman.h>
#include <sys/wait.h>
extern char **environ;

struct worker_config { elite_grant grant; uint64_t total; uint64_t seed; uint32_t producers; uint32_t index; };
struct worker_result { uint64_t count; uint64_t bitmap_bytes; elite_cleanup_receipt receipt; };
static pid_t children[32];
static char owned_name[ELITE_NAME_BYTES];
static void cleanup_children(void)
{
    for(unsigned i=0;i<32;++i) if(children[i]>0) (void)kill(children[i],SIGKILL);
    for(unsigned i=0;i<32;++i) if(children[i]>0) {while(waitpid(children[i],NULL,0)<0&&errno==EINTR){} children[i]=0;}
    if(owned_name[0]) (void)shm_unlink(owned_name);
}
static int worker(void)
{
    (void)alarm(600);
    struct worker_config cfg;test_read_all(STDIN_FILENO,&cfg,sizeof(cfg));
    elite_connection *c=NULL;OK(elite_attach(&cfg.grant,&c));
    struct elite_immutable_header info;OK(elite_get_info(c,&info));
    bool parked=info.wait_mode==ELITE_PARKABLE_SPSC;
    bool producer=cfg.grant.role==ELITE_PRODUCER;
    uint64_t quota=cfg.total/cfg.producers;
    size_t bitmap_bytes=(size_t)((cfg.total+7)/8);
    unsigned char *bitmap=producer?NULL:calloc(bitmap_bytes,1);CHECK(producer||bitmap!=NULL);
    unsigned char ready=1;test_write_all(STDOUT_FILENO,&ready,1);test_read_all(STDIN_FILENO,&ready,1);CHECK(ready==1);
    uint64_t count=0;unsigned idle=0;
    if(producer) {
        for(uint64_t seq=0;seq<quota;) {
            elite_lease l;elite_write_span span;elite_result r=elite_write_reserve(c,&l,&span);
            if(r.status==ELITE_NO_CAPACITY_OBSERVED) {if(parked){r=elite_wait_space(c,1000000);CHECK(r.status==ELITE_OK||r.status==ELITE_NO_CAPACITY_OBSERVED);}else if(++idle%1024==0)(void)sched_yield();continue;}
            CHECK(r.status==ELITE_OK);idle=0;
            uint64_t id=(uint64_t)cfg.index*quota+seq;
            test_payload_write(span.data,id,cfg.index,seq,cfg.seed);
            r=elite_write_commit(c,&l,64,7,id);CHECK(r.status==ELITE_OK&&r.outcome==ELITE_PUBLISHED);
            ++seq;++count;
        }
    } else {
        int flags=fcntl(STDIN_FILENO,F_GETFL);CHECK(flags>=0);CHECK(fcntl(STDIN_FILENO,F_SETFL,flags|O_NONBLOCK)==0);
        bool drain=false;
        for(;;) {
            elite_lease l;elite_read_span span;elite_result r=elite_read_borrow(c,&l,&span);
            if(r.status==ELITE_NO_DATA_OBSERVED) {
                if(drain) break;
                unsigned char command=0;ssize_t n=read(STDIN_FILENO,&command,1);
                if(n==1) {CHECK(command==2);drain=true;}
                else CHECK(n<0 && (errno==EAGAIN||errno==EINTR));
                if(parked&&!drain){r=elite_wait_data(c,1000000);CHECK(r.status==ELITE_OK||r.status==ELITE_NO_DATA_OBSERVED);}
                else if(++idle%1024==0)(void)sched_yield();
                continue;
            }
            CHECK(r.status==ELITE_OK);idle=0;uint64_t id=span.message_id;
            CHECK(id<cfg.total&&span.length==64&&span.message_type==7);
            CHECK(test_payload_check(span.data,id,id/quota,id%quota,cfg.seed));
            size_t byte=(size_t)(id/8);unsigned char bit=(unsigned char)(1u<<(unsigned)(id%8));
            CHECK((bitmap[byte]&bit)==0);bitmap[byte]|=bit;
            r=elite_read_release(c,&l);CHECK(r.status==ELITE_OK&&r.outcome==ELITE_RETURNED);++count;
        }
    }
    struct worker_result result;memset(&result,0,sizeof(result));result.count=count;
    result.bitmap_bytes=producer?0:bitmap_bytes;
    OK(elite_detach(&c,&result.receipt));
    test_write_all(STDOUT_FILENO,&result,sizeof(result));
    if(!producer) test_write_all(STDOUT_FILENO,bitmap,bitmap_bytes);
    free(bitmap);return 0;
}
static void reconcile(const elite_grant *g,uint64_t total)
{
    int fd=shm_open(g->name,O_RDWR,0);CHECK(fd>=0);
    void *b=mmap(NULL,(size_t)g->segment_bytes,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);CHECK(b!=MAP_FAILED);
    struct elite_immutable_header info;OK(elite_validate_prefix(b,512,g->segment_bytes,&info));
    if(info.layout_profile==ELITE_SPSC) {
        struct elite_spsc_ring_header *h=b;
        CHECK(atomic_load_explicit(&h->published.value,memory_order_acquire)==total);
        CHECK(atomic_load_explicit(&h->reclaimed.value,memory_order_acquire)==total);
    } else {
        struct elite_mpmc_ncq_header *h=b;uint64_t n=info.capacity;
        uint64_t head=atomic_load_explicit(&h->qf_head.value,memory_order_seq_cst);
        uint64_t tail=atomic_load_explicit(&h->qf_tail.value,memory_order_seq_cst);CHECK(tail-head==n);
        CHECK(atomic_load_explicit(&h->qr_head.value,memory_order_seq_cst)==atomic_load_explicit(&h->qr_tail.value,memory_order_seq_cst));
        struct elite_ncq_entry_cell *q=(struct elite_ncq_entry_cell *)((unsigned char *)b+info.qf_entries_offset);
        struct elite_slot_descriptor *d=(struct elite_slot_descriptor *)((unsigned char *)b+info.descriptors_offset);
        unsigned char *seen=calloc((size_t)n,1);CHECK(seen!=NULL);
        for(uint64_t t=head;t<tail;++t) {
            uint64_t e=atomic_load_explicit(&q[t&(n-1)].cycle_index,memory_order_seq_cst),index=e&(n-1);
            CHECK((e&~(n-1))==(t&~(n-1)));CHECK(!seen[index]);seen[index]=1;
            uint64_t s=atomic_load_explicit(&d[index].status_word,memory_order_acquire);
            CHECK((s&3)==ELITE_EMPTY&&d[index].epoch==(s>>2));
        }
        free(seen);
    }
    CHECK(munmap(b,(size_t)g->segment_bytes)==0);CHECK(close(fd)==0);
}
int main(int argc,char **argv)
{
    if(argc==2 && strcmp(argv[1],"worker")==0) return worker();
    if(argc!=5) {fprintf(stderr,"usage: %s spsc|park|ncq producers consumers total_messages\n",argv[0]);return 2;}
    uint32_t mode=strcmp(argv[1],"ncq")==0?ELITE_NCQ:ELITE_SPSC;
    bool park=strcmp(argv[1],"park")==0;
    unsigned long pv=strtoul(argv[2],NULL,10),cv=strtoul(argv[3],NULL,10);
    uint64_t total=strtoull(argv[4],NULL,10);
    CHECK(pv>=1&&cv>=1&&pv+cv<=32&&total>0&&total<=UINT64_C(100000000)&&total%pv==0);
    uint32_t p=(uint32_t)pv,c=(uint32_t)cv,k=p+c;
    CHECK(mode!=ELITE_SPSC||(p==1&&c==1));CHECK(atexit(cleanup_children)==0);
    (void)alarm(600);
    elite_authority *a=NULL;elite_object *o=NULL;test_authority(&a);
    elite_endpoint_definition defs[32];test_definitions(defs,p,c);
    for(uint32_t i=0;i<k;++i) test_id(defs[i].process_incarnation_id,(uint64_t)i+1000);
    elite_config config=test_config(mode,p,c,1024);if(park)config.wait_mode=ELITE_PARKABLE_SPSC;OK(elite_create(a,&config,defs,&o));OK(elite_object_activate(o));
    int to_child[32],from_child[32];elite_grant first;memset(&first,0,sizeof(first));
    for(uint32_t i=0;i<k;++i) {
        int input[2],output[2];CHECK(pipe(input)==0&&pipe(output)==0);
        for(unsigned n=0;n<2;++n) {CHECK(fcntl(input[n],F_SETFD,FD_CLOEXEC)==0);CHECK(fcntl(output[n],F_SETFD,FD_CLOEXEC)==0);}
        posix_spawn_file_actions_t act;CHECK(posix_spawn_file_actions_init(&act)==0);
        CHECK(posix_spawn_file_actions_adddup2(&act,input[0],STDIN_FILENO)==0);
        CHECK(posix_spawn_file_actions_adddup2(&act,output[1],STDOUT_FILENO)==0);
        char *args[]={argv[0],(char *)"worker",NULL};
        CHECK(posix_spawn(&children[i],argv[0],&act,NULL,args,environ)==0);
        CHECK(posix_spawn_file_actions_destroy(&act)==0);
        CHECK(close(input[0])==0&&close(output[1])==0);to_child[i]=input[1];from_child[i]=output[0];
        OK(elite_object_register_process(o,i,children[i]));
        struct worker_config wc;memset(&wc,0,sizeof(wc));OK(elite_object_grant(o,i,&wc.grant));
        if(i==0){first=wc.grant;memcpy(owned_name,wc.grant.name,ELITE_NAME_BYTES);}
        wc.total=total;wc.seed=UINT64_C(0x5bb43c0039172ae1);wc.producers=p;wc.index=i;
        test_write_all(to_child[i],&wc,sizeof(wc));
    }
    for(uint32_t i=0;i<k;++i){unsigned char ready=0;test_read_all(from_child[i],&ready,1);CHECK(ready==1);}
    uint64_t start=test_now();unsigned char command=1;
    for(uint32_t i=0;i<k;++i)test_write_all(to_child[i],&command,1);
    uint64_t produced=0,received=0;
    for(uint32_t i=0;i<p;++i){struct worker_result r;test_read_all(from_child[i],&r,sizeof(r));CHECK(r.bitmap_bytes==0);produced+=r.count;OK(elite_object_ack_cleanup(o,&r.receipt));}
    command=2;for(uint32_t i=p;i<k;++i)test_write_all(to_child[i],&command,1);
    size_t bytes=(size_t)((total+7)/8);unsigned char *all=calloc(bytes,1),*one=malloc(bytes);CHECK(all!=NULL&&one!=NULL);
    for(uint32_t i=p;i<k;++i){struct worker_result r;test_read_all(from_child[i],&r,sizeof(r));CHECK(r.bitmap_bytes==bytes);test_read_all(from_child[i],one,bytes);
        uint64_t pop=0;
        for(size_t j=0;j<bytes;++j){CHECK((all[j]&one[j])==0);all[j]|=one[j];unsigned char v=one[j];while(v){pop+=v&1u;v>>=1;}}
        CHECK(pop==r.count);received+=r.count;OK(elite_object_ack_cleanup(o,&r.receipt));}
    uint64_t elapsed=test_now()-start;
    CHECK(produced==total&&received==total);
    for(uint64_t i=0;i<total;++i)CHECK((all[i/8]&(1u<<(unsigned)(i%8)))!=0);
    for(uint32_t i=0;i<k;++i){int s=0;CHECK(waitpid(children[i],&s,0)==children[i]);children[i]=0;CHECK(WIFEXITED(s)&&WEXITSTATUS(s)==0);CHECK(close(to_child[i])==0&&close(from_child[i])==0);}
    reconcile(&first,total);OK(elite_object_destroy(&o));owned_name[0]='\0';OK(elite_authority_destroy(&a));
    printf("PASS native_spawn_ipc mode=%s producers=%u consumers=%u messages=%llu bytes_each=64 duplicates=0 missing=0 bad_payloads=0 all_tokens_returned=yes controller_elapsed_ns=%llu (not_latency)\n",mode==ELITE_SPSC?(park?"PARKABLE_SPSC":"SPSC"):"NCQ-SC64",p,c,(unsigned long long)total,(unsigned long long)elapsed);
    free(all);free(one);return 0;
}
