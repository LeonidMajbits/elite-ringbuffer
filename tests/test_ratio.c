#include "bench_ratio.h"
#include <errno.h>
#include <stdlib.h>
#include <inttypes.h>
int main(int argc,char **argv)
{
    if(argc!=5)return 2;
    uint64_t v[4];
    for(int i=0;i<4;++i){char *end;errno=0;v[i]=(uint64_t)strtoumax(argv[i+1],&end,10);if(errno||*end)return 2;}
    if(bench_json_ratio(stdout,v[0],v[1],v[2],v[3])!=0)return 1;
    return fputc('\n',stdout)==EOF?1:0;
}
