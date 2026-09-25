#include "elite_internal.h"
#include <unistd.h>

uint32_t elite_crc32(const void *bytes,size_t n)
{
    const unsigned char *p=bytes; uint32_t crc=UINT32_MAX;
    for(size_t i=0;i<n;++i) {
        crc^=p[i];
        for(unsigned b=0;b<8;++b) crc=(crc>>1)^((UINT32_C(0)-(crc&1))&UINT32_C(0xedb88320));
    }
    return crc^UINT32_MAX;
}
uint64_t elite_crc64(const void *bytes,size_t n)
{
    const unsigned char *p=bytes; uint64_t crc=0;
    for(size_t i=0;i<n;++i) {
        crc^=(uint64_t)p[i]<<56;
        for(unsigned b=0;b<8;++b) crc=(crc<<1)^((UINT64_C(0)-(crc>>63))&UINT64_C(0x42f0e1eba9ea3693));
    }
    return crc;
}
uint32_t elite_header_crc32(const struct elite_immutable_header *h)
{
    const unsigned char *p=(const unsigned char *)h; uint32_t crc=UINT32_MAX;
    for(size_t i=0;i<512;++i) {
        crc^=(i>=52 && i<56)?0:p[i];
        for(unsigned b=0;b<8;++b) crc=(crc>>1)^((UINT32_C(0)-(crc&1))&UINT32_C(0xedb88320));
    }
    return crc^UINT32_MAX;
}
static bool add(uint64_t a,uint64_t b,uint64_t *r)
{ if(a>UINT64_MAX-b) return false; *r=a+b; return true; }
static bool mul(uint64_t a,uint64_t b,uint64_t *r)
{ if(b!=0 && a>UINT64_MAX/b) return false; *r=a*b; return true; }
static bool round_up(uint64_t a,uint64_t q,uint64_t *r)
{ if(!add(a,q-1,r)) return false; *r&=~(q-1); return true; }

elite_result el_geometry(struct elite_immutable_header *h)
{
#if defined(__FreeBSD__)
    /* New opt-in platform: do not accept an unimplemented sleeping adapter. */
    if(h->wait_mode!=ELITE_POLL_ONLY)
        return el_result(ELITE_UNSUPPORTED,ELITE_NONE,0);
#endif
    uint64_t n=h->capacity,k=(uint64_t)h->producer_endpoints+h->consumer_endpoints;
    uint64_t q=ELITE_QUANTUM, o=0,x=0;
    if(n<2 || n>(UINT64_C(1)<<31) || (n&(n-1))!=0 || k<2 || k>n ||
        h->producer_endpoints==0 || h->consumer_endpoints==0 || h->max_payload_bytes==0 ||
        h->layout_profile<ELITE_SPSC || h->layout_profile>ELITE_NCQ ||
        h->wait_mode>ELITE_PARKABLE_SPSC || h->payload_checksum_mode>ELITE_CHECKSUM_CRC64 ||
        (h->layout_profile==ELITE_SPSC && (h->producer_endpoints!=1 || h->consumer_endpoints!=1)) ||
        (h->layout_profile==ELITE_NCQ && h->wait_mode!=ELITE_POLL_ONLY))
        return el_result(ELITE_BAD_LAYOUT,ELITE_NONE,0);
    h->endpoint_count=(uint32_t)k;
    h->participants_offset=q; h->participant_stride=256;
    h->descriptor_stride=128; h->queue_entry_stride=h->layout_profile==ELITE_NCQ?128:0;
    if(!round_up(h->max_payload_bytes,128,&h->payload_stride) || !mul(k,256,&x) ||
        !add(q,x,&o) || !round_up(o,q,&o)) return el_result(ELITE_BAD_LAYOUT,ELITE_NONE,0);
    h->qf_entries_offset=0; h->qr_entries_offset=0;
    if(!mul(n,128,&x)) return el_result(ELITE_BAD_LAYOUT,ELITE_NONE,0);
    if(h->layout_profile==ELITE_NCQ) {
        h->qf_entries_offset=o;
        if(!add(o,x,&o)||!round_up(o,q,&o)) return el_result(ELITE_BAD_LAYOUT,ELITE_NONE,0);
        h->qr_entries_offset=o;
        if(!add(o,x,&o)||!round_up(o,q,&o)) return el_result(ELITE_BAD_LAYOUT,ELITE_NONE,0);
    }
    h->descriptors_offset=o;
    if(!add(o,x,&o)||!round_up(o,q,&o)) return el_result(ELITE_BAD_LAYOUT,ELITE_NONE,0);
    h->payloads_offset=o;
    if(!mul(n,h->payload_stride,&x)||!add(o,x,&o)||!round_up(o,q,&o) ||
        o>h->max_backing_bytes || o>(uint64_t)SIZE_MAX || o>(uint64_t)PTRDIFF_MAX || o>INT64_MAX)
        return el_result(ELITE_BAD_LAYOUT,ELITE_NONE,0);
    h->segment_bytes=o; h->ticket_ceiling=UINT64_MAX-n;
    h->epoch_ceiling=ELITE_EPOCH_CEILING;
    h->max_wait_slice_ns=h->wait_mode==ELITE_PARKABLE_SPSC?ELITE_MAX_WAIT_SLICE_NS:0;
    return el_result(ELITE_OK,ELITE_NONE,0);
}

elite_result elite_platform_admit(void)
{
#if (!defined(__linux__) && !defined(__APPLE__) && !(defined(__FreeBSD__) && defined(ELITE_EXPERIMENTAL_FREEBSD))) || (!defined(__x86_64__) && !defined(__aarch64__))
    return el_result(ELITE_UNSUPPORTED,ELITE_NONE,0);
#else
    const uint32_t value=UINT32_C(0x01020304);
    if(*(const unsigned char *)&value!=4 || sizeof(size_t)!=8 || sizeof(off_t)!=8)
        return el_result(ELITE_UNSUPPORTED,ELITE_NONE,0);
    _Atomic uint32_t a; _Atomic uint64_t b;
    atomic_init(&a,0); atomic_init(&b,0);
    if(!atomic_is_lock_free(&a)||!atomic_is_lock_free(&b)) return el_result(ELITE_UNSUPPORTED,ELITE_NONE,0);
    long v=sysconf(_SC_PAGESIZE);
    if(v<128 || v>16384 || ((uint64_t)v&((uint64_t)v-1))!=0 || ELITE_QUANTUM%(uint64_t)v!=0)
        return el_result(ELITE_UNSUPPORTED,ELITE_NONE,0);
    return el_result(ELITE_OK,ELITE_NONE,0);
#endif
}

elite_result elite_validate_prefix(const void *bytes,size_t available,uint64_t backing,
    struct elite_immutable_header *out)
{
    if(bytes==NULL || out==NULL || available<512) return el_result(ELITE_INVALID_ARGUMENT,ELITE_NONE,0);
    const uint32_t endian=1;
    if(*(const unsigned char *)&endian!=1) return el_result(ELITE_UNSUPPORTED,ELITE_NONE,0);
    struct elite_immutable_header h;
    memcpy(&h,bytes,sizeof(h)); /* ordinary, stable, immutable bytes only */
    static const unsigned char magic[8]={'E','L','I','T','E','I','P','C'};
    if(memcmp(h.magic,magic,8)!=0 || h.abi_version!=ELITE_ABI_VERSION ||
        h.header_bytes!=2048 || h.immutable_bytes!=512 || h.format_flags!=0 ||
        h.endian_tag!=UINT32_C(0x01020304) || h.isolation_bytes!=128 ||
        h.mapping_quantum!=16384 || h.atomic_abi_id!=1 || h.lifecycle_profile!=1 ||
        h.max_backing_objects!=4 || h.max_quarantined_objects!=2)
        return el_result(ELITE_BAD_ABI,ELITE_NONE,0);
    if(!el_zero(h.reserved_110,sizeof(h.reserved_110))) return el_result(ELITE_BAD_LAYOUT,ELITE_NONE,0);
    if(h.header_crc32!=elite_header_crc32(&h)) return el_result(ELITE_INTEGRITY,ELITE_NONE,0);
    if(el_zero(h.session_id,16)||el_zero(h.host_instance_id,16)||el_zero(h.authority_instance_id,16)||h.authority_epoch==0)
        return el_result(ELITE_BAD_IDENTITY,ELITE_NONE,0);
    struct elite_immutable_header expected=h;
    elite_result r=el_geometry(&expected);
    if(r.status!=ELITE_OK || h.segment_bytes!=backing || memcmp(&h,&expected,sizeof(h))!=0)
        return el_result(ELITE_BAD_LAYOUT,ELITE_NONE,0);
    *out=h; return el_result(ELITE_OK,ELITE_NONE,0);
}
