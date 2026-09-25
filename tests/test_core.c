#include "test_support.h"

static unsigned checks=0;
static void one_profile(uint32_t mode,uint32_t checksum,uint64_t n)
{
    elite_authority *a=NULL;elite_object *o=NULL;elite_connection *p=NULL,*c=NULL;
    test_authority(&a);elite_endpoint_definition defs[2];test_definitions(defs,1,1);
    elite_config cfg=test_config(mode,1,1,n);cfg.payload_checksum_mode=checksum;
    OK(elite_create(a,&cfg,defs,&o));OK(elite_object_activate(o));
    elite_grant pg,cg;test_attach_self(o,0,&p,&pg);test_attach_self(o,1,&c,&cg);
    struct elite_immutable_header h,parsed;OK(elite_get_info(p,&h));
    CHECK(h.segment_bytes==(mode==ELITE_SPSC?(n==1024?294912:65536):(n==1024?557056:98304)));
    OK(elite_validate_prefix(&h,512,h.segment_bytes,&parsed));
    CHECK(memcmp(&h,&parsed,sizeof(h))==0);++checks;
    elite_lease wl,rl;elite_write_span ws;elite_read_span rs;
    CHECK(elite_read_borrow(c,&rl,&rs).status==ELITE_NO_DATA_OBSERVED);
    CHECK(elite_write_reserve(c,&wl,&ws).status==ELITE_INVALID_ARGUMENT);
    CHECK(elite_wait_data(c,0).status==ELITE_UNSUPPORTED);
    /* Duplicate attach cannot produce a receipt that resolves the real holder. */
    elite_connection *duplicate=NULL;CHECK(elite_attach(&pg,&duplicate).status==ELITE_BAD_IDENTITY);
    CHECK(duplicate!=NULL);elite_cleanup_receipt dr;OK(elite_detach(&duplicate,&dr));
    CHECK(elite_object_ack_cleanup(o,&dr).status==ELITE_INVALID_ARGUMENT);++checks;
    /* All capacity filled, then drained. Includes repeated physical reuse. */
    for(uint64_t round=0;round<4;++round) {
        for(uint64_t j=0;j<n;++j) {
            OK(elite_write_reserve(p,&wl,&ws));uint64_t id=round*n+j;
            test_payload_write(ws.data,id,0,id,123);
            OK(elite_write_commit(p,&wl,64,7,id));
            CHECK(elite_write_commit(p,&wl,64,7,id).status==ELITE_INVALID_LEASE);
        }
        CHECK(elite_write_reserve(p,&wl,&ws).status==ELITE_NO_CAPACITY_OBSERVED);
        for(uint64_t j=0;j<n;++j) {
            OK(elite_read_borrow(c,&rl,&rs));uint64_t id=round*n+j;
            CHECK(rs.message_id==id && rs.length==64 && rs.message_type==7 && rs.epoch>0);
            CHECK(test_payload_check(rs.data,id,0,id,123));OK(elite_read_release(c,&rl));
            CHECK(elite_read_release(c,&rl).status==ELITE_INVALID_LEASE);
        }
        CHECK(elite_read_borrow(c,&rl,&rs).status==ELITE_NO_DATA_OBSERVED);
    }
    ++checks;
    /* Abort consumes identity; stale native lease rejected before shared reads. */
    OK(elite_write_reserve(p,&wl,&ws));elite_lease stale=wl;
    OK(elite_write_abort(p,&wl));OK(elite_write_reserve(p,&wl,&ws));
    CHECK(memcmp(&stale,&wl,sizeof(wl))!=0);
    CHECK(elite_write_abort(p,&stale).status==ELITE_INVALID_LEASE);
    CHECK(elite_write_commit(p,&wl,65,0,0).status==ELITE_INVALID_ARGUMENT);
    OK(elite_view_retain(p,&wl));OK(elite_view_retain(p,&wl));
    CHECK(elite_write_commit(p,&wl,0,0,0).status==ELITE_BUSY);
    CHECK(elite_detach(&p,&dr).status==ELITE_BUSY);
    OK(elite_view_end(p,&wl));CHECK(elite_write_abort(p,&wl).status==ELITE_BUSY);
    OK(elite_view_end(p,&wl));OK(elite_write_commit(p,&wl,0,0,0));
    OK(elite_read_borrow(c,&rl,&rs));CHECK(rs.length==0 && rs.message_id==0);
    OK(elite_view_retain(c,&rl));CHECK(elite_read_release(c,&rl).status==ELITE_BUSY);
    OK(elite_view_end(c,&rl));OK(elite_read_release(c,&rl));++checks;
    /* Gate retirement rejects new work but an admitted owner can abort safely. */
    OK(elite_write_reserve(p,&wl,&ws));OK(elite_retire(c,0));
    elite_result r=elite_write_commit(p,&wl,0,0,0);CHECK(r.status==ELITE_RETIRED && r.outcome==ELITE_RETAINED);
    OK(elite_write_abort(p,&wl));CHECK(elite_write_reserve(p,&wl,&ws).status==ELITE_RETIRED);
    test_detach(o,&p);test_detach(o,&c);OK(elite_object_destroy(&o));OK(elite_authority_destroy(&a));++checks;
}
static void parser_fixtures(void)
{
    elite_authority *a=NULL;elite_object *o=NULL;test_authority(&a);
    elite_endpoint_definition d[2];test_definitions(d,1,1);elite_config cfg=test_config(ELITE_SPSC,1,1,32);
    OK(elite_create(a,&cfg,d,&o));struct elite_immutable_header h,x,result;OK(elite_object_info(o,&h));
    unsigned rejected=0;
    for(size_t i=0;i<512;++i) {
        x=h;((unsigned char *)&x)[i]^=1;
        CHECK(elite_validate_prefix(&x,512,h.segment_bytes,&result).status!=ELITE_OK);++rejected;
    }
    x=h;x.descriptor_stride=8;x.header_crc32=elite_header_crc32(&x);CHECK(elite_validate_prefix(&x,512,h.segment_bytes,&result).status==ELITE_BAD_LAYOUT);
    x=h;x.capacity=UINT64_MAX;x.header_crc32=elite_header_crc32(&x);CHECK(elite_validate_prefix(&x,512,h.segment_bytes,&result).status==ELITE_BAD_LAYOUT);
    x=h;x.payload_stride=UINT64_MAX;x.header_crc32=elite_header_crc32(&x);CHECK(elite_validate_prefix(&x,512,h.segment_bytes,&result).status==ELITE_BAD_LAYOUT);
    x=h;x.wait_mode=2;x.header_crc32=elite_header_crc32(&x);CHECK(elite_validate_prefix(&x,512,h.segment_bytes,&result).status==ELITE_BAD_LAYOUT);
    CHECK(elite_validate_prefix(&h,511,h.segment_bytes,&result).status==ELITE_INVALID_ARGUMENT);
    printf("parser_single_byte_mutations_rejected=%u\n",rejected);
    OK(elite_object_destroy(&o));OK(elite_authority_destroy(&a));checks+=6;
}
static void quotas(void)
{
    elite_authority *a=NULL;elite_object *o[4]={NULL,NULL,NULL,NULL},*extra=NULL;test_authority(&a);
    elite_endpoint_definition d[2];test_definitions(d,1,1);elite_config cfg=test_config(ELITE_NCQ,1,1,32);
    for(unsigned i=0;i<3;++i) {
        OK(elite_create(a,&cfg,d,&o[i]));OK(elite_object_activate(o[i]));
        if(i<2) OK(elite_object_quarantine(o[i]));
    }
    OK(elite_create(a,&cfg,d,&o[3]));
    CHECK(elite_create(a,&cfg,d,&extra).status==ELITE_BUSY && extra==NULL);
    CHECK(elite_object_quarantine(o[2]).status==ELITE_BUSY);
    CHECK(elite_object_activate(o[3]).status==ELITE_BUSY);
    for(unsigned i=0;i<4;++i) OK(elite_object_destroy(&o[i]));
    OK(elite_authority_destroy(&a));++checks;
}
int main(void)
{
    OK(elite_platform_admit());CHECK(elite_crc32("123456789",9)==UINT32_C(0xcbf43926));
    CHECK(elite_crc64("123456789",9)==UINT64_C(0x6c40df5f0b497347));
    CHECK(elite_crc32(NULL,0)==0 && elite_crc64(NULL,0)==0);++checks;
    for(uint32_t mode=1;mode<=2;++mode) for(uint32_t crc=0;crc<=1;++crc) {
        one_profile(mode,crc,2);one_profile(mode,crc,32);one_profile(mode,crc,1024);
    }
    parser_fixtures();quotas();
    printf("PASS core_groups=%u shared_header=2048 descriptor=128 participant=256\n",checks);return 0;
}
