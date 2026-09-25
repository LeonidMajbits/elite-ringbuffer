#define _GNU_SOURCE 1
#include "elite_topology.h"
#include <ctype.h>
#include <errno.h>
#include <dirent.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#else
#include <sched.h>
#endif
#define TEXT_LIMIT 16384u
static void js(FILE *f,const char *s)
{
    fputc('"',f);
    for(const unsigned char *p=(const unsigned char *)s;*p;++p){
        if(*p=='"'||*p=='\\'){fputc('\\',f);fputc(*p,f);}
        else if(*p<32||*p>=127)fprintf(f,"\\u%04x",(unsigned)*p);
        else fputc(*p,f);
    }
    fputc('"',f);
}
static int read_text(const char *root,const char *path,char *out,size_t cap)
{
    char name[4096];int n=snprintf(name,sizeof(name),"%s%s",root,path);
    if(n<0||(size_t)n>=sizeof(name)||cap<2)return EOVERFLOW;
    FILE *f=fopen(name,"r");if(!f){out[0]='\0';return errno;}
    size_t got=fread(out,1,cap-1,f);int e=ferror(f)?EIO:0;
    if(got==cap-1&&fgetc(f)!=EOF)e=EOVERFLOW;
    if(fclose(f)!=0&&e==0)e=errno;
    out[got]='\0';return e;
}
static int u64(const char *text,uint64_t *v,const char **after)
{
    if(!isdigit((unsigned char)*text))return EINVAL;
    errno=0;char *end;unsigned long long x=strtoull(text,&end,10);
    if(errno==ERANGE)return ERANGE;
    *v=(uint64_t)x;*after=end;return 0;
}
int elite_topology_parse_size(const char *text,uint64_t *bytes)
{
    if(!text||!bytes)return EINVAL;
    uint64_t n,m=1;const char *p;int e=u64(text,&n,&p);if(e)return e;
    if(*p=='K'||*p=='k'){m=1024;++p;}else if(*p=='M'||*p=='m'){m=1048576;++p;}
    else if(*p=='G'||*p=='g'){m=1073741824;++p;}
    while(isspace((unsigned char)*p))++p;
    if(*p!='\0')return EINVAL;
    if(n>UINT64_MAX/m)return ERANGE;
    *bytes=n*m;return 0;
}
int elite_topology_parse_cpulist(const char *text,struct elite_topo_set *set)
{
    if(!text||!set)return EINVAL;
    memset(set,0,sizeof(*set));const char *p=text;
    while(isspace((unsigned char)*p))++p;
    if(!*p)return 0;
    for(;;){
        uint64_t a,b;const char *end;int e=u64(p,&a,&end);if(e){set->error=e;break;}
        p=end;b=a;
        if(*p=='-'){e=u64(p+1,&b,&end);if(e){set->error=e;break;}p=end;}
        if(b<a||b>=ELITE_TOPO_MAX_CPUS){set->error=ERANGE;break;}
        for(uint64_t i=a;i<=b;++i){if(set->bits[i]){set->error=EINVAL;break;}set->bits[i]=1;++set->count;}
        if(set->error)break;
        if(*p==','){++p;continue;}
        while(isspace((unsigned char)*p))++p;
        if(*p)set->error=EINVAL;
        break;
    }
    if(set->error){int e=set->error;memset(set,0,sizeof(*set));set->error=e;}
    return set->error;
}
static struct elite_topo_value value_at(const char *root,const char *path)
{
    char text[256];struct elite_topo_value v={0,0,false};
    v.error=read_text(root,path,text,sizeof(text));if(!v.error)v.error=elite_topology_parse_size(text,&v.value);
    v.available=v.error==0;return v;
}
static void set_at(const char *root,const char *path,struct elite_topo_set *s)
{
    char text[TEXT_LIMIT];memset(s,0,sizeof(*s));int e=read_text(root,path,text,sizeof(text));
    if(!e)e=elite_topology_parse_cpulist(text,s);
    s->error=e;
}
static int pathf(char *out,size_t cap,const char *base,unsigned i,const char *suffix)
{int n=snprintf(out,cap,"%s%u/%s",base,i,suffix);return n<0||(size_t)n>=cap?EOVERFLOW:0;}
static bool numbered(const char *name,const char *prefix,unsigned *i)
{
    size_t n=strlen(prefix);if(strncmp(name,prefix,n))return false;
    uint64_t v;const char *end;if(u64(name+n,&v,&end)||*end||v>UINT_MAX)return false;
    *i=(unsigned)v;return true;
}
static int read_ids(const char *root,const char *path,const char *prefix,unsigned *ids,size_t cap,size_t *count)
{
    char full[4096];int n=snprintf(full,sizeof(full),"%s%s",root,path);*count=0;
    if(n<0||(size_t)n>=sizeof(full))return EOVERFLOW;
    DIR *d=opendir(full);if(!d)return errno;
    int e=0;struct dirent *de;
    while((de=readdir(d))!=NULL){unsigned i;if(!numbered(de->d_name,prefix,&i))continue;
        if(*count==cap){e=EOVERFLOW;break;}ids[(*count)++]=i;
    }
    closedir(d);
    for(size_t a=0;a<*count;++a)for(size_t b=a+1;b<*count;++b)if(ids[b]<ids[a]){unsigned tmp=ids[a];ids[a]=ids[b];ids[b]=tmp;}
    return e;
}
static void scalar_json(FILE *f,const struct elite_topo_value *v)
{
    fprintf(f,"{\"value\":");if(v->available)fprintf(f,"%"PRIu64,v->value);else fputs("null",f);
    fprintf(f,",\"status\":\"%s\",\"error\":%d}",v->available?"OBSERVED":"UNAVAILABLE",v->error);
}
static void set_json(FILE *f,const struct elite_topo_set *s)
{
    fprintf(f,"{\"status\":\"%s\",\"error\":%d,\"cpus\":[",s->error?"UNAVAILABLE":"OBSERVED",s->error);
    bool comma=false;for(unsigned i=0;i<ELITE_TOPO_MAX_CPUS;++i)if(s->bits[i]){fprintf(f,"%s%u",comma?",":"",i);comma=true;}
    fputs("]}",f);
}
const char *elite_topology_cluster_name(unsigned c)
{
    if(c==TOPOLOGY_CLUSTER_PERF)return "TOPOLOGY_CLUSTER_PERF";
    if(c==TOPOLOGY_CLUSTER_EFFICIENCY)return "TOPOLOGY_CLUSTER_EFFICIENCY";
    if(c==TOPOLOGY_CLUSTER_MIXED)return "TOPOLOGY_CLUSTER_MIXED";
    return "TOPOLOGY_CLUSTER_UNKNOWN";
}
static bool canonical_path(const char *p)
{
    if(!p||p[0]!='/')return false;
    while(*p){
        while(*p=='/')++p;
        size_t n=strcspn(p,"/");
        if((n==1&&p[0]=='.')||(n==2&&p[0]=='.'&&p[1]=='.'))return false;
        p+=n;
    }
    return true;
}
/* Read only visible ancestors; no claim about hidden host-side constraints. */
static void cgroup_discover(const char *root,struct elite_hardware_topology *t)
{
    char cg[TEXT_LIMIT],mi[TEXT_LIMIT],relative[1024]="",mount[1024]="",mountroot[1024]="";
    if(read_text(root,"/proc/self/cgroup",cg,sizeof(cg))){strcpy(t->cgroup_status,"UNAVAILABLE");return;}
    char *save=NULL;for(char *l=strtok_r(cg,"\n",&save);l;l=strtok_r(NULL,"\n",&save))
        if(!strncmp(l,"0::",3)&&strlen(l+3)<sizeof(relative))strcpy(relative,l+3);
    if(!*relative){strcpy(t->cgroup_status,"V1_OR_UNAVAILABLE_NOT_ASSUMED_UNLIMITED");return;}
    /* mountinfo may exceed the bounded reader: fail closed rather than use a prefix. */
    if(read_text(root,"/proc/self/mountinfo",mi,sizeof(mi))){strcpy(t->cgroup_status,"MOUNTINFO_UNAVAILABLE_OR_TOO_LARGE");return;}
    save=NULL;for(char *l=strtok_r(mi,"\n",&save);l;l=strtok_r(NULL,"\n",&save)){
        if(!strstr(l," - cgroup2 "))continue;
        char mr[1024],mp[1024];if(sscanf(l,"%*u %*u %*s %1023s %1023s",mr,mp)!=2)continue;
        if(strchr(mr,'\\')||strchr(mp,'\\'))continue; /* escaped mounts are unsupported, never guessed */
        size_t n=strlen(mr);
        if(!strcmp(mr,"/")||(!strncmp(relative,mr,n)&&(relative[n]=='/'||relative[n]=='\0'))){strcpy(mount,mp);strcpy(mountroot,mr);break;}
    }
    if(!*mount||!canonical_path(relative)||!canonical_path(mount)||!canonical_path(mountroot)){
        strcpy(t->cgroup_status,"UNRESOLVED_MOUNT");return;
    }
    char current[2048];const char *suffix=!strcmp(mountroot,"/")?relative:relative+strlen(mountroot);
    int n=snprintf(current,sizeof(current),"%s%s",mount,!strcmp(suffix,"/")?"":suffix);
    if(n<0||(size_t)n>=sizeof(current)){strcpy(t->cgroup_status,"PATH_LIMIT");return;}
    t->cgroups=calloc(ELITE_TOPO_MAX_CGROUPS,sizeof(*t->cgroups));if(!t->cgroups){strcpy(t->cgroup_status,"ALLOCATION_FAILED");return;}
    for(;;){
        if(t->cgroup_count==ELITE_TOPO_MAX_CGROUPS){t->partial=true;strcpy(t->cgroup_status,"ANCESTOR_LIMIT");break;}
        struct elite_topo_cgroup *g=&t->cgroups[t->cgroup_count++];
        if(strlen(current)>=sizeof(g->path)){t->partial=true;strcpy(t->cgroup_status,"PATH_LIMIT");break;}
        strcpy(g->path,current);char path[4096],txt[256];
        snprintf(path,sizeof(path),"%s/cpu.max",current);int e=read_text(root,path,txt,sizeof(txt));
        g->quota_us.error=e;g->period_us.error=e;
        if(!e){char first[128],second[128],extra;uint64_t q=0,p=0;
            if(sscanf(txt,"%127s %127s %c",first,second,&extra)!=2||elite_topology_parse_size(second,&p)||p==0)e=EINVAL;
            else if(!strcmp(first,"max")){g->quota_unlimited=true;g->period_us=(struct elite_topo_value){p,0,true};}
            else if(elite_topology_parse_size(first,&q)||q==0)e=EINVAL;
            else{g->quota_us=(struct elite_topo_value){q,0,true};g->period_us=(struct elite_topo_value){p,0,true};}
            if(e){g->quota_us.error=e;g->period_us.error=e;}
        }
        snprintf(path,sizeof(path),"%s/memory.max",current);e=read_text(root,path,txt,sizeof(txt));
        if(!e&&!strncmp(txt,"max",3)&&(txt[3]=='\0'||isspace((unsigned char)txt[3])))g->memory_unlimited=true;
        else g->memory_max=value_at(root,path);
        snprintf(path,sizeof(path),"%s/memory.current",current);g->memory_current=value_at(root,path);
        snprintf(path,sizeof(path),"%s/cpuset.cpus.effective",current);set_at(root,path,&g->cpus_effective);
        snprintf(path,sizeof(path),"%s/cpuset.mems.effective",current);set_at(root,path,&g->mems_effective);
        if(!strcmp(current,mount)){strcpy(t->cgroup_status,"V2_VISIBLE_ANCESTORS_ONLY");break;}
        char *slash=strrchr(current,'/');if(!slash||strlen(current)<=strlen(mount)){strcpy(t->cgroup_status,"UNRESOLVED_ANCESTOR");break;}*slash='\0';
    }
}
static void linux_discover(const char *root,bool fixture,struct elite_hardware_topology *t)
{
    strcpy(t->platform,"Linux");char txt[TEXT_LIMIT],path[1024];
    set_at(root,"/sys/devices/system/cpu/online",&t->online_cpus);
    if(fixture){set_at(root,"/proc/self/allowed_cpus",&t->allowed_cpus);}
#ifndef __APPLE__
    else{cpu_set_t mask;CPU_ZERO(&mask);if(sched_getaffinity(0,sizeof(mask),&mask)<0)t->allowed_cpus.error=errno;
        else for(unsigned i=0;i<ELITE_TOPO_MAX_CPUS&&i<CPU_SETSIZE;++i)if(CPU_ISSET(i,&mask)){t->allowed_cpus.bits[i]=1;++t->allowed_cpus.count;}}
#endif
    if(t->online_cpus.error||t->allowed_cpus.error)t->partial=true;
    if(!read_text(root,"/proc/cpuinfo",txt,sizeof(txt))){
        t->hypervisor_flag=strstr(txt," hypervisor ")!=NULL;
        char *p=strstr(txt,"model name");if(!p)p=strstr(txt,"Hardware");if(p&&(p=strchr(p,':'))!=NULL){++p;while(*p==' '||*p=='\t')++p;size_t n=strcspn(p,"\n");if(n>=sizeof(t->model))n=sizeof(t->model)-1;memcpy(t->model,p,n);t->model[n]='\0';}
    }else ++t->observation_errors;
    if(!read_text(root,"/proc/meminfo",txt,sizeof(txt))){char *p=strstr(txt,"MemTotal:");if(p){uint64_t kb;char unit[8];if(sscanf(p,"MemTotal: %"SCNu64" %7s",&kb,unit)==2&&!strcmp(unit,"kB")&&kb<=UINT64_MAX/1024)t->physical_memory_bytes=(struct elite_topo_value){kb*1024,0,true};}}
    t->logical_cpus=(struct elite_topo_value){t->online_cpus.count,t->online_cpus.error,t->online_cpus.error==0};
    t->cpus=calloc(t->allowed_cpus.count? t->allowed_cpus.count:1,sizeof(*t->cpus));
    if(!t->cpus){t->partial=true;return;}
    for(unsigned id=0;id<ELITE_TOPO_MAX_CPUS;++id)if(t->allowed_cpus.bits[id]){
        struct elite_topo_cpu *c=&t->cpus[t->cpu_count++];c->cpu_id=id;c->numa_node=-1;
        pathf(path,sizeof(path),"/sys/devices/system/cpu/cpu",id,"topology/physical_package_id");c->package_id=value_at(root,path);
        pathf(path,sizeof(path),"/sys/devices/system/cpu/cpu",id,"topology/die_id");c->die_id=value_at(root,path);
        pathf(path,sizeof(path),"/sys/devices/system/cpu/cpu",id,"topology/core_id");c->core_id=value_at(root,path);
        pathf(path,sizeof(path),"/sys/devices/system/cpu/cpu",id,"topology/cluster_id");c->cluster_id=value_at(root,path);
        pathf(path,sizeof(path),"/sys/devices/system/cpu/cpu",id,"topology/thread_siblings_list");set_at(root,path,&c->smt_siblings);
        pathf(path,sizeof(path),"/sys/devices/system/cpu/cpu",id,"cache");unsigned ids[ELITE_TOPO_MAX_CACHES];size_t count;
        int e=read_ids(root,path,"index",ids,ELITE_TOPO_MAX_CACHES,&count);if(e){++t->observation_errors;t->partial=true;}
        for(size_t i=0;i<count;++i){struct elite_topo_cache *x=&c->caches[c->cache_count++];x->index=ids[i];
            char p[1024];snprintf(p,sizeof(p),"/sys/devices/system/cpu/cpu%u/cache/index%u/level",id,x->index);x->level=value_at(root,p);
            snprintf(p,sizeof(p),"/sys/devices/system/cpu/cpu%u/cache/index%u/size",id,x->index);x->bytes=value_at(root,p);
            snprintf(p,sizeof(p),"/sys/devices/system/cpu/cpu%u/cache/index%u/coherency_line_size",id,x->index);x->line_bytes=value_at(root,p);
            snprintf(p,sizeof(p),"/sys/devices/system/cpu/cpu%u/cache/index%u/id",id,x->index);x->id=value_at(root,p);
            snprintf(p,sizeof(p),"/sys/devices/system/cpu/cpu%u/cache/index%u/shared_cpu_list",id,x->index);set_at(root,p,&x->shared_cpus);
            snprintf(p,sizeof(p),"/sys/devices/system/cpu/cpu%u/cache/index%u/type",id,x->index);if(!read_text(root,p,x->type,sizeof(x->type)))x->type[strcspn(x->type,"\r\n")]='\0';
            if(!x->level.available||!x->bytes.available||x->shared_cpus.error)t->partial=true;
        }
    }
    t->nodes=calloc(ELITE_TOPO_MAX_NODES,sizeof(*t->nodes));
    if(t->nodes){unsigned ids[ELITE_TOPO_MAX_NODES];size_t count;int e=read_ids(root,"/sys/devices/system/node","node",ids,ELITE_TOPO_MAX_NODES,&count);if(e){t->partial=true;++t->observation_errors;}
        for(size_t i=0;i<count;++i){struct elite_topo_node *n=&t->nodes[t->node_count++];n->node_id=ids[i];pathf(path,sizeof(path),"/sys/devices/system/node/node",n->node_id,"cpulist");set_at(root,path,&n->cpus);
            if(n->cpus.error){t->partial=true;continue;}
            for(size_t j=0;j<t->cpu_count;++j)if(n->cpus.bits[t->cpus[j].cpu_id]){if(t->cpus[j].numa_node!=-1){t->partial=true;t->cpus[j].numa_node=-2;}else if(n->node_id<=INT_MAX)t->cpus[j].numa_node=(int)n->node_id;}}
    }else t->partial=true;
    bool all=t->cpu_count>0;uint64_t physical=0;
    for(size_t i=0;i<t->cpu_count;++i){struct elite_topo_cpu *c=&t->cpus[i];if(!c->core_id.available||!c->package_id.available||!c->die_id.available){all=false;break;}bool previous=false;
        for(size_t j=0;j<i;++j)if(t->cpus[j].core_id.value==c->core_id.value&&t->cpus[j].package_id.value==c->package_id.value&&t->cpus[j].die_id.value==c->die_id.value)previous=true;
        if(!previous)++physical;
    }
    t->physical_cpus=(struct elite_topo_value){physical,all?0:ENODATA,all};
    cgroup_discover(root,t);
    char full[4096];snprintf(full,sizeof(full),"%s/.dockerenv",root);t->container_marker=access(full,F_OK)==0;
    snprintf(full,sizeof(full),"%s/run/.containerenv",root);t->container_marker=t->container_marker||access(full,F_OK)==0;
    if(t->container_marker||t->hypervisor_flag)strcpy(t->environment,"TOPOLOGY_VIRTUALIZED_CONTAINER");
    else if(t->cgroup_count>0||t->allowed_cpus.count<t->online_cpus.count)strcpy(t->environment,"RESOURCE_SCOPED_PHYSICAL_IDENTITY_UNVERIFIED");
    else strcpy(t->environment,"PHYSICAL_OR_VIRTUAL_UNVERIFIED");
}
#ifdef __APPLE__
static struct elite_topo_value sysvalue(const char *name)
{
    struct elite_topo_value v={0,0,false};uint64_t data=0;size_t n=sizeof(data);
    if(sysctlbyname(name,&data,&n,NULL,0)<0){v.error=errno;return v;}
    if(n!=4&&n!=8){v.error=EINVAL;return v;}v.value=n==4?(uint64_t)(uint32_t)data:data;v.available=true;return v;
}
static void darwin_discover(struct elite_hardware_topology *t)
{
    strcpy(t->platform,"Darwin");strcpy(t->environment,"PHYSICAL_OR_VIRTUAL_UNVERIFIED");strcpy(t->cgroup_status,"NOT_APPLICABLE");
    size_t len=sizeof(t->model);if(sysctlbyname("hw.model",t->model,&len,NULL,0)<0)strcpy(t->model,"UNKNOWN");
    t->physical_cpus=sysvalue("hw.physicalcpu");t->logical_cpus=sysvalue("hw.logicalcpu");
    t->physical_memory_bytes=sysvalue("hw.memsize");t->cache_line_bytes=sysvalue("hw.cachelinesize");t->system_l3_bytes=sysvalue("hw.l3cachesize");
    t->memory_frequency_hz=sysvalue("hw.memfrequency");t->memory_channels=sysvalue("hw.memchannels");
    struct elite_topo_value trans=sysvalue("sysctl.proc_translated");t->translation_error=trans.error;t->translated=trans.available&&trans.value!=0;
    struct elite_topo_value levels=sysvalue("hw.nperflevels");
    if(!levels.available){t->partial=true;return;}
    uint64_t count=levels.value;if(count>ELITE_TOPO_MAX_LEVELS){count=ELITE_TOPO_MAX_LEVELS;t->partial=true;}
    for(unsigned i=0;i<count;++i){struct elite_topo_perflevel *p=&t->perflevels[t->perflevel_count++];p->index=i;char key[96];
        snprintf(key,sizeof(key),"hw.perflevel%u.name",i);len=sizeof(p->name);if(sysctlbyname(key,p->name,&len,NULL,0)<0){p->name_error=errno;strcpy(p->name,"UNKNOWN");}p->name[sizeof(p->name)-1]='\0';
        if(!strcmp(p->name,"Performance"))p->cluster_class=TOPOLOGY_CLUSTER_PERF;
        else if(!strcmp(p->name,"Efficiency"))p->cluster_class=TOPOLOGY_CLUSTER_EFFICIENCY;
        t->host_cluster_class|=p->cluster_class;
#define SYSFIELD(field,suffix) snprintf(key,sizeof(key),"hw.perflevel%u." suffix,i);p->field=sysvalue(key)
        SYSFIELD(physical,"physicalcpu");SYSFIELD(logical,"logicalcpu");SYSFIELD(l1d_bytes,"l1dcachesize");SYSFIELD(l2_bytes,"l2cachesize");SYSFIELD(l3_bytes,"l3cachesize");SYSFIELD(cpus_per_l2,"cpusperl2");
#undef SYSFIELD
    }
    /* The aggregate perflevel API supplies no CPU-ID-to-class map or binding. */
    t->allowed_cpus.error=ENOTSUP;t->online_cpus.error=ENOTSUP;
}
#endif
static int discover(const char *root,bool fixture,struct elite_hardware_topology **out)
{
    if(!out||!root)return EINVAL;
    *out=NULL;struct elite_hardware_topology *t=calloc(1,sizeof(*t));if(!t)return ENOMEM;
    strcpy(t->source_kind,fixture?"SYNTHETIC_FIXTURE":"LIVE_OS_VISIBLE");strcpy(t->model,"UNKNOWN");
    t->memory_frequency_hz.error=ENOTSUP;t->memory_channels.error=ENOTSUP;t->system_l3_bytes.error=ENOTSUP;t->cache_line_bytes.error=ENOTSUP;t->physical_memory_bytes.error=ENODATA;
    struct utsname u;if(!fixture&&uname(&u)==0){snprintf(t->architecture,sizeof(t->architecture),"%.*s",63,u.machine);snprintf(t->kernel,sizeof(t->kernel),"%.*s",255,u.release);}else{strcpy(t->architecture,"FIXTURE");strcpy(t->kernel,"FIXTURE");}
    long p=fixture?-1:sysconf(_SC_PAGESIZE);t->page_bytes=(struct elite_topo_value){p>0?(uint64_t)p:0,p>0?0:ENODATA,p>0};
#ifdef __APPLE__
    if(!fixture)darwin_discover(t);else linux_discover(root,true,t);
#else
    linux_discover(root,fixture,t);
#endif
    *out=t;return 0;
}
int elite_topology_discover(struct elite_hardware_topology **out){return discover("",false,out);}
int elite_topology_fixture(const char *root,struct elite_hardware_topology **out){if(!root||root[0]!='/')return EINVAL;return discover(root,true,out);}
void elite_topology_free(struct elite_hardware_topology *t){if(t){free(t->cpus);free(t->nodes);free(t->cgroups);free(t);}}
int elite_topology_json(FILE *f,const struct elite_hardware_topology *t)
{
    if(!f||!t)return EINVAL;
    fputs("{\"schema\":\"elite-hardware-topology-v1\",\"source_kind\":",f);js(f,t->source_kind);
#define STR(key,member) fputs(",\"" key "\":",f);js(f,t->member)
#define VAL(key,member) fputs(",\"" key "\":",f);scalar_json(f,&t->member)
    STR("platform",platform);STR("architecture",architecture);STR("kernel",kernel);STR("model",model);STR("environment",environment);STR("cgroup_status",cgroup_status);
    fprintf(f,",\"partial\":%s,\"container_marker\":%s,\"hypervisor_flag\":%s,\"translated\":%s,\"translation_error\":%d,\"observation_errors\":%u,\"max_cpu_id_exclusive\":%u",t->partial?"true":"false",t->container_marker?"true":"false",t->hypervisor_flag?"true":"false",t->translated?"true":"false",t->translation_error,t->observation_errors,ELITE_TOPO_MAX_CPUS);
    fputs(",\"scope\":\"cache/core records for affinity-eligible CPUs; node maps and online CPUs are OS-visible, not physical attestation\",\"worker_cluster_mapping\":\"NOT_ESTABLISHED\",\"host_cluster_class\":",f);js(f,elite_topology_cluster_name(t->host_cluster_class));
    VAL("page_bytes",page_bytes);VAL("physical_memory_bytes",physical_memory_bytes);VAL("physical_cpus",physical_cpus);VAL("logical_cpus",logical_cpus);VAL("memory_frequency_hz",memory_frequency_hz);VAL("memory_channels",memory_channels);VAL("system_l3_bytes",system_l3_bytes);VAL("cache_line_bytes",cache_line_bytes);
    fputs(",\"online_cpus\":",f);set_json(f,&t->online_cpus);fputs(",\"allowed_cpus\":",f);set_json(f,&t->allowed_cpus);
    fputs(",\"cpus\":[",f);for(size_t i=0;i<t->cpu_count;++i){const struct elite_topo_cpu *c=&t->cpus[i];fprintf(f,"%s{\"cpu_id\":%u,\"numa_node\":%d,\"package_id\":",i?",":"",c->cpu_id,c->numa_node);scalar_json(f,&c->package_id);
        fputs(",\"die_id\":",f);scalar_json(f,&c->die_id);fputs(",\"core_id\":",f);scalar_json(f,&c->core_id);fputs(",\"cluster_id\":",f);scalar_json(f,&c->cluster_id);fputs(",\"smt_siblings\":",f);set_json(f,&c->smt_siblings);fputs(",\"caches\":[",f);
        for(size_t j=0;j<c->cache_count;++j){const struct elite_topo_cache *x=&c->caches[j];fprintf(f,"%s{\"index\":%u,\"type\":",j?",":"",x->index);js(f,x->type);
            fputs(",\"level\":",f);scalar_json(f,&x->level);fputs(",\"bytes\":",f);scalar_json(f,&x->bytes);fputs(",\"line_bytes\":",f);scalar_json(f,&x->line_bytes);fputs(",\"id\":",f);scalar_json(f,&x->id);fputs(",\"shared_cpus\":",f);set_json(f,&x->shared_cpus);fputc('}',f);}
        fputs("]}",f);}
    fputs("],\"nodes\":[",f);for(size_t i=0;i<t->node_count;++i){fprintf(f,"%s{\"node_id\":%u,\"cpus\":",i?",":"",t->nodes[i].node_id);set_json(f,&t->nodes[i].cpus);fputc('}',f);}
    fputs("],\"perflevels\":[",f);for(size_t i=0;i<t->perflevel_count;++i){const struct elite_topo_perflevel *p=&t->perflevels[i];fprintf(f,"%s{\"index\":%u,\"name\":",i?",":"",p->index);js(f,p->name);fputs(",\"cluster_class\":",f);js(f,elite_topology_cluster_name(p->cluster_class));fprintf(f,",\"name_error\":%d,\"physical\":",p->name_error);scalar_json(f,&p->physical);fputs(",\"logical\":",f);scalar_json(f,&p->logical);
#define PV(key,field) fputs(",\"" key "\":",f);scalar_json(f,&p->field)
        PV("l1d_bytes",l1d_bytes);PV("l2_bytes",l2_bytes);PV("l3_bytes",l3_bytes);PV("cpus_per_l2",cpus_per_l2);
#undef PV
        fputc('}',f);}
    fputs("],\"cgroups\":[",f);for(size_t i=0;i<t->cgroup_count;++i){const struct elite_topo_cgroup *g=&t->cgroups[i];if(i)fputc(',',f);fputs("{\"path\":",f);js(f,g->path);fprintf(f,",\"quota_unlimited\":%s,\"memory_unlimited\":%s,\"quota_us\":",g->quota_unlimited?"true":"false",g->memory_unlimited?"true":"false");scalar_json(f,&g->quota_us);fputs(",\"period_us\":",f);scalar_json(f,&g->period_us);fputs(",\"memory_max\":",f);scalar_json(f,&g->memory_max);fputs(",\"memory_current\":",f);scalar_json(f,&g->memory_current);fputs(",\"cpus_effective\":",f);set_json(f,&g->cpus_effective);fputs(",\"mems_effective\":",f);set_json(f,&g->mems_effective);fputc('}',f);}
    fputs("]}",f);
#undef STR
#undef VAL
    return ferror(f)?EIO:0;
}
