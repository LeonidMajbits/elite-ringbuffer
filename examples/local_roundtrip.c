/* Minimal single-process demonstration of two independently mapped endpoints.
 * Native IPC stress with spawn/exec is in tests/test_ipc.c. */
#include "elite_ringbuffer.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define TRY(call) do { elite_result r_=(call); if(r_.status!=ELITE_OK) { \
 fprintf(stderr,"%s: %s (outcome=%u, os=%d)\n",#call,elite_status_string(r_.status),r_.outcome,r_.os_error); return 1; }} while(0)
int main(void)
{
    uint8_t host[16]={1},authority[16]={2};struct timespec now;
    if(clock_gettime(CLOCK_REALTIME,&now)!=0)return 1;
    uint64_t identity=(uint64_t)now.tv_sec^(uint64_t)now.tv_nsec^((uint64_t)(unsigned long)getpid()<<32);
    memcpy(authority+8,&identity,8);
    /* A real application supplies nonreused host/authority identities and keeps
     * the bounded manager alive; these demo values are not a security token. */
    elite_authority *a=NULL;elite_object *o=NULL;elite_connection *p=NULL,*c=NULL;
    elite_config cfg={ELITE_SPSC,ELITE_POLL_ONLY,ELITE_CHECKSUM_NONE,64,32,1,1,0};
    elite_endpoint_definition d[2]={{{1},{9},ELITE_PRODUCER},{{2},{9},ELITE_CONSUMER}};
    TRY(elite_authority_create(host,authority,UINT64_C(1048576),&a));
    TRY(elite_create(a,&cfg,d,&o));TRY(elite_object_activate(o));
    elite_grant pg,cg;
    TRY(elite_object_register_process(o,0,getpid()));TRY(elite_object_register_process(o,1,getpid()));
    TRY(elite_object_grant(o,0,&pg));TRY(elite_object_grant(o,1,&cg));
    TRY(elite_attach(&pg,&p));TRY(elite_attach(&cg,&c));
    elite_lease w,r;elite_write_span ws;elite_read_span rs;
    TRY(elite_write_reserve(p,&w,&ws));
    /* Construct directly in shared storage. No intermediate transport buffer. */
    unsigned char *bytes=ws.data;bytes[0]='O';bytes[1]='K';
    TRY(elite_write_commit(p,&w,2,1,42));
    TRY(elite_read_borrow(c,&r,&rs));printf("message %llu: %.*s\n",(unsigned long long)rs.message_id,(int)rs.length,(const char *)rs.data);
    TRY(elite_read_release(c,&r));
    elite_cleanup_receipt pr,cr;
    TRY(elite_detach(&p,&pr));TRY(elite_object_ack_cleanup(o,&pr));
    TRY(elite_detach(&c,&cr));TRY(elite_object_ack_cleanup(o,&cr));
    TRY(elite_object_destroy(&o));TRY(elite_authority_destroy(&a));return 0;
}
