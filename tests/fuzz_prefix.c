/* Bounded deterministic stable-byte parser corpus, not a random live-atomic
 * exerciser and not a replacement for a memory-model checker. */
#include "test_support.h"
int main(int argc,char **argv)
{
    uint64_t count=argc>1?strtoull(argv[1],NULL,10):UINT64_C(1000000);
    CHECK(count>0&&count<=UINT64_C(10000000));
    elite_authority *a=NULL;elite_object *o=NULL;test_authority(&a);
    elite_endpoint_definition d[2];test_definitions(d,1,1);elite_config cfg=test_config(ELITE_NCQ,1,1,32);
    OK(elite_create(a,&cfg,d,&o));struct elite_immutable_header base,x,out;OK(elite_object_info(o,&base));
    uint64_t state=UINT64_C(0x744125b77cc09301),accepted=0,rejected=0;
    for(uint64_t i=0;i<count;++i){
        state=state*UINT64_C(6364136223846793005)+UINT64_C(1442695040888963407);
        x=base;size_t offset=(size_t)((state>>32)&511);unsigned bit=(unsigned)(state&7);
        ((unsigned char *)&x)[offset]^=(unsigned char)(1u<<bit);
        if((i&1)==0)x.header_crc32=elite_header_crc32(&x);
        elite_result r=elite_validate_prefix(&x,512,base.segment_bytes,&out);
        if(r.status==ELITE_OK){
            ++accepted;CHECK(out.segment_bytes==base.segment_bytes&&out.mapping_quantum==16384&&out.descriptor_stride==128);
            CHECK(out.header_crc32==elite_header_crc32(&out)&&out.capacity>=2&&(out.capacity&(out.capacity-1))==0);
            CHECK(out.endpoint_count==out.producer_endpoints+out.consumer_endpoints&&out.endpoint_count<=out.capacity);
        }else ++rejected;
    }
    OK(elite_object_destroy(&o));OK(elite_authority_destroy(&a));
    printf("PASS stable_prefix_corpus inputs=%llu accepted_valid_mutations=%llu rejected=%llu seed=744125b77cc09301\n",(unsigned long long)count,(unsigned long long)accepted,(unsigned long long)rejected);return 0;
}
