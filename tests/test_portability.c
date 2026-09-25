#include "elite_api.h"
#include <stdio.h>
#include <unistd.h>
int main(void)
{
    elite_authority *a=NULL;
    elite_object *o=NULL;
    const uint8_t h[16]={1},id[16]={2};
    elite_config cfg={ELITE_SPSC,ELITE_PARKABLE_SPSC,ELITE_CHECKSUM_NONE,64,32,1,1,0};
    elite_endpoint_definition defs[2]={{{1},{3},ELITE_PRODUCER},{{2},{3},ELITE_CONSUMER}};
    elite_result r=elite_authority_create(h,id,1048576,&a);
    if(r.status!=ELITE_OK) return 1;
    r=elite_create(a,&cfg,defs,&o);
#ifdef __FreeBSD__
    if(r.status!=ELITE_UNSUPPORTED || o!=NULL) return 2;
#else
    if(r.status!=ELITE_OK) return 3;
    if(elite_object_destroy(&o).status!=ELITE_OK) return 4;
#endif
    if(elite_authority_destroy(&a).status!=ELITE_OK) return 5;
    (void)puts("PASS platform contract (FreeBSD: polling only; unsupported parking rejected)");
    return 0;
}
