#ifndef ELITE_TEST_SUPPORT_H
#define ELITE_TEST_SUPPORT_H
#include "elite_ringbuffer.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define CHECK(x) do { if(!(x)) {fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); exit(1);} } while(0)
#define OK(x) do {elite_result _tr=(x); if(_tr.status!=ELITE_OK){fprintf(stderr,"FAIL %s:%d: %s -> %s outcome=%u os=%d\n",__FILE__,__LINE__,#x,elite_status_string(_tr.status),_tr.outcome,_tr.os_error);exit(1);}} while(0)
static inline uint64_t test_now(void)
{ struct timespec t; CHECK(clock_gettime(CLOCK_MONOTONIC,&t)==0); return (uint64_t)t.tv_sec*UINT64_C(1000000000)+(uint64_t)t.tv_nsec; }
static inline void test_id(uint8_t id[16],uint64_t salt)
{
    uint64_t a=test_now(),b=((uint64_t)(unsigned long)getpid()<<32)^salt;
    memcpy(id,&a,8); memcpy(id+8,&b,8);
}
static inline void test_authority(elite_authority **a)
{ uint8_t host[16],id[16];test_id(host,1);test_id(id,2);OK(elite_authority_create(host,id,UINT64_C(67108864),a)); }
static inline void test_definitions(elite_endpoint_definition *e,uint32_t p,uint32_t c)
{
    uint8_t incarnation[16];test_id(incarnation,5);
    for(uint32_t i=0;i<p+c;++i) {
        memset(&e[i],0,sizeof(e[i]));test_id(e[i].endpoint_id,(uint64_t)i+10);
        memcpy(e[i].process_incarnation_id,incarnation,16);e[i].role=i<p?ELITE_PRODUCER:ELITE_CONSUMER;
    }
}
static inline elite_config test_config(uint32_t mode,uint32_t p,uint32_t c,uint64_t n)
{ elite_config cfg={mode,ELITE_POLL_ONLY,ELITE_CHECKSUM_NONE,64,n,p,c,0};return cfg; }
static inline void test_attach_self(elite_object *o,uint32_t i,elite_connection **c,elite_grant *g)
{OK(elite_object_register_process(o,i,getpid()));OK(elite_object_grant(o,i,g));OK(elite_attach(g,c));}
static inline void test_detach(elite_object *o,elite_connection **c)
{elite_cleanup_receipt r;OK(elite_detach(c,&r));OK(elite_object_ack_cleanup(o,&r));}
static inline void test_write_all(int fd,const void *ptr,size_t size)
{
    const unsigned char *p=ptr;
    while(size) {ssize_t n=write(fd,p,size);if(n<0&&errno==EINTR)continue;CHECK(n>0);p+=(size_t)n;size-=(size_t)n;}
}
static inline void test_read_all(int fd,void *ptr,size_t size)
{
    unsigned char *p=ptr;
    while(size) {ssize_t n=read(fd,p,size);if(n<0&&errno==EINTR)continue;CHECK(n>0);p+=(size_t)n;size-=(size_t)n;}
}
static inline void test_payload_write(void *span,uint64_t id,uint64_t p,uint64_t s,uint64_t seed)
{
    uint64_t *w=span,v=((id<<17)|(id>>47))^seed;
    w[0]=id;w[1]=~id;w[2]=p;w[3]=s;w[4]=seed;w[5]=v;w[6]=~v;w[7]=id^UINT64_C(0x9e3779b97f4a7c15);
}
static inline bool test_payload_check(const void *span,uint64_t id,uint64_t p,uint64_t s,uint64_t seed)
{
    const uint64_t *w=span;uint64_t v=((id<<17)|(id>>47))^seed;
    return w[0]==id && w[1]==~id && w[2]==p && w[3]==s && w[4]==seed && w[5]==v && w[6]==~v && w[7]==(id^UINT64_C(0x9e3779b97f4a7c15));
}
#endif
