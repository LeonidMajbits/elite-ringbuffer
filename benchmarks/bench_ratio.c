#include "bench_ratio.h"
#include <errno.h>
#include <stdbool.h>
#include <string.h>
/* Five little-endian limbs: 128-bit products plus space for a decimal digit. */
struct wide { uint32_t v[5]; };
static struct wide product(uint64_t a,uint64_t b)
{
    struct wide r={{0}};uint32_t x[2]={(uint32_t)a,(uint32_t)(a>>32)};
    uint32_t y[2]={(uint32_t)b,(uint32_t)(b>>32)};
    for(unsigned i=0;i<2;++i){uint64_t carry=0;
        for(unsigned j=0;j<2;++j){uint64_t v=(uint64_t)x[i]*y[j]+r.v[i+j]+carry;
            r.v[i+j]=(uint32_t)v;carry=v>>32;}
        r.v[i+2]=(uint32_t)carry;}
    return r;
}
static bool nonzero(const struct wide *a)
{for(unsigned i=0;i<5;++i)if(a->v[i])return true;return false;}
static int compare(const struct wide *a,const struct wide *b)
{for(unsigned k=5;k>0;--k){unsigned i=k-1;if(a->v[i]!=b->v[i])return a->v[i]>b->v[i]?1:-1;}return 0;}
static void subtract(struct wide *a,const struct wide *b)
{
    uint64_t borrow=0;for(unsigned i=0;i<5;++i){uint64_t x=a->v[i],y=(uint64_t)b->v[i]+borrow;
        a->v[i]=(uint32_t)(x-y);borrow=x<y?1u:0u;}
}
static void shift(struct wide *a,uint32_t bit)
{for(unsigned i=0;i<5;++i){uint32_t next=a->v[i]>>31;a->v[i]=(a->v[i]<<1)|bit;bit=next;}}
static void divide(const struct wide *a,const struct wide *d,struct wide *q,struct wide *r)
{
    memset(q,0,sizeof(*q));memset(r,0,sizeof(*r));
    for(unsigned k=160;k>0;--k){unsigned i=k-1;shift(r,(a->v[i/32]>>(i%32))&1u);
        if(compare(r,d)>=0){subtract(r,d);q->v[i/32]|=UINT32_C(1)<<(i%32);}}
}
static unsigned div10(struct wide *a)
{uint64_t rem=0;for(unsigned k=5;k>0;--k){unsigned i=k-1;uint64_t v=(rem<<32)|a->v[i];a->v[i]=(uint32_t)(v/10);rem=v%10;}return (unsigned)rem;}
static void mul10(struct wide *a)
{uint64_t c=0;for(unsigned i=0;i<5;++i){uint64_t x=(uint64_t)a->v[i]*10+c;a->v[i]=(uint32_t)x;c=x>>32;}}
int bench_json_ratio(FILE *out,uint64_t n1,uint64_t n2,uint64_t d1,uint64_t d2)
{
    if(out==NULL||d1==0||d2==0){errno=EINVAL;return -1;}
    struct wide n=product(n1,n2),d=product(d1,d2),q,r;divide(&n,&d,&q,&r);
    char digits[64];unsigned count=0;do{digits[count++]=(char)('0'+div10(&q));}while(nonzero(&q));
    while(count)if(fputc(digits[--count],out)==EOF)return -1;
    if(fputc('.',out)==EOF)return -1;
    for(unsigned i=0;i<80;++i){mul10(&r);unsigned digit=0;while(compare(&r,&d)>=0){subtract(&r,&d);++digit;}
        if(fputc((int)('0'+digit),out)==EOF)return -1;
        if(!nonzero(&r))break;
    }
    return ferror(out)?-1:0;
}
