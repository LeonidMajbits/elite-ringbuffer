/* Native authority resource negatives. No runtime field is patched/reset. */
#include "test_support.h"
#include <inttypes.h>
#include <signal.h>
#include <sys/wait.h>
int main(void)
{
    (void)alarm(30);
    elite_authority *a=NULL;elite_object *o[4]={NULL,NULL,NULL,NULL},*fifth=NULL;
    test_authority(&a);elite_endpoint_definition d[2];test_definitions(d,1,1);
    elite_config cfg=test_config(ELITE_SPSC,1,1,4);
    elite_connection *held[2]={NULL,NULL};elite_grant g[2];elite_lease lease[2];elite_write_span span[2];
    for(unsigned i=0;i<2;++i){OK(elite_create(a,&cfg,d,&o[i]));OK(elite_object_activate(o[i]));
        test_attach_self(o[i],0,&held[i],&g[i]);OK(elite_write_reserve(held[i],&lease[i],&span[i]));
        memset(span[i].data,(int)(i+1),64);OK(elite_object_quarantine(o[i]));
        CHECK(elite_object_destroy(&o[i]).status==ELITE_BUSY);}
    OK(elite_create(a,&cfg,d,&o[2]));OK(elite_object_activate(o[2]));
    OK(elite_create(a,&cfg,d,&o[3]));
    elite_result q=elite_object_quarantine(o[2]);CHECK(q.status==ELITE_BUSY&&q.outcome==ELITE_RETAINED);
    elite_result c=elite_create(a,&cfg,d,&fifth);CHECK(c.status==ELITE_BUSY&&fifth==NULL);
    elite_result ad=elite_authority_destroy(&a);CHECK(ad.status==ELITE_BUSY&&a!=NULL);
    for(unsigned i=0;i<2;++i){for(unsigned j=0;j<64;++j)CHECK(((unsigned char *)span[i].data)[j]==(unsigned char)(i+1));
        OK(elite_abandon_retained(held[i],&lease[i]));test_detach(o[i],&held[i]);}
    for(unsigned i=0;i<4;++i)OK(elite_object_destroy(&o[i]));
    OK(elite_authority_destroy(&a));
    printf("{\"schema\":\"elite-chaos-resource-v1\",\"scope\":\"single_process_authority_and_live_lease_negative\",\"objects\":4,\"quarantines\":2,\"third_quarantine_status\":%u,\"fifth_create_status\":%u,\"premature_authority_destroy_status\":%u,\"retained_bytes_unchanged\":true,\"cleanup_complete\":true}\n",q.status,c.status,ad.status);
    return 0;
}
