#include "bench_common.h"
#include <stdint.h>
void *elite_matrix_control_create(const char *path);
void *elite_matrix_control_attach(const char *path);
void elite_matrix_control_close(void *p);
uint32_t elite_matrix_control_get(void *p,uint32_t i);
void elite_matrix_control_set(void *p,uint32_t i,uint32_t v);
uint64_t elite_matrix_tick(void);
uint32_t elite_matrix_numer(void);
uint32_t elite_matrix_denom(void);
void elite_matrix_placement(int cpu,int qos,int finish,char *out,size_t cap);
void elite_matrix_reconcile(const elite_grant *g,uint64_t n,const char *dir);
#include <fcntl.h>
#include <sys/mman.h>
void *elite_matrix_control_create(const char *path)
{int fd=open(path,O_CREAT|O_EXCL|O_RDWR,0600);if(fd<0)return NULL;if(ftruncate(fd,BENCH_CTRL_BYTES)<0){close(fd);return NULL;}struct bench_control *p=mmap(NULL,BENCH_CTRL_BYTES,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);close(fd);if(p==MAP_FAILED)return NULL;memset(p,0,BENCH_CTRL_BYTES);for(unsigned i=0;i<BENCH_MAX_WORKERS;++i)atomic_init(&p->cell[i].drain,0);return p;}
void *elite_matrix_control_attach(const char *path){return bench_control_open(path);}
void elite_matrix_control_close(void *p){bench_control_close(p);}
uint32_t elite_matrix_control_get(void *p,uint32_t i){B_CHECK(i<BENCH_MAX_WORKERS);return atomic_load_explicit(&((struct bench_control *)p)->cell[i].drain,memory_order_acquire);}
void elite_matrix_control_set(void *p,uint32_t i,uint32_t v){B_CHECK(i<BENCH_MAX_WORKERS);atomic_store_explicit(&((struct bench_control *)p)->cell[i].drain,v,memory_order_release);}
uint64_t elite_matrix_tick(void){return bench_ordered_tick();}
uint32_t elite_matrix_numer(void){struct bench_clock c;bench_clock_init(&c);return c.numer;}
uint32_t elite_matrix_denom(void){struct bench_clock c;bench_clock_init(&c);return c.denom;}
static struct bench_placement saved;
void elite_matrix_placement(int cpu,int qos,int finish,char *out,size_t cap)
{if(!finish)bench_placement_apply(cpu,0,qos,&saved);else bench_placement_finish(&saved);FILE *f=tmpfile();B_CHECK(f);bench_placement_json(f,&saved);B_CHECK(fflush(f)==0&&fseek(f,0,SEEK_SET)==0);size_t n=fread(out,1,cap-1,f);B_CHECK(fgetc(f)==EOF&&!ferror(f));out[n]='\0';B_CHECK(fclose(f)==0);}
void elite_matrix_reconcile(const elite_grant *g,uint64_t n,const char *dir){bench_reconcile(g,n,dir,"final-reconcile.json");}
