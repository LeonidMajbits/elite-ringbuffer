#include "bench_common.h"
int main(void)
{
    uint64_t *values=malloc(100000*sizeof(uint64_t));B_CHECK(values!=NULL);
    for(uint64_t i=0;i<100000;++i)values[i]=100000-i;
    struct bench_stats s;bench_statistics(values,100000,&s);
    B_CHECK(s.q[0]==50000&&s.q[1]==90000&&s.q[2]==99000&&s.q[3]==99900&&s.q[4]==99990&&s.q[5]==100000);
    B_CHECK(s.minimum_nonzero==1&&s.zeros==0);
    uint64_t single[1]={0};bench_statistics(single,1,&s);B_CHECK(s.q[0]==0&&s.q[5]==0&&s.zeros==1);
    uint64_t sparse[5]={9,0,2,0,7};bench_statistics(sparse,5,&s);B_CHECK(s.q[0]==2&&s.q[1]==9&&s.zeros==2&&s.minimum_nonzero==2);
    struct bench_clock clock={125,3,0};B_CHECK(bench_ns_ceil(&clock,1)==42&&bench_ns_ceil(&clock,3)==125&&bench_ns_ceil(&clock,0)==0);
    clock.numer=1;clock.denom=1;B_CHECK(bench_ns_ceil(&clock,UINT64_MAX)==UINT64_MAX);
    clock.numer=UINT32_MAX;clock.denom=UINT32_MAX;B_CHECK(bench_ns_ceil(&clock,UINT64_MAX)==UINT64_MAX);
    _Alignas(128) uint64_t payload[8];bench_payload_write(payload,37,2,7,BENCH_SEED);B_CHECK(bench_payload_check(payload,37,2,7,BENCH_SEED));payload[6]^=1;B_CHECK(!bench_payload_check(payload,37,2,7,BENCH_SEED));
    free(values);puts("PASS benchmark tool groups=8 exact-ranks p99.99 rounding zeros payload negative-control");return 0;
}
