#include "elite_topology.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x);exit(1);}}while(0)
int main(void)
{
    struct elite_topo_set s;uint64_t v;
    CHECK(elite_topology_parse_cpulist("2,4,8-10\n",&s)==0&&s.count==5&&s.bits[2]&&s.bits[10]&&!s.bits[3]);
    const char *bad[]={"1,1","1-3,2","4-2","1024","-1","1,","1 2","1--2","18446744073709551616","1;x"};
    for(size_t i=0;i<sizeof(bad)/sizeof(bad[0]);++i)CHECK(elite_topology_parse_cpulist(bad[i],&s)!=0&&s.count==0);
    CHECK(elite_topology_parse_cpulist("",&s)==0&&s.count==0);
    CHECK(elite_topology_parse_cpulist("0-1023",&s)==0&&s.count==1024);
    CHECK(elite_topology_parse_cpulist(NULL,&s)==EINVAL&&elite_topology_parse_cpulist("0",NULL)==EINVAL);
    CHECK(elite_topology_parse_size("32K\n",&v)==0&&v==32768);
    CHECK(elite_topology_parse_size("1048576",&v)==0&&v==1048576);
    CHECK(elite_topology_parse_size("16M",&v)==0&&v==16777216);
    CHECK(elite_topology_parse_size("2G",&v)==0&&v==2147483648ULL);
    CHECK(elite_topology_parse_size("18446744073709551615",&v)==0&&v==UINT64_MAX);
    CHECK(elite_topology_parse_size("18446744073709551615K",&v)==ERANGE);
    CHECK(elite_topology_parse_size("-1",&v)!=0&&elite_topology_parse_size("32KB",&v)!=0);
    CHECK(elite_topology_parse_size(NULL,&v)==EINVAL&&elite_topology_parse_size("1",NULL)==EINVAL);
    CHECK(elite_topology_cluster_name(TOPOLOGY_CLUSTER_MIXED)!=NULL);
    puts("PASS topology parser: sparse IDs, bounds, duplicate rejection, units, overflow, nulls");return 0;
}
