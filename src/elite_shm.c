#include "elite_internal.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/syscall.h>
#endif

/* The bounded manager is single-owner. This is application control-plane
 * coordination, NOT part of the lock-free data path or automatic takeover. */
struct el_holder {
    pid_t pid;
    int pidfd;
    uint32_t registered;
    uint32_t issued;
    uint32_t resolved;
};
struct elite_object {
    struct elite_immutable_header info;
    elite_authority *authority;
    void *mapping;
    size_t bytes;
    int fd;
    char name[ELITE_NAME_BYTES];
    uint32_t slot;
    uint32_t created;
    uint32_t constructed;
    uint32_t active;
    uint32_t quarantined;
    uint32_t no_more_grants;
    uint32_t cleanup_uncertain;
    struct el_holder *holders;
};
struct elite_authority {
    uint64_t magic;
    uint8_t host_id[16];
    uint8_t authority_id[16];
    uint64_t max_bytes;
    uint64_t next_epoch;
    pid_t owner_pid;
    elite_object *objects[4];
};
static bool valid_authority(const elite_authority *a)
{ return a!=NULL && a->magic==ELITE_AUTH_MAGIC && a->owner_pid==getpid(); }
static bool valid_object(const elite_object *o)
{ return o!=NULL && valid_authority(o->authority) && o->slot<4 && o->authority->objects[o->slot]==o; }
static _Atomic uint64_t *object_gate(elite_object *o)
{ return &((struct elite_cell64 *)((unsigned char *)o->mapping+512))->value; }
static _Atomic uint64_t *object_failure(elite_object *o)
{ return &((struct elite_cell64 *)((unsigned char *)o->mapping+640))->value; }
static struct elite_participant_record *object_participants(elite_object *o)
{ return (struct elite_participant_record *)((unsigned char *)o->mapping+ELITE_QUANTUM); }

static void name_from_id(const uint8_t id[16],char name[ELITE_NAME_BYTES])
{
    static const char alphabet[]="abcdefghijklmnopqrstuvwxyz234567";
    memcpy(name,"/el-",4);
    for(unsigned group=0;group<26;++group) {
        unsigned v=0;
        for(unsigned bit=0;bit<5;++bit) {
            unsigned pos=group*5+bit;
            unsigned x=pos<128 ? (((unsigned)id[pos/8] >> (7u - (pos % 8u))) & 1u):0u;
            v=(v<<1)|x;
        }
        name[4+group]=alphabet[v];
    }
    name[30]='\0';
}

static int id_compare(const void *a,const void *b) { return memcmp(a,b,16); }
static elite_result definitions_valid(const elite_endpoint_definition *defs,const struct elite_immutable_header *h)
{
    if(defs==NULL) return el_result(ELITE_INVALID_ARGUMENT,ELITE_NONE,0);
    size_t k=h->endpoint_count;
    if(k>SIZE_MAX/16) return el_result(ELITE_BAD_LAYOUT,ELITE_NONE,0);
    unsigned char *ids=malloc(k*16);
    if(ids==NULL) return el_result(ELITE_OS_ERROR,ELITE_NONE,ENOMEM);
    uint32_t p=0,cons=0;
    for(size_t i=0;i<k;++i) {
        if(el_zero(defs[i].endpoint_id,16)||el_zero(defs[i].process_incarnation_id,16)||
            (defs[i].role!=ELITE_PRODUCER && defs[i].role!=ELITE_CONSUMER)) {
            free(ids); return el_result(ELITE_BAD_IDENTITY,ELITE_NONE,0);
        }
        if(defs[i].role==ELITE_PRODUCER) ++p; else ++cons;
        memcpy(ids+16*i,defs[i].endpoint_id,16);
    }
    qsort(ids,k,16,id_compare);
    bool duplicate=false;
    for(size_t i=1;i<k;++i) if(memcmp(ids+16*(i-1),ids+16*i,16)==0) duplicate=true;
    free(ids);
    if(duplicate || p!=h->producer_endpoints || cons!=h->consumer_endpoints)
        return el_result(ELITE_BAD_IDENTITY,ELITE_NONE,0);
    return el_result(ELITE_OK,ELITE_NONE,0);
}

elite_result elite_authority_create(const uint8_t host[16],const uint8_t id[16],
    uint64_t max_bytes,elite_authority **out)
{
    if(out==NULL) return el_result(ELITE_INVALID_ARGUMENT,ELITE_NONE,0);
    *out=NULL;
    if(host==NULL || id==NULL || el_zero(host,16)||el_zero(id,16)||max_bytes<ELITE_QUANTUM ||
        max_bytes>(uint64_t)PTRDIFF_MAX) return el_result(ELITE_INVALID_ARGUMENT,ELITE_NONE,0);
    elite_result r=elite_platform_admit(); if(r.status!=ELITE_OK) return r;
    elite_authority *a=calloc(1,sizeof(*a));
    if(a==NULL) return el_result(ELITE_OS_ERROR,ELITE_NONE,ENOMEM);
    a->magic=ELITE_AUTH_MAGIC; memcpy(a->host_id,host,16); memcpy(a->authority_id,id,16);
    a->max_bytes=max_bytes; a->owner_pid=getpid(); *out=a;
    return el_result(ELITE_OK,ELITE_NONE,0);
}
elite_result elite_authority_destroy(elite_authority **pa)
{
    if(pa==NULL || !valid_authority(*pa)) return el_result(ELITE_INVALID_ARGUMENT,ELITE_NONE,0);
    for(unsigned i=0;i<4;++i) if((*pa)->objects[i]!=NULL) return el_result(ELITE_BUSY,ELITE_RETAINED,0);
    (*pa)->magic=0; free(*pa); *pa=NULL; return el_result(ELITE_OK,ELITE_NONE,0);
}

static void init_cell64(struct elite_cell64 *cell,uint64_t value) { atomic_init(&cell->value,value); }
static void init_cell32(struct elite_cell32 *cell) { atomic_init(&cell->value,0); }
static void construct(elite_object *o,const elite_endpoint_definition *defs)
{
    unsigned char *base=o->mapping;
    /* Raw, exclusively owned allocation: never do this to an exposed object. */
    memset(base,0,o->bytes);
    if(o->info.layout_profile==ELITE_SPSC) {
        struct elite_spsc_ring_header *h=o->mapping;
        h->immutable=o->info;
        init_cell64(&h->admission,ELITE_BUILDING); init_cell64(&h->failure,0);
        init_cell64(&h->published,0); init_cell64(&h->reclaimed,0);
        init_cell32(&h->data_wait); init_cell32(&h->space_wait);
    } else {
        struct elite_mpmc_ncq_header *h=o->mapping;
        h->immutable=o->info;
        init_cell64(&h->admission,ELITE_BUILDING); init_cell64(&h->failure,0);
        init_cell64(&h->qf_head,0); init_cell64(&h->qf_tail,o->info.capacity);
        init_cell64(&h->qr_head,o->info.capacity); init_cell64(&h->qr_tail,o->info.capacity);
        init_cell32(&h->reserved_data_wait); init_cell32(&h->reserved_space_wait);
        struct elite_ncq_entry_cell *f=(struct elite_ncq_entry_cell *)(base+o->info.qf_entries_offset);
        struct elite_ncq_entry_cell *r=(struct elite_ncq_entry_cell *)(base+o->info.qr_entries_offset);
        for(uint64_t i=0;i<o->info.capacity;++i) {
            atomic_init(&f[i].cycle_index,i); atomic_init(&r[i].cycle_index,0);
        }
    }
    struct elite_participant_record *parts=object_participants(o);
    for(uint32_t i=0;i<o->info.endpoint_count;++i) {
        memcpy(parts[i].endpoint_id,defs[i].endpoint_id,16);
        memcpy(parts[i].process_incarnation_id,defs[i].process_incarnation_id,16);
        parts[i].endpoint_index=i; parts[i].role=defs[i].role;
        parts[i].max_outstanding_tokens=1; parts[i].flags=0; parts[i].grant_epoch=1;
        atomic_init(&parts[i].attachment.value,ELITE_PART_NEW);
    }
    struct elite_slot_descriptor *ds=(struct elite_slot_descriptor *)(base+o->info.descriptors_offset);
    for(uint64_t i=0;i<o->info.capacity;++i) {
        ds[i].epoch=0; atomic_init(&ds[i].status_word,0);
        ds[i].payload_length=0; ds[i].message_type=0; ds[i].checksum=0; ds[i].message_id=0;
    }
    o->constructed=1;
    elite_store_release_u64(object_gate(o),ELITE_READY);
}

elite_result elite_create(elite_authority *a,const elite_config *cfg,
    const elite_endpoint_definition *defs,elite_object **out)
{
    if(out==NULL) return el_result(ELITE_INVALID_ARGUMENT,ELITE_NONE,0);
    *out=NULL;
    if(!valid_authority(a)||cfg==NULL) return el_result(ELITE_AUTHORITY_REQUIRED,ELITE_NONE,0);
    unsigned slot=4;
    for(unsigned i=0;i<4;++i) {
        elite_object *x=a->objects[i];
        if(x==NULL) { if(slot==4) slot=i; }
        else if(!x->active && !x->quarantined) return el_result(ELITE_BUSY,ELITE_RETAINED,0);
    }
    if(slot==4 || a->next_epoch>=UINT64_MAX-1) return el_result(ELITE_BUSY,ELITE_RETAINED,0);
    struct elite_immutable_header h;
    memset(&h,0,sizeof(h)); memcpy(h.magic,"ELITEIPC",8);
    h.abi_version=ELITE_ABI_VERSION; h.header_bytes=2048; h.immutable_bytes=512;
    h.layout_profile=cfg->layout_profile; h.endian_tag=UINT32_C(0x01020304);
    h.isolation_bytes=128; h.mapping_quantum=16384; h.atomic_abi_id=1;
    h.wait_mode=cfg->wait_mode; h.payload_checksum_mode=cfg->payload_checksum_mode;
    h.capacity=cfg->capacity; h.max_payload_bytes=cfg->max_payload_bytes;
    h.producer_endpoints=cfg->producer_endpoints; h.consumer_endpoints=cfg->consumer_endpoints;
    h.lifecycle_profile=1; h.max_backing_objects=4; h.max_quarantined_objects=2;
    h.max_backing_bytes=a->max_bytes; h.creation_utc_ns=cfg->creation_utc_ns;
    elite_result r=el_geometry(&h); if(r.status!=ELITE_OK) return r;
    r=definitions_valid(defs,&h); if(r.status!=ELITE_OK) return r;
    memcpy(h.host_instance_id,a->host_id,16); memcpy(h.authority_instance_id,a->authority_id,16);
    uint64_t newest=0;
    for(unsigned i=0;i<4;++i) if(a->objects[i]!=NULL && a->objects[i]->info.authority_epoch>newest) {
        newest=a->objects[i]->info.authority_epoch;
        memcpy(h.predecessor_session_id,a->objects[i]->info.session_id,16);
    }
    /* A one-to-one local session sequence, not random nonce collision claims.
     * Caller must not reuse the authority identity across unresolved domains. */
    do {
        h.authority_epoch=++a->next_epoch;
        memcpy(h.session_id,a->authority_id,16);
        for(unsigned i=0;i<8;++i) h.session_id[8+i]^=(uint8_t)(h.authority_epoch>>(8*i));
    } while(el_zero(h.session_id,16));
    h.header_crc32=elite_header_crc32(&h);
    elite_object *o=el_alloc_aligned(sizeof(*o));
    if(o==NULL) return el_result(ELITE_OS_ERROR,ELITE_NONE,errno);
    o->info=h; o->authority=a; o->slot=slot; o->fd=-1; o->bytes=(size_t)h.segment_bytes;
    o->holders=calloc(h.endpoint_count,sizeof(*o->holders));
    if(o->holders==NULL) { free(o); return el_result(ELITE_OS_ERROR,ELITE_NONE,ENOMEM); }
    for(uint32_t i=0;i<h.endpoint_count;++i) o->holders[i].pidfd=-1;
    name_from_id(h.session_id,o->name);
    a->objects[slot]=o; *out=o; /* Reserve bounded ledger BEFORE OS creation. */
    o->fd=shm_open(o->name,O_RDWR|O_CREAT|O_EXCL,0600);
    if(o->fd<0) {
        int e=errno; a->objects[slot]=NULL; free(o->holders); free(o); *out=NULL;
        return el_result(e==EEXIST?ELITE_CREATE_CONFLICT:ELITE_OS_ERROR,ELITE_NONE,e);
    }
    o->created=1;
    if(fcntl(o->fd,F_SETFD,FD_CLOEXEC)<0)
        return el_result(ELITE_OS_ERROR,ELITE_RETAINED,errno);
#ifndef __APPLE__
    /* Linux shm descriptors support chmod. Darwin PSXSHM is not a vnode:
     * retain the owner-only creation mode instead (Gemini metal finding). */
    if(fchmod(o->fd,0600)<0)
        return el_result(ELITE_OS_ERROR,ELITE_RETAINED,errno);
#endif
    /* Do not temporarily alter the process-wide umask. A restrictive umask
     * that removed owner access fails closed before any grant is issued. */
    struct stat created_stat;
    if(fstat(o->fd,&created_stat)<0)
        return el_result(ELITE_OS_ERROR,ELITE_RETAINED,errno);
    if(created_stat.st_uid!=geteuid() || (created_stat.st_mode & 0777)!=0600)
        return el_result(ELITE_OS_ERROR,ELITE_RETAINED,EACCES);
    if(ftruncate(o->fd,(off_t)o->bytes)<0)
        return el_result(ELITE_OS_ERROR,ELITE_RETAINED,errno);
    o->mapping=mmap(NULL,o->bytes,PROT_READ|PROT_WRITE,MAP_SHARED,o->fd,0);
    if(o->mapping==MAP_FAILED) { o->mapping=NULL; return el_result(ELITE_OS_ERROR,ELITE_RETAINED,errno); }
    if((uintptr_t)o->mapping%ELITE_ISOLATION!=0) return el_result(ELITE_BAD_LAYOUT,ELITE_RETAINED,0);
    construct(o,defs);
    return el_result(ELITE_OK,ELITE_NONE,0);
}

elite_result elite_object_activate(elite_object *o)
{
    if(!valid_object(o)||!o->constructed) return el_result(ELITE_INVALID_ARGUMENT,ELITE_NONE,0);
    if(o->no_more_grants||o->quarantined||el_state(elite_load_acquire_u64(object_gate(o)))!=ELITE_READY)
        return el_result(ELITE_RETIRED,ELITE_NONE,0);
    for(unsigned i=0;i<4;++i) if(o->authority->objects[i]!=NULL && o->authority->objects[i]!=o && o->authority->objects[i]->active)
        return el_result(ELITE_BUSY,ELITE_NONE,0);
    o->active=1; return el_result(ELITE_OK,ELITE_NONE,0);
}
elite_result elite_object_register_process(elite_object *o,uint32_t index,pid_t pid)
{
    if(!valid_object(o)||!o->constructed||index>=o->info.endpoint_count||pid<=0)
        return el_result(ELITE_INVALID_ARGUMENT,ELITE_NONE,0);
    struct el_holder *h=&o->holders[index];
    if(h->registered||o->no_more_grants) return el_result(ELITE_BUSY,ELITE_NONE,0);
    /* An owned unreaped child cannot have its PID reused. No other thread may
     * reap it outside this authority contract. Obtain Linux identity handle
     * before the grant can expose any mapping. */
    if(pid!=o->authority->owner_pid) {
        siginfo_t si;
        memset(&si,0,sizeof(si));
        if(waitid(P_PID,(id_t)pid,&si,WEXITED|WNOHANG|WNOWAIT)<0)
            return el_result(ELITE_AUTHORITY_REQUIRED,ELITE_NONE,errno);
#if defined(__linux__) && defined(SYS_pidfd_open)
        long fd=syscall(SYS_pidfd_open,pid,0);
        if(fd<0) return el_result(errno==ENOSYS?ELITE_UNSUPPORTED:ELITE_OS_ERROR,ELITE_NONE,errno);
        h->pidfd=(int)fd;
#elif defined(__linux__)
        return el_result(ELITE_UNSUPPORTED,ELITE_NONE,0);
#endif
    }
    h->pid=pid; h->registered=1;
    return el_result(ELITE_OK,ELITE_NONE,0);
}
elite_result elite_object_grant(elite_object *o,uint32_t i,elite_grant *g)
{
    if(g==NULL) return el_result(ELITE_INVALID_ARGUMENT,ELITE_NONE,0);
    memset(g,0,sizeof(*g));
    if(!valid_object(o)||!o->constructed||i>=o->info.endpoint_count)
        return el_result(ELITE_INVALID_ARGUMENT,ELITE_NONE,0);
    if(!o->active || o->no_more_grants || el_state(elite_load_acquire_u64(object_gate(o)))!=ELITE_READY ||
        elite_load_acquire_u64(object_failure(o))!=0) return el_result(ELITE_RETIRED,ELITE_NONE,0);
    struct el_holder *h=&o->holders[i];
    if(!h->registered || h->issued || h->resolved) return el_result(ELITE_AUTHORITY_REQUIRED,ELITE_NONE,0);
    struct elite_participant_record *p=&object_participants(o)[i];
    memcpy(g->name,o->name,ELITE_NAME_BYTES); g->constructed=ELITE_CONSTRUCTED;
    g->segment_bytes=o->info.segment_bytes; g->header_crc32=o->info.header_crc32;
    g->layout_profile=o->info.layout_profile; g->atomic_abi_id=1; g->endpoint_index=i;
    g->role=p->role; g->authority_epoch=o->info.authority_epoch; g->grant_epoch=1;
    memcpy(g->session_id,o->info.session_id,16); memcpy(g->host_instance_id,o->info.host_instance_id,16);
    memcpy(g->authority_instance_id,o->info.authority_instance_id,16);
    memcpy(g->endpoint_id,p->endpoint_id,16); memcpy(g->process_incarnation_id,p->process_incarnation_id,16);
    h->issued=1; /* Potential holder recorded BEFORE the caller sends the grant. */
    return el_result(ELITE_OK,ELITE_NONE,0);
}
elite_result elite_object_ack_cleanup(elite_object *o,const elite_cleanup_receipt *r)
{
    if(!valid_object(o)||!o->constructed||r==NULL||r->endpoint_index>=o->info.endpoint_count||r->local_cleanup_complete!=1||r->owns_grant_claim!=1)
        return el_result(ELITE_INVALID_ARGUMENT,ELITE_NONE,0);
    struct elite_participant_record *p=&object_participants(o)[r->endpoint_index];
    if(memcmp(r->session_id,o->info.session_id,16)||memcmp(r->endpoint_id,p->endpoint_id,16)||
        memcmp(r->process_incarnation_id,p->process_incarnation_id,16)) return el_result(ELITE_BAD_IDENTITY,ELITE_NONE,0);
    uint64_t attachment=elite_load_acquire_u64(&p->attachment.value);
    if(attachment!=ELITE_PART_QUIESCENT && attachment!=ELITE_PART_REJECTED)
        return el_result(ELITE_BUSY,ELITE_RETAINED,0);
    struct el_holder *h=&o->holders[r->endpoint_index];
    if(!h->issued) return el_result(ELITE_BAD_IDENTITY,ELITE_NONE,0);
    /* The trusted application receipt covers aliases and final mapping cleanup,
     * not merely a sampled QUIESCENT word. Idempotent for identical receipt. */
    h->resolved=1; return el_result(ELITE_OK,ELITE_NONE,0);
}
elite_result elite_authority_reap_child(elite_authority *a,pid_t pid,int *status)
{
    if(!valid_authority(a)||pid<=0||pid==a->owner_pid||status==NULL) return el_result(ELITE_INVALID_ARGUMENT,ELITE_NONE,0);
    bool known=false;
    for(unsigned j=0;j<4;++j) if(a->objects[j]!=NULL)
        for(uint32_t i=0;i<a->objects[j]->info.endpoint_count;++i) {
            struct el_holder *h=&a->objects[j]->holders[i];
            if(h->registered&&h->pid==pid&&!h->resolved) known=true;
        }
    if(!known) return el_result(ELITE_BAD_IDENTITY,ELITE_NONE,0);
    int s=0; pid_t got=waitpid(pid,&s,WNOHANG);
    if(got<0) return el_result(errno==ECHILD?ELITE_AUTHORITY_REQUIRED:ELITE_OS_ERROR,ELITE_NONE,errno);
    if(got==0 || (!WIFEXITED(s)&&!WIFSIGNALED(s))) return el_result(ELITE_NOT_READY,ELITE_NONE,0);
    *status=s;
    for(unsigned j=0;j<4;++j) if(a->objects[j]!=NULL) {
        elite_object *o=a->objects[j]; bool touched=false;
        for(uint32_t i=0;i<o->info.endpoint_count;++i) {
            struct el_holder *h=&o->holders[i];
            if(h->registered&&h->pid==pid&&!h->resolved) {h->resolved=1; touched=true;}
        }
        if(touched&&o->constructed) (void)elite_object_retire(o,ELITE_FAILURE_PEER);
    }
    return el_result(ELITE_OK,ELITE_NONE,0);
}

static int notify_object(elite_object *o)
{
    if(o->info.wait_mode!=ELITE_PARKABLE_SPSC) return 0;
    struct elite_spsc_ring_header *h=o->mapping;
    int a=el_notify(&h->data_wait.value),b=el_notify(&h->space_wait.value);
    return a?a:b;
}
elite_result elite_object_retire(elite_object *o,uint64_t reason)
{
    if(!valid_object(o)||!o->constructed) return el_result(ELITE_INVALID_ARGUMENT,ELITE_NONE,0);
    if(reason>ELITE_FAILURE_AUTHORITY) return el_result(ELITE_INVALID_ARGUMENT,ELITE_NONE,0);
    o->no_more_grants=1;
    elite_result r=el_retire_raw(object_gate(o),object_failure(o),o->info.endpoint_count,reason);
    if(r.status==ELITE_OK) { int e=notify_object(o); if(e) r=el_result(ELITE_OS_ERROR,ELITE_NONE,e); }
    return r;
}
elite_result elite_object_quarantine(elite_object *o)
{
    if(!valid_object(o)||!o->constructed) return el_result(ELITE_INVALID_ARGUMENT,ELITE_NONE,0);
    if(o->quarantined) return el_result(ELITE_OK,ELITE_NONE,0);
    unsigned count=0;
    for(unsigned i=0;i<4;++i) if(o->authority->objects[i]!=NULL && o->authority->objects[i]->quarantined) ++count;
    if(count>=2) { (void)elite_object_retire(o,ELITE_FAILURE_RESOURCE); return el_result(ELITE_BUSY,ELITE_RETAINED,0); }
    elite_result r=elite_object_retire(o,0); if(r.status!=ELITE_OK) return r;
    for(;;) {
        uint64_t old=atomic_load_explicit(object_gate(o),memory_order_seq_cst);
        if(!el_gate_valid(old,o->info.endpoint_count)||el_state(old)==ELITE_SEALED)
            return el_result(ELITE_INTEGRITY,ELITE_RETAINED,0);
        uint64_t desired=(old&UINT64_C(0xffffffff00000000))|ELITE_QUARANTINED;
        if(atomic_compare_exchange_strong_explicit(object_gate(o),&old,desired,memory_order_seq_cst,memory_order_seq_cst)) break;
    }
    o->quarantined=1; o->active=0; return el_result(ELITE_OK,ELITE_RETAINED,0);
}
elite_result elite_object_info(elite_object *o,struct elite_immutable_header *h)
{
    if(!valid_object(o)||h==NULL) return el_result(ELITE_INVALID_ARGUMENT,ELITE_NONE,0);
    *h=o->info; return el_result(ELITE_OK,ELITE_NONE,0);
}
elite_result elite_object_destroy(elite_object **po)
{
    if(po==NULL||!valid_object(*po)) return el_result(ELITE_INVALID_ARGUMENT,ELITE_NONE,0);
    elite_object *o=*po; o->no_more_grants=1;
    if(o->cleanup_uncertain) return el_result(ELITE_OUTCOME_UNCERTAIN,ELITE_RETAINED,0);
    if(o->constructed) {
        (void)elite_object_retire(o,0);
        for(uint32_t i=0;i<o->info.endpoint_count;++i)
            if(o->holders[i].issued&&!o->holders[i].resolved) return el_result(ELITE_BUSY,ELITE_RETAINED,0);
        /* All potential holders resolved by trusted receipts/terminal evidence.
         * Only NOW may an inflated count be reconciled. No token reclamation. */
        for(;;) {
            uint64_t old=atomic_load_explicit(object_gate(o),memory_order_seq_cst);
            if(!el_gate_valid(old,o->info.endpoint_count)) return el_result(ELITE_INTEGRITY,ELITE_RETAINED,0);
            if(atomic_compare_exchange_strong_explicit(object_gate(o),&old,ELITE_SEALED,
                memory_order_seq_cst,memory_order_seq_cst)) break;
        }
    }
    if(o->created) {
        if(shm_unlink(o->name)<0 && errno!=ENOENT) return el_result(ELITE_OS_ERROR,ELITE_RETAINED,errno);
        o->created=0;
    }
    if(o->mapping!=NULL) {
        if(munmap(o->mapping,o->bytes)<0) return el_result(ELITE_OS_ERROR,ELITE_RETAINED,errno);
        o->mapping=NULL; o->constructed=0;
    }
    if(o->fd>=0) {
        int fd=o->fd; o->fd=-1;
        if(close(fd)<0) {o->cleanup_uncertain=1;return el_result(ELITE_OS_ERROR,ELITE_RETAINED,errno);}
    }
    for(uint32_t i=0;i<o->info.endpoint_count;++i) if(o->holders[i].pidfd>=0) {
        int fd=o->holders[i].pidfd; o->holders[i].pidfd=-1;
        if(close(fd)<0) {o->cleanup_uncertain=1;return el_result(ELITE_OS_ERROR,ELITE_RETAINED,errno);}
    }
    o->authority->objects[o->slot]=NULL; free(o->holders); free(o); *po=NULL;
    return el_result(ELITE_OK,ELITE_NONE,0);
}

static bool reserved_ok(elite_connection *c)
{
    const unsigned char *b=c->mapping;
    const struct elite_immutable_header *h=&c->info;
    if(!el_zero(b+2048,ELITE_QUANTUM-2048)) return false;
    if(c->spsc!=NULL) {
        if(!el_zero(c->spsc->admission.reserved,120)||!el_zero(c->spsc->failure.reserved,120)||
            !el_zero(c->spsc->published.reserved,120)||!el_zero(c->spsc->reclaimed.reserved,120)||
            !el_zero(c->spsc->data_wait.reserved,124)||!el_zero(c->spsc->space_wait.reserved,124)||
            !el_zero(c->spsc->reserved_500,768)) return false;
    } else {
        if(!el_zero(c->ncq->admission.reserved,120)||!el_zero(c->ncq->failure.reserved,120)||
            !el_zero(c->ncq->qf_head.reserved,120)||!el_zero(c->ncq->qf_tail.reserved,120)||
            !el_zero(c->ncq->qr_head.reserved,120)||!el_zero(c->ncq->qr_tail.reserved,120)||
            !el_zero(c->ncq->reserved_data_wait.reserved,124)||!el_zero(c->ncq->reserved_space_wait.reserved,124)||
            !el_zero(c->ncq->reserved_600,512)) return false;
    }
    uint64_t end=ELITE_QUANTUM+(uint64_t)256*h->endpoint_count;
    uint64_t first=c->spsc!=NULL?h->descriptors_offset:h->qf_entries_offset;
    if(!el_zero(b+end,(size_t)(first-end))) return false;
    for(uint64_t i=0;i<h->capacity;++i) {
        if(!el_zero(c->descriptors[i].reserved_028,88)) return false;
        if(!el_zero(c->payloads+i*h->payload_stride+h->max_payload_bytes,
            (size_t)(h->payload_stride-h->max_payload_bytes))) return false;
        if(c->ncq!=NULL && (!el_zero(c->qf[i].reserved,120)||!el_zero(c->qr[i].reserved,120))) return false;
    }
    if(c->ncq!=NULL) {
        end=h->qf_entries_offset+h->capacity*128;
        if(!el_zero(b+end,(size_t)(h->qr_entries_offset-end))) return false;
        end=h->qr_entries_offset+h->capacity*128;
        if(!el_zero(b+end,(size_t)(h->descriptors_offset-end))) return false;
    }
    end=h->descriptors_offset+h->capacity*128;
    if(!el_zero(b+end,(size_t)(h->payloads_offset-end))) return false;
    end=h->payloads_offset+h->capacity*h->payload_stride;
    return el_zero(b+end,(size_t)(h->segment_bytes-end));
}
static elite_result registry_ok(elite_connection *c,const elite_grant *g)
{
    struct elite_participant_record *parts=(struct elite_participant_record *)((unsigned char *)c->mapping+ELITE_QUANTUM);
    uint32_t p=0,cons=0,k=c->info.endpoint_count;
    unsigned char *ids=malloc((size_t)k*16);
    if(ids==NULL) return el_result(ELITE_OS_ERROR,ELITE_NONE,ENOMEM);
    for(uint32_t i=0;i<k;++i) {
        struct elite_participant_record *x=&parts[i];
        if(el_zero(x->endpoint_id,16)||el_zero(x->process_incarnation_id,16)||x->endpoint_index!=i||
            (x->role!=ELITE_PRODUCER&&x->role!=ELITE_CONSUMER)||x->max_outstanding_tokens!=1||x->flags!=0||
            x->grant_epoch!=1||!el_zero(x->reserved_038,72)||!el_zero(x->attachment.reserved,120)) {
            free(ids); return el_result(ELITE_BAD_LAYOUT,ELITE_NONE,0);
        }
        if(x->role==ELITE_PRODUCER) ++p; else ++cons;
        memcpy(ids+(size_t)i*16,x->endpoint_id,16);
    }
    qsort(ids,k,16,id_compare); bool duplicate=false;
    for(uint32_t i=1;i<k;++i) if(memcmp(ids+(size_t)(i-1)*16,ids+(size_t)i*16,16)==0) duplicate=true;
    free(ids);
    if(duplicate||p!=c->info.producer_endpoints||cons!=c->info.consumer_endpoints)
        return el_result(ELITE_BAD_LAYOUT,ELITE_NONE,0);
    c->participant=&parts[g->endpoint_index];
    if(memcmp(g->endpoint_id,c->participant->endpoint_id,16)||
        memcmp(g->process_incarnation_id,c->participant->process_incarnation_id,16)||
        g->role!=c->participant->role||g->grant_epoch!=c->participant->grant_epoch)
        return el_result(ELITE_BAD_IDENTITY,ELITE_NONE,0);
    return el_result(ELITE_OK,ELITE_NONE,0);
}

/* Cleanup on an unsuccessful pre-enrollment attach: normal errors do not
 * authorize reuse of the issued grant. Caller still supplies cleanup receipt
 * or terminal process evidence to the manager. */
static elite_result attach_cleanup(elite_connection *c,elite_result error,elite_connection **out)
{
    c->detached=1;
    if(c->mapping!=NULL) {
        if(munmap(c->mapping,c->mapping_bytes)<0) { *out=c; return el_result(ELITE_OS_ERROR,ELITE_RETAINED,errno); }
        c->mapping=NULL;
    }
    if(c->fd>=0) { int fd=c->fd; c->fd=-1; if(close(fd)<0) {c->cleanup_uncertain=1; *out=c; return el_result(ELITE_OS_ERROR,ELITE_RETAINED,errno); } }
    /* Keep a cleanup-only handle so a failed grant attempt has a receipt. */
    *out=c; return error;
}
elite_result elite_attach(const elite_grant *g,elite_connection **out)
{
    if(out==NULL) return el_result(ELITE_INVALID_ARGUMENT,ELITE_NONE,0);
    *out=NULL;
    if(g==NULL||g->constructed!=ELITE_CONSTRUCTED) return el_result(ELITE_AUTHORITY_REQUIRED,ELITE_NONE,0);
    elite_result r=elite_platform_admit(); if(r.status!=ELITE_OK) return r;
    char expected_name[ELITE_NAME_BYTES]; name_from_id(g->session_id,expected_name);
    if(memcmp(g->name,expected_name,ELITE_NAME_BYTES)||g->atomic_abi_id!=1||
        (g->layout_profile!=ELITE_SPSC&&g->layout_profile!=ELITE_NCQ)||
        g->segment_bytes<ELITE_QUANTUM||g->segment_bytes>(uint64_t)PTRDIFF_MAX||
        g->authority_epoch==0||g->grant_epoch!=1)
        return el_result(ELITE_BAD_IDENTITY,ELITE_NONE,0);
    elite_connection *c=el_alloc_aligned(sizeof(*c));
    if(c==NULL) return el_result(ELITE_OS_ERROR,ELITE_NONE,errno);
    c->magic=ELITE_CONN_MAGIC; c->fd=-1; c->mapping_bytes=(size_t)g->segment_bytes;
    memcpy(c->receipt.session_id,g->session_id,16); memcpy(c->receipt.endpoint_id,g->endpoint_id,16);
    memcpy(c->receipt.process_incarnation_id,g->process_incarnation_id,16);
    c->receipt.endpoint_index=g->endpoint_index;
    c->fd=shm_open(g->name,O_RDWR,0);
    if(c->fd<0) return attach_cleanup(c,el_result(ELITE_OS_ERROR,ELITE_NONE,errno),out);
    struct stat st;
    if(fstat(c->fd,&st)<0) return attach_cleanup(c,el_result(ELITE_OS_ERROR,ELITE_NONE,errno),out);
    if(st.st_size<0||(uint64_t)st.st_size!=g->segment_bytes||st.st_uid!=geteuid()||
        (st.st_mode&0777)!=0600) return attach_cleanup(c,el_result(ELITE_BAD_IDENTITY,ELITE_NONE,0),out);
    if(fcntl(c->fd,F_SETFD,FD_CLOEXEC)<0) return attach_cleanup(c,el_result(ELITE_OS_ERROR,ELITE_NONE,errno),out);
    c->mapping=mmap(NULL,c->mapping_bytes,PROT_READ|PROT_WRITE,MAP_SHARED,c->fd,0);
    if(c->mapping==MAP_FAILED) {c->mapping=NULL;return attach_cleanup(c,el_result(ELITE_OS_ERROR,ELITE_NONE,errno),out);}
    if((uintptr_t)c->mapping%128!=0) return attach_cleanup(c,el_result(ELITE_BAD_LAYOUT,ELITE_NONE,0),out);
    /* Fixed, already-constructed subobject promised by trusted grant. */
    c->gate=&((struct elite_cell64 *)((unsigned char *)c->mapping+512))->value;
    c->failure=&((struct elite_cell64 *)((unsigned char *)c->mapping+640))->value;
    uint64_t gate=elite_load_acquire_u64(c->gate);
    if(el_state(gate)==ELITE_BUILDING) return attach_cleanup(c,el_result(ELITE_NOT_READY,ELITE_NONE,0),out);
    r=elite_validate_prefix(c->mapping,512,g->segment_bytes,&c->info);
    if(r.status!=ELITE_OK) return attach_cleanup(c,r,out);
    if(!el_gate_valid(gate,c->info.endpoint_count)) return attach_cleanup(c,el_result(ELITE_INTEGRITY,ELITE_NONE,0),out);
    if(g->layout_profile!=c->info.layout_profile||g->header_crc32!=c->info.header_crc32||
        g->authority_epoch!=c->info.authority_epoch||g->endpoint_index>=c->info.endpoint_count||
        memcmp(g->session_id,c->info.session_id,16)||memcmp(g->host_instance_id,c->info.host_instance_id,16)||
        memcmp(g->authority_instance_id,c->info.authority_instance_id,16))
        return attach_cleanup(c,el_result(ELITE_BAD_IDENTITY,ELITE_NONE,0),out);
    unsigned char *b=c->mapping;
    c->descriptors=(struct elite_slot_descriptor *)(b+c->info.descriptors_offset);
    c->payloads=b+c->info.payloads_offset;
    if(c->info.layout_profile==ELITE_SPSC) c->spsc=c->mapping;
    else {c->ncq=c->mapping;c->qf=(struct elite_ncq_entry_cell *)(b+c->info.qf_entries_offset);c->qr=(struct elite_ncq_entry_cell *)(b+c->info.qr_entries_offset);}
    if(!reserved_ok(c)) return attach_cleanup(c,el_result(ELITE_BAD_LAYOUT,ELITE_NONE,0),out);
    r=registry_ok(c,g); if(r.status!=ELITE_OK) return attach_cleanup(c,r,out);
    uint64_t expected=ELITE_PART_NEW;
    if(!atomic_compare_exchange_strong_explicit(&c->participant->attachment.value,&expected,ELITE_PART_JOINING,
        memory_order_seq_cst,memory_order_seq_cst)) return attach_cleanup(c,el_result(ELITE_BAD_IDENTITY,ELITE_NONE,0),out);
    c->receipt.owns_grant_claim=1;
    for(;;) {
        gate=atomic_load_explicit(c->gate,memory_order_seq_cst);
        if(!el_gate_valid(gate,c->info.endpoint_count)||el_state(gate)!=ELITE_READY||el_count(gate)>=c->info.endpoint_count) {
            atomic_store_explicit(&c->participant->attachment.value,ELITE_PART_REJECTED,memory_order_release);
            return attach_cleanup(c,el_result(ELITE_RETIRED,ELITE_NONE,0),out);
        }
        if(atomic_compare_exchange_strong_explicit(c->gate,&gate,gate+(UINT64_C(1)<<32),
            memory_order_seq_cst,memory_order_seq_cst)) break;
    }
    c->enrolled=1; c->role=g->role;
    EL_HOOK(c,ELITE_HOOK_JOINED,0,0);
    atomic_store_explicit(&c->participant->attachment.value,ELITE_PART_ACTIVE,memory_order_release);
    if(c->spsc!=NULL) c->cursor=elite_load_acquire_u64(c->role==ELITE_PRODUCER?&c->spsc->published.value:&c->spsc->reclaimed.value);
    *out=c;
    return el_ready(c,false); /* RETIRED may return an enrolled cleanup obligation. */
}

elite_result elite_detach(elite_connection **pc,elite_cleanup_receipt *receipt)
{
    if(pc==NULL||*pc==NULL||(*pc)->magic!=ELITE_CONN_MAGIC||receipt==NULL)
        return el_result(ELITE_INVALID_ARGUMENT,ELITE_NONE,0);
    elite_connection *c=*pc;
    if(c->cleanup_uncertain) return el_result(ELITE_OUTCOME_UNCERTAIN,ELITE_RETAINED,0);
    if(c->in_call||c->owned||c->views) return el_result(ELITE_BUSY,ELITE_RETAINED,0);
    if(c->enrolled) {
        atomic_store_explicit(&c->participant->attachment.value,ELITE_PART_QUIESCENT,memory_order_release);
        for(;;) {
            uint64_t old=atomic_load_explicit(c->gate,memory_order_seq_cst);
            if(!el_gate_valid(old,c->info.endpoint_count)||el_count(old)==0)
                return el_result(ELITE_INTEGRITY,ELITE_RETAINED,0);
            if(atomic_compare_exchange_strong_explicit(c->gate,&old,old-(UINT64_C(1)<<32),
                memory_order_seq_cst,memory_order_seq_cst)) break;
        }
        /* DETACH LP: nothing below accesses shared storage. */
        c->enrolled=0; c->detached=1;
        EL_HOOK(c,ELITE_HOOK_DETACH_DECREMENTED,0,0);
    }
    if(c->mapping!=NULL) {
        if(munmap(c->mapping,c->mapping_bytes)<0) return el_result(ELITE_OS_ERROR,ELITE_RETAINED,errno);
        c->mapping=NULL;
    }
    if(c->fd>=0) { int fd=c->fd;c->fd=-1;if(close(fd)<0) {c->cleanup_uncertain=1;return el_result(ELITE_OS_ERROR,ELITE_RETAINED,errno);} }
    c->receipt.local_cleanup_complete=1; *receipt=c->receipt;
    c->magic=0; free(c); *pc=NULL;
    return el_result(ELITE_OK,ELITE_NONE,0);
}
