#define _GNU_SOURCE 1
#include "bench_identity.h"
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#include "cache_build_info.h"
struct sha { uint32_t h[8]; uint64_t bytes; unsigned used; unsigned char block[64]; };
static uint32_t rotate(uint32_t x,unsigned b){return (x>>b)|(x<<(32u-b));}
static void transform(struct sha *s)
{
    static const uint32_t k[64]={
        0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
        0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
        0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
        0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
        0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
        0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
        0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
        0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u};
    uint32_t w[64];
    for(unsigned i=0;i<16;++i)w[i]=(uint32_t)s->block[4*i]<<24|(uint32_t)s->block[4*i+1]<<16|(uint32_t)s->block[4*i+2]<<8|s->block[4*i+3];
    for(unsigned i=16;i<64;++i){uint32_t x=w[i-15],y=w[i-2];w[i]=w[i-16]+(rotate(x,7)^rotate(x,18)^(x>>3))+w[i-7]+(rotate(y,17)^rotate(y,19)^(y>>10));}
    uint32_t a=s->h[0],b=s->h[1],c=s->h[2],d=s->h[3],e=s->h[4],f=s->h[5],g=s->h[6],h=s->h[7];
    for(unsigned i=0;i<64;++i){uint32_t t=h+(rotate(e,6)^rotate(e,11)^rotate(e,25))+((e&f)^(~e&g))+k[i]+w[i];uint32_t u=(rotate(a,2)^rotate(a,13)^rotate(a,22))+((a&b)^(a&c)^(b&c));h=g;g=f;f=e;e=d+t;d=c;c=b;b=a;a=t+u;}
    s->h[0]+=a;s->h[1]+=b;s->h[2]+=c;s->h[3]+=d;s->h[4]+=e;s->h[5]+=f;s->h[6]+=g;s->h[7]+=h;
}
static void init(struct sha *s)
{
    memset(s,0,sizeof(*s));const uint32_t h[8]={0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};memcpy(s->h,h,sizeof(h));
}
static void add(struct sha *s,const unsigned char *p,size_t n)
{
    s->bytes+=(uint64_t)n;
    while(n){size_t k=64u-s->used;if(k>n)k=n;memcpy(s->block+s->used,p,k);s->used+=(unsigned)k;p+=k;n-=k;if(s->used==64){transform(s);s->used=0;}}
}
static void finish(struct sha *s,char hex[65])
{
    uint64_t bits=s->bytes*8;unsigned char one=0x80,zero=0,tail[8];add(s,&one,1);
    while(s->used!=56)add(s,&zero,1);
    for(unsigned i=0;i<8;++i)tail[i]=(unsigned char)(bits>>(56u-8u*i));
    add(s,tail,8);static const char digits[]="0123456789abcdef";
    for(unsigned i=0;i<32;++i){unsigned char x=(unsigned char)(s->h[i/4]>>(24u-8u*(i%4)));hex[2*i]=digits[x>>4];hex[2*i+1]=digits[x&15u];}hex[64]='\0';
}
void bench_sha256_bytes(const void *data,size_t n,char hex[65])
{struct sha s;init(&s);add(&s,data,n);finish(&s,hex);}
int bench_sha256_file(const char *path,char hex[65],uint64_t *bytes)
{
    FILE *f=fopen(path,"rb");if(f==NULL)return -1;struct sha s;init(&s);unsigned char buffer[16384];size_t n;
    while((n=fread(buffer,1,sizeof(buffer),f))!=0){if(s.bytes>UINT64_MAX/8-(uint64_t)n){(void)fclose(f);errno=EOVERFLOW;return -1;}add(&s,buffer,n);}
    if(ferror(f)){int e=errno;(void)fclose(f);errno=e?e:EIO;return -1;}
    if(fclose(f)!=0)return -1;
    *bytes=s.bytes;finish(&s,hex);return 0;
}
int bench_executable_path(char *p,size_t capacity)
{
    if(capacity<2){errno=EINVAL;return -1;}
#ifdef __APPLE__
    if(capacity>UINT32_MAX){errno=EOVERFLOW;return -1;}uint32_t n=(uint32_t)capacity;
    if(_NSGetExecutablePath(p,&n)!=0){errno=ENAMETOOLONG;return -1;}
    /* Keep this path only as an observation. Offline users pass --binary. */
    return 0;
#elif defined(__linux__)
    ssize_t n=readlink("/proc/self/exe",p,capacity-1);
    if(n<0)return -1;
    if((size_t)n>=capacity-1){errno=ENAMETOOLONG;return -1;}
    p[n]='\0';return 0;
#else
    (void)p;errno=ENOTSUP;return -1;
#endif
}
void bench_build_identity_json(FILE *f)
{for(size_t i=0;i<sizeof(elite_cache_build_parts)/sizeof(elite_cache_build_parts[0]);++i)(void)fputs(elite_cache_build_parts[i],f);}
