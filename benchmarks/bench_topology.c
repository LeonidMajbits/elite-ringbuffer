#include "elite_topology.h"
#include <string.h>
int main(int argc,char **argv)
{
    struct elite_hardware_topology *t=NULL;
    int e=(argc==3&&!strcmp(argv[1],"--fixture"))?elite_topology_fixture(argv[2],&t):
          (argc==1?elite_topology_discover(&t):1);
    if(e){fprintf(stderr,"topology error %d\n",e);return 1;}
    e=elite_topology_json(stdout,t);fputc('\n',stdout);elite_topology_free(t);return e?1:0;
}
