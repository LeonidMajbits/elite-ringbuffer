/* Coherent, quiescent near-limit verification fixtures. They are NOT normal
 * constructor output and never mutate a concurrently operating generation. */
#include "test_support.h"
#include "elite_internal.h"
static void limit_case(uint32_t mode,bool epoch_limit)
{
    elite_authority *a=NULL;elite_object *o=NULL;elite_connection *p=NULL,*c=NULL;
    test_authority(&a);elite_endpoint_definition d[2];test_definitions(d,1,1);elite_config cfg=test_config(mode,1,1,4);
    OK(elite_create(a,&cfg,d,&o));OK(elite_object_activate(o));elite_grant pg,cg;test_attach_self(o,0,&p,&pg);test_attach_self(o,1,&c,&cg);
    elite_lease l;elite_write_span w;
    if(epoch_limit) {
        for(unsigned i=0;i<4;++i){p->descriptors[i].epoch=ELITE_EPOCH_CEILING;
            if(mode==ELITE_NCQ)atomic_store_explicit(&p->descriptors[i].status_word,ELITE_EPOCH_CEILING<<2,memory_order_release);}
        elite_result r=elite_write_reserve(p,&l,&w);CHECK(r.status==ELITE_COUNTER_LIMIT&&w.data==NULL);
        if(r.outcome==ELITE_RETAINED)OK(elite_abandon_retained(p,&l));
    } else if(mode==ELITE_SPSC) {
        uint64_t j=p->info.ticket_ceiling;p->cursor=j-1;c->cursor=j-1;p->cached_peer=j-1;c->cached_peer=j-1;
        atomic_store_explicit(&p->spsc->published.value,j-1,memory_order_release);
        atomic_store_explicit(&p->spsc->reclaimed.value,j-1,memory_order_release);
        OK(elite_write_reserve(p,&l,&w));OK(elite_write_commit(p,&l,0,0,0));
        elite_lease rl;elite_read_span rs;OK(elite_read_borrow(c,&rl,&rs));OK(elite_read_release(c,&rl));
        CHECK(elite_write_reserve(p,&l,&w).status==ELITE_COUNTER_LIMIT);
        CHECK(atomic_load_explicit(&p->spsc->published.value,memory_order_acquire)==j);
    } else {
        uint64_t n=4,j=p->info.ticket_ceiling,m=j-n;
        atomic_store_explicit(&p->ncq->qf_head.value,m,memory_order_seq_cst);
        atomic_store_explicit(&p->ncq->qf_tail.value,j,memory_order_seq_cst);
        atomic_store_explicit(&p->ncq->qr_head.value,j,memory_order_seq_cst);
        atomic_store_explicit(&p->ncq->qr_tail.value,j,memory_order_seq_cst);
        for(uint64_t t=m;t<j;++t){
            uint64_t b=t&(n-1);atomic_store_explicit(&p->qf[b].cycle_index,t,memory_order_seq_cst);
            atomic_store_explicit(&p->qr[b].cycle_index,t,memory_order_seq_cst);
            uint64_t epoch=(m+n-1-b)/n;p->descriptors[b].epoch=epoch;
            atomic_store_explicit(&p->descriptors[b].status_word,epoch<<2,memory_order_release);
        }
        uint64_t before=atomic_load_explicit(&p->qr[j&(n-1)].cycle_index,memory_order_seq_cst);
        OK(elite_write_reserve(p,&l,&w));elite_result r=elite_write_commit(p,&l,0,0,0);
        CHECK(r.status==ELITE_COUNTER_LIMIT&&r.outcome==ELITE_RETAINED);
        CHECK(atomic_load_explicit(&p->qr[j&(n-1)].cycle_index,memory_order_seq_cst)==before);
        CHECK(atomic_load_explicit(&p->ncq->qr_tail.value,memory_order_seq_cst)==j);
        OK(elite_abandon_retained(p,&l));
    }
    test_detach(o,&p);test_detach(o,&c);OK(elite_object_destroy(&o));OK(elite_authority_destroy(&a));
    printf("PASS near_limit mode=%u limit=%s no_wrap_and_no_terminal_publication\n",mode,epoch_limit?"EPOCH":"TICKET");
}
int main(void){for(uint32_t m=1;m<=2;++m){limit_case(m,false);limit_case(m,true);}return 0;}
