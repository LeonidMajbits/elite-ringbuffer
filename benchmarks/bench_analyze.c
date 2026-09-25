/* Offline, exact RTT replay. No Python integer array explosion for 100M values.
 * 800MB duration storage at 100M; libc qsort can require another 800MB. */
#include "bench_common.h"
static uint64_t integer(const char *s)
{
    char *end=NULL;errno=0;B_CHECK(*s>='0'&&*s<='9');uint64_t x=strtoull(s,&end,10);B_CHECK(errno==0&&end!=s&&*end=='\0');return x;
}
int main(int argc,char **argv)
{
    if(argc!=7){fprintf(stderr,"usage: %s DURATION_FILE START_FILE_OR_DASH COUNT NUMER DENOM EXPECT_MONOTONIC_STARTS_0_OR_1\n",argv[0]);return 2;}
    uint64_t count=integer(argv[3]),numer=integer(argv[4]),denom=integer(argv[5]),check=integer(argv[6]);
    B_CHECK(count>0&&count<=BENCH_MAX_ITEMS&&numer>0&&numer<=UINT32_MAX&&denom>0&&denom<=UINT32_MAX&&check<=1);
    uint64_t *values=malloc((size_t)count*8);B_CHECK(values!=NULL);FILE *f=fopen(argv[1],"rb");B_CHECK(f!=NULL);B_CHECK(fread(values,8,(size_t)count,f)==count&&fgetc(f)==EOF&&!ferror(f));B_CHECK(fclose(f)==0);
    if(check){FILE *s=fopen(argv[2],"rb");B_CHECK(s!=NULL);uint64_t prior_end=0;
        for(uint64_t i=0;i<count;++i){uint64_t start;B_CHECK(fread(&start,8,1,s)==1);B_CHECK(start>=prior_end&&values[i]<=UINT64_MAX-start);prior_end=start+values[i];}
        B_CHECK(fgetc(s)==EOF&&!ferror(s));B_CHECK(fclose(s)==0);
    }
    struct bench_clock clock={(uint32_t)numer,(uint32_t)denom,0};struct bench_stats stats;bench_statistics(values,count,&stats);bench_stats_json(stdout,&stats,&clock);(void)fputc('\n',stdout);free(values);return 0;
}
