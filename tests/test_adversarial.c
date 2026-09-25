/* Controlled histories. Same-process tests intentionally rebind all endpoint
 * accesses to ONE virtual mapping for the TSan core lane. Original independent
 * mappings are retained, then restored before normal public detach. This is
 * not a production shared-mapping optimization or an interprocess TSan claim. */
#include "test_support.h"
#include "elite_internal.h"
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <sys/mman.h>

struct cohort {
    elite_authority *a;elite_object *o;elite_connection *c[4];elite_grant g[4];
    void *original[4];uint32_t k;
};
static void bind_mapping(elite_connection *c,void *b)
{
    c->mapping=b;unsigned char *p=b;
    c->gate=&((struct elite_cell64 *)(p+512))->value;
    c->failure=&((struct elite_cell64 *)(p+640))->value;
    c->participant=&((struct elite_participant_record *)(p+ELITE_QUANTUM))[c->receipt.endpoint_index];
    c->descriptors=(struct elite_slot_descriptor *)(p+c->info.descriptors_offset);
    c->payloads=p+c->info.payloads_offset;
    if(c->info.layout_profile==ELITE_SPSC)c->spsc=b;
    else {c->ncq=b;c->qf=(struct elite_ncq_entry_cell *)(p+c->info.qf_entries_offset);c->qr=(struct elite_ncq_entry_cell *)(p+c->info.qr_entries_offset);}
}
static void setup(struct cohort *x,uint32_t mode,uint32_t p,uint32_t c,uint32_t crc,uint32_t wait)
{
    memset(x,0,sizeof(*x));x->k=p+c;CHECK(x->k<=4);test_authority(&x->a);
    elite_endpoint_definition d[4];test_definitions(d,p,c);elite_config cfg=test_config(mode,p,c,4);
    cfg.payload_checksum_mode=crc;cfg.wait_mode=wait;
    OK(elite_create(x->a,&cfg,d,&x->o));OK(elite_object_activate(x->o));
    for(uint32_t i=0;i<x->k;++i){test_attach_self(x->o,i,&x->c[i],&x->g[i]);x->original[i]=x->c[i]->mapping;}
    for(uint32_t i=1;i<x->k;++i)bind_mapping(x->c[i],x->original[0]);
}
static void finish(struct cohort *x)
{
    for(uint32_t i=0;i<x->k;++i){bind_mapping(x->c[i],x->original[i]);test_detach(x->o,&x->c[i]);}
    OK(elite_object_destroy(&x->o));OK(elite_authority_destroy(&x->a));
}
static void send_one(elite_connection *c,uint64_t id)
{
    elite_lease l;elite_write_span w;OK(elite_write_reserve(c,&l,&w));
    test_payload_write(w.data,id,0,id,17);OK(elite_write_commit(c,&l,64,7,id));
}
static uint64_t receive_one(elite_connection *c)
{
    elite_lease l;elite_read_span s;OK(elite_read_borrow(c,&l,&s));
    uint64_t id=s.message_id;CHECK(test_payload_check(s.data,id,0,id,17));OK(elite_read_release(c,&l));return id;
}
struct pause_state {_Atomic int reached;_Atomic int resume;uint32_t point;bool fired;};
static void pause_at(void *arg,uint32_t point,uint64_t ticket,uint64_t block)
{
    struct pause_state *p=arg;(void)ticket;(void)block;
    if(point!=p->point||p->fired)return;
    p->fired=true;atomic_store_explicit(&p->reached,1,memory_order_release);
    while(!atomic_load_explicit(&p->resume,memory_order_acquire))(void)sched_yield();
}
static void wait_reached(struct pause_state *p)
{while(!atomic_load_explicit(&p->reached,memory_order_acquire))(void)sched_yield();}
static void pause_init(struct pause_state *p,uint32_t point)
{atomic_init(&p->reached,0);atomic_init(&p->resume,0);p->point=point;p->fired=false;}
struct task {elite_connection *c;uint64_t id;uint32_t receive;};
static void *task_run(void *arg)
{struct task *t=arg;if(t->receive)t->id=receive_one(t->c);else send_one(t->c,t->id);return NULL;}
static void publication_history(uint32_t point)
{
    struct cohort x;setup(&x,ELITE_NCQ,2,1,0,0);
    struct pause_state pause;pause_init(&pause,point);OK(elite_test_set_hook(x.c[0],pause_at,&pause));
    struct task t={x.c[0],UINT64_C(999999),0};pthread_t thread;CHECK(pthread_create(&thread,NULL,task_run,&t)==0);wait_reached(&pause);
    if(point==ELITE_HOOK_QR_AFTER_INSTALL)CHECK(receive_one(x.c[2])==999999);
    for(uint64_t i=0;i<10000;++i){send_one(x.c[1],i);CHECK(receive_one(x.c[2])==i);}
    atomic_store_explicit(&pause.resume,1,memory_order_release);CHECK(pthread_join(thread,NULL)==0);
    if(point==ELITE_HOOK_RESERVED)CHECK(receive_one(x.c[2])==999999);
    finish(&x);printf("PASS controlled publication point=%u healthy_lifecycles_while_paused=10000\n",point);
}
static void stale_head_history(void)
{
    struct cohort x;setup(&x,ELITE_NCQ,1,2,0,0);send_one(x.c[0],1);
    struct pause_state pause;pause_init(&pause,ELITE_HOOK_HEAD_OBSERVED);OK(elite_test_set_hook(x.c[1],pause_at,&pause));
    struct task t={x.c[1],0,1};pthread_t thread;CHECK(pthread_create(&thread,NULL,task_run,&t)==0);wait_reached(&pause);
    CHECK(receive_one(x.c[2])==1);
    for(uint64_t i=2;i<102;++i){send_one(x.c[0],i);CHECK(receive_one(x.c[2])==i);}
    send_one(x.c[0],8888);atomic_store_explicit(&pause.resume,1,memory_order_release);
    CHECK(pthread_join(thread,NULL)==0);CHECK(t.id==8888);
    finish(&x);puts("PASS controlled stale_head discards_old_entry_after_101_competing_claims");
}
static void returned_block_history(void)
{
    struct cohort x;setup(&x,ELITE_NCQ,1,2,0,0);send_one(x.c[0],1);
    struct pause_state pause;pause_init(&pause,ELITE_HOOK_QF_AFTER_INSTALL);OK(elite_test_set_hook(x.c[1],pause_at,&pause));
    struct task t={x.c[1],0,1};pthread_t thread;CHECK(pthread_create(&thread,NULL,task_run,&t)==0);wait_reached(&pause);
    for(uint64_t i=2;i<10002;++i){send_one(x.c[0],i);CHECK(receive_one(x.c[2])==i);}
    atomic_store_explicit(&pause.resume,1,memory_order_release);CHECK(pthread_join(thread,NULL)==0);CHECK(t.id==1);
    finish(&x);puts("PASS controlled late_returner no_post_LP_descriptor_write");
}
struct corruption {unsigned char *byte;};
static void corrupt_owned(void *arg,uint32_t point,uint64_t t,uint64_t b)
{(void)t;(void)b;if(point==ELITE_HOOK_CHECKSUM_READY)((struct corruption *)arg)->byte[7]^=1;}
static void checksum_history(uint32_t mode)
{
    struct cohort x;setup(&x,mode,1,1,1,0);
    elite_lease w,r;elite_write_span ws;elite_read_span rs;OK(elite_write_reserve(x.c[0],&w,&ws));
    test_payload_write(ws.data,7,0,7,17);struct corruption arg={ws.data};OK(elite_test_set_hook(x.c[0],corrupt_owned,&arg));
    OK(elite_write_commit(x.c[0],&w,64,7,7));elite_result result=elite_read_borrow(x.c[1],&r,&rs);
    CHECK(result.status==ELITE_INTEGRITY&&result.outcome==ELITE_RETAINED&&rs.data==NULL);
    CHECK(elite_read_release(x.c[1],&r).status==ELITE_RETIRED);OK(elite_abandon_retained(x.c[1],&r));
    finish(&x);printf("PASS integrity mode=%u retained_corrupt_token_not_reclaimed\n",mode);
}
struct waiter {elite_connection *c;elite_result result;};
static void *waiter_run(void *arg)
{struct waiter *w=arg;w->result=elite_wait_data(w->c,UINT64_C(1000000000));return NULL;}
static void waiting_history(void)
{
    struct cohort x;setup(&x,ELITE_SPSC,1,1,0,ELITE_PARKABLE_SPSC);
    CHECK(elite_wait_data(x.c[1],0).status==ELITE_NO_DATA_OBSERVED);
    CHECK(elite_wait_data(x.c[1],UINT64_MAX).status==ELITE_INVALID_ARGUMENT);
    CHECK(elite_wait_data(x.c[1],UINT64_C(1000000)).status==ELITE_NO_DATA_OBSERVED);
    struct waiter w={x.c[1],{0,0,0,0}};pthread_t th;CHECK(pthread_create(&th,NULL,waiter_run,&w)==0);
    while(atomic_load_explicit(&x.c[0]->spsc->data_wait.value,memory_order_acquire)!=1)(void)sched_yield();
    send_one(x.c[0],55);CHECK(pthread_join(th,NULL)==0);CHECK(w.result.status==ELITE_OK);CHECK(receive_one(x.c[1])==55);
    CHECK(pthread_create(&th,NULL,waiter_run,&w)==0);
    while(atomic_load_explicit(&x.c[0]->spsc->data_wait.value,memory_order_acquire)!=1)(void)sched_yield();
    OK(elite_retire(x.c[0],0));CHECK(pthread_join(th,NULL)==0);CHECK(w.result.status==ELITE_RETIRED);
    finish(&x);puts("PASS SPSC finite_wait wake_before_or_after_block and_retirement");
}
/* One common virtual mapping, continuously concurrent 2P/2C data path. */
struct stress_context {elite_connection *c;_Atomic unsigned *done;uint64_t base;uint64_t received;unsigned char *seen;};
static void *stress_producer(void *arg)
{
    struct stress_context *x=arg;
    for(uint64_t i=0;i<50000;) {
        elite_lease l;elite_write_span w;elite_result r=elite_write_reserve(x->c,&l,&w);
        if(r.status==ELITE_NO_CAPACITY_OBSERVED){(void)sched_yield();continue;}
        CHECK(r.status==ELITE_OK);uint64_t id=x->base+i;test_payload_write(w.data,id,0,id,17);
        OK(elite_write_commit(x->c,&l,64,7,id));++i;
    }
    (void)atomic_fetch_add_explicit(x->done,1,memory_order_release);return NULL;
}
static void *stress_consumer(void *arg)
{
    struct stress_context *x=arg;
    for(;;) {
        elite_lease l;elite_read_span s;elite_result r=elite_read_borrow(x->c,&l,&s);
        if(r.status==ELITE_NO_DATA_OBSERVED){
            if(atomic_load_explicit(x->done,memory_order_acquire)==2) {
                r=elite_read_borrow(x->c,&l,&s);
                if(r.status==ELITE_NO_DATA_OBSERVED)break;
            } else {(void)sched_yield();continue;}
        }
        CHECK(r.status==ELITE_OK);uint64_t id=s.message_id;CHECK(id<100000&&test_payload_check(s.data,id,0,id,17));
        CHECK(!x->seen[id]);x->seen[id]=1;++x->received;OK(elite_read_release(x->c,&l));
    }
    return NULL;
}
static void threaded_stress(void)
{
    struct cohort x;setup(&x,ELITE_NCQ,2,2,0,0);_Atomic unsigned done;atomic_init(&done,0);
    struct stress_context ctx[4];pthread_t t[4];memset(ctx,0,sizeof(ctx));
    for(unsigned i=0;i<4;++i){ctx[i].c=x.c[i];ctx[i].done=&done;ctx[i].base=(uint64_t)i*50000;
        if(i>=2){ctx[i].seen=calloc(100000,1);CHECK(ctx[i].seen!=NULL);}
        CHECK(pthread_create(&t[i],NULL,i<2?stress_producer:stress_consumer,&ctx[i])==0);}
    for(unsigned i=0;i<4;++i)CHECK(pthread_join(t[i],NULL)==0);
    CHECK(ctx[2].received+ctx[3].received==100000);
    for(unsigned i=0;i<100000;++i)CHECK((unsigned)ctx[2].seen[i]+ctx[3].seen[i]==1);
    free(ctx[2].seen);free(ctx[3].seen);finish(&x);puts("PASS one_mapping_threads 2P2C messages=100000 exact_membership=yes");
}
int main(int argc,char **argv)
{
    (void)argc;(void)argv;(void)alarm(120);
    threaded_stress();publication_history(ELITE_HOOK_RESERVED);publication_history(ELITE_HOOK_QR_AFTER_INSTALL);
    stale_head_history();returned_block_history();checksum_history(ELITE_SPSC);checksum_history(ELITE_NCQ);waiting_history();
    puts("PASS adversarial_histories=8");return 0;
}
