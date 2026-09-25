/* Turn 14. Owned-process V2/V8 fault harness. Production sources are unchanged.
 * Build twice: existing test hooks enabled, and the ordinary no-hook library.
 * Pipe messages coordinate tests and carry evidence/grants, NEVER payloads.
 * The authority-only observer reads ordinary descriptors only after endpoints
 * are idle/detached or terminal. It never repairs a queue or inserts an orphan.
 */
#include "test_support.h"
#include "elite_internal.h" /* verification-only ownership/address evidence */
#include "chaos_protocol.h"
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <sched.h>
#include <signal.h>
#include <spawn.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
extern char **environ;

struct worker { pid_t pid; int input, output, reaped, done; uint32_t id, role;
    uint64_t last_sequence, last_tick, received_tick, cut_block, cut_ticket;
    uint64_t completed, specials, usr, alrm, forbidden_block; unsigned char *bitmap; size_t bitmap_bytes; };
struct generation { elite_object *object; struct elite_immutable_header info;
    char name[ELITE_NAME_BYTES]; void *mapping; int fd; struct worker *workers[CH_MAX_WORKERS];
    uint32_t count, producers; };
static struct worker workers[CH_MAX_WORKERS * 2u];
static size_t worker_count;
static struct generation generations[4];
static size_t generation_count;
static elite_authority *authority;
static uint64_t deadline, log_sequence;
static int parent_process;
static volatile sig_atomic_t interrupted, saw_usr, saw_alarm;
static struct ch_config child_cfg;
static uint64_t child_sequence;
static elite_connection *child_connection;
static uint32_t hook_once;
static int cohort_retirement;
static struct worker *overdue_worker;
static uint64_t overdue_deadline;

static void signal_flag(int sig)
{ if(sig==SIGUSR1) saw_usr=1; else if(sig==SIGALRM) saw_alarm=1; else interrupted=1; }
static void check_time(void)
{ CHECK(!interrupted && (deadline==0 || test_now()<deadline)); }
static void delay_ns(uint64_t ns)
{ struct timespec ts={(time_t)(ns/UINT64_C(1000000000)),(long)(ns%UINT64_C(1000000000))};
  while(nanosleep(&ts,&ts)<0){CHECK(errno==EINTR);check_time();} }
static void install_signals(int restart)
{
    struct sigaction sa; memset(&sa,0,sizeof(sa));sa.sa_handler=signal_flag;
    CHECK(sigemptyset(&sa.sa_mask)==0);sa.sa_flags=restart?SA_RESTART:0;
    CHECK(sigaction(SIGUSR1,&sa,NULL)==0);CHECK(sigaction(SIGALRM,&sa,NULL)==0);
    CHECK(sigaction(SIGTERM,&sa,NULL)==0);CHECK(sigaction(SIGINT,&sa,NULL)==0);
    sa.sa_handler=SIG_IGN;CHECK(sigaction(SIGPIPE,&sa,NULL)==0);
}
static void io_read(int fd,void *ptr,size_t size)
{
    unsigned char *p=ptr;
    while(size){check_time();struct pollfd f={fd,POLLIN,0};int got=poll(&f,1,100);
        if(got<0&&errno==EINTR){continue;}
        CHECK(got>=0);if(got==0){continue;}
        ssize_t n=read(fd,p,size);if(n<0&&errno==EINTR)continue;CHECK(n>0);p+=(size_t)n;size-=(size_t)n;}
}
static void io_write(int fd,const void *ptr,size_t size)
{
    const unsigned char *p=ptr;
    while(size){check_time();ssize_t n=write(fd,p,size);if(n<0&&errno==EINTR)continue;
        CHECK(n>0);p+=(size_t)n;size-=(size_t)n;}
}
static void event_begin(const char *event)
{ printf("{\"event\":\"%s\",\"log_sequence\":%"PRIu64",\"observer_tick\":%"PRIu64,event,++log_sequence,test_now()); }
static void event_end(void){puts("}");CHECK(fflush(stdout)==0);}
static void simple_event(const char *event){event_begin(event);event_end();}
static void cleanup(void)
{
    if(!parent_process)return;
    /* Unreaped owned PIDs cannot be reused under this sole-waiter contract. */
    for(size_t i=0;i<worker_count;++i)if(!workers[i].reaped&&workers[i].pid>0)(void)kill(workers[i].pid,SIGKILL);
    for(size_t i=0;i<worker_count;++i)if(!workers[i].reaped&&workers[i].pid>0){
        int st=0;pid_t p;do{p=waitpid(workers[i].pid,&st,0);}while(p<0&&errno==EINTR);
        if(p==workers[i].pid)workers[i].reaped=1;}
    /* Only this invocation's exclusive-created names, after its child fences.
     * No unregistered descendant or transferred mapping is allowed. */
    for(size_t i=0;i<generation_count;++i){struct generation *g=&generations[i];
        if(g->mapping!=NULL){(void)munmap(g->mapping,(size_t)g->info.segment_bytes);g->mapping=NULL;}
        if(g->fd>=0){(void)close(g->fd);g->fd=-1;}
        if(g->name[0]!='\0')(void)shm_unlink(g->name);}
}
static void child_send(uint32_t kind,uint64_t a,uint64_t b,uint64_t c,uint64_t d)
{
    struct ch_event_record e;memset(&e,0,sizeof(e));e.event=kind;e.worker_id=child_cfg.worker_id;
    e.sequence=++child_sequence;e.tick=test_now();e.data[0]=a;e.data[1]=b;e.data[2]=c;e.data[3]=d;
    e.data[4]=(uint64_t)saw_usr;e.data[5]=(uint64_t)saw_alarm;
    io_write(STDOUT_FILENO,&e,sizeof(e));
}
static void cut_pause(uint32_t point,uint64_t ticket,uint64_t block)
{
    if(hook_once){return;}
    hook_once=1;
    child_send(CH_CUT,point,ticket,block,0);
    CHECK(raise(SIGSTOP)==0); /* Parent confirms WIFSTOPPED before injection. */
}
#ifdef ELITE_TESTING
static void fault_hook(void *unused,uint32_t point,uint64_t ticket,uint64_t block)
{(void)unused;if(point==child_cfg.cut)cut_pause(point,ticket,block);}
#endif
static void child_commit(uint64_t id)
{
    elite_lease l;elite_write_span s;
    elite_result r=elite_write_reserve(child_connection,&l,&s);CHECK(r.status==ELITE_OK);
    test_payload_write(s.data,id,0,id,child_cfg.seed);
    r=elite_write_commit(child_connection,&l,64,7,id);CHECK(r.status==ELITE_OK&&r.outcome==ELITE_PUBLISHED);
}
static int command_available(struct ch_command_record *cmd)
{
    struct pollfd f={STDIN_FILENO,POLLIN,0};int n=poll(&f,1,0);
    if(n<0&&errno==EINTR){return 0;}
    CHECK(n>=0);
    if(n==0){return 0;}
    io_read(STDIN_FILENO,cmd,sizeof(*cmd));return 1;
}
static void child_run(const struct ch_command_record *cmd)
{
    child_cfg.forbidden_block=cmd->forbidden_block;
    const int producer=child_cfg.grant.role==ELITE_PRODUCER;
    CHECK(cmd->total>0&&cmd->total<=CH_MAX_MESSAGES);
    size_t bytes=(size_t)((cmd->total+7)/8);
    unsigned char *bits=producer?NULL:calloc(bytes,1);CHECK(producer||bits!=NULL);
    uint64_t count=0,special=0,attempts=0,first=test_now(),retained_block=0;int draining=0;
    child_send(CH_STARTED,cmd->first,cmd->quota,cmd->total,producer?1u:0u);
    for(;;){
        check_time();
        if((attempts++ & UINT64_C(255))==0){struct ch_command_record x;
            if(command_available(&x)){CHECK(x.command==CH_DRAIN&&!producer);draining=1;}
            if((attempts & UINT64_C(4095))==1)child_send(CH_PROGRESS,count,special,attempts,draining?1u:0u);}
        if(producer){
            if(count==cmd->quota)break;
            uint64_t id=cmd->first+count;elite_lease l;elite_write_span s;
            elite_result r=elite_write_reserve(child_connection,&l,&s);
            if(r.status==ELITE_NO_CAPACITY_OBSERVED)continue;
            if(cmd->reserved&&r.status==ELITE_RETIRED){break;}
            CHECK(r.status==ELITE_OK);
            CHECK(child_cfg.forbidden_block==CH_NONE||child_connection->block!=child_cfg.forbidden_block);
            test_payload_write(s.data,id,0,id,child_cfg.seed);
            r=elite_write_commit(child_connection,&l,64,7,id);
            if(cmd->reserved&&r.status==ELITE_RETIRED&&r.outcome==ELITE_RETAINED){
                retained_block=child_connection->block+1;OK(elite_abandon_retained(child_connection,&l));break;}
            CHECK(r.status==ELITE_OK&&r.outcome==ELITE_PUBLISHED);++count;
        }else{
            elite_lease l;elite_read_span s;elite_result r=elite_read_borrow(child_connection,&l,&s);
            if(r.status==ELITE_NO_DATA_OBSERVED){if(draining)break;continue;}
            if(cmd->reserved&&r.status==ELITE_RETIRED){break;}
            CHECK(r.status==ELITE_OK&&s.length==64&&s.message_type==7);
            uint64_t id=s.message_id;
            CHECK(child_cfg.forbidden_block==CH_NONE||child_connection->block!=child_cfg.forbidden_block);
            CHECK(test_payload_check(s.data,id,0,id,child_cfg.seed));
            if(id==CH_SENTINEL){CHECK(++special==1);}else{
                CHECK(id<cmd->total);size_t b=(size_t)(id/8);unsigned char m=(unsigned char)(1u<<(unsigned)(id%8));
                CHECK((bits[b]&m)==0);bits[b]|=m;++count;}
            r=elite_read_release(child_connection,&l);
            if(cmd->reserved&&r.status==ELITE_RETIRED&&r.outcome==ELITE_RETAINED){
                retained_block=child_connection->block+1;OK(elite_abandon_retained(child_connection,&l));break;}
            CHECK(r.status==ELITE_OK&&r.outcome==ELITE_RETURNED);
        }
    }
    struct ch_event_record e;memset(&e,0,sizeof(e));e.event=CH_DONE;e.worker_id=child_cfg.worker_id;
    e.sequence=++child_sequence;e.tick=test_now();e.data[0]=count;e.data[1]=special;e.data[2]=producer?0:(uint64_t)bytes;
    e.data[3]=first;e.data[4]=(uint64_t)saw_usr;e.data[5]=(uint64_t)saw_alarm;e.data[6]=attempts;e.data[7]=retained_block;
    io_write(STDOUT_FILENO,&e,sizeof(e));if(!producer)io_write(STDOUT_FILENO,bits,bytes);free(bits);
}
static void child_victim(void)
{
    elite_lease l;elite_result r;hook_once=0;
#ifdef ELITE_TESTING
    OK(elite_test_set_hook(child_connection,fault_hook,NULL));
#endif
    if(child_cfg.grant.role==ELITE_PRODUCER){
        elite_write_span s;OK(elite_write_reserve(child_connection,&l,&s));
        uint64_t *w=s.data;w[0]=CH_SENTINEL;w[1]=~CH_SENTINEL;w[2]=0;w[3]=CH_SENTINEL;
        if(child_cfg.cut==CH_CUT_MID||child_cfg.cut==CH_CUT_WRITE)cut_pause(child_cfg.cut,0,child_connection->block);
        test_payload_write(s.data,CH_SENTINEL,0,CH_SENTINEL,child_cfg.seed);
        r=elite_write_commit(child_connection,&l,64,7,CH_SENTINEL);
    }else{
        elite_read_span s;OK(elite_read_borrow(child_connection,&l,&s));
        CHECK(s.message_id==CH_SENTINEL&&test_payload_check(s.data,CH_SENTINEL,0,CH_SENTINEL,child_cfg.seed));
        if(child_cfg.cut==CH_CUT_READ)cut_pause(CH_CUT_READ,0,child_connection->block);
        /* Resume from an old read checks its still-held bytes before return. */
        CHECK(test_payload_check(s.data,CH_SENTINEL,0,CH_SENTINEL,child_cfg.seed));
        r=elite_read_release(child_connection,&l);
    }
    CHECK(r.status==ELITE_OK||(r.status==ELITE_RETIRED&&r.outcome==ELITE_RETAINED));
    if(r.outcome==ELITE_RETAINED)OK(elite_abandon_retained(child_connection,&l));
#ifdef ELITE_TESTING
    OK(elite_test_set_hook(child_connection,NULL,NULL));
#endif
    child_send(CH_VICTIM_DONE,r.status,r.outcome,0,0);
}
static void child_random_abort(void)
{
    uint64_t count=0;
    for(;;){check_time();elite_lease l;elite_write_span span;
        elite_result r=elite_write_reserve(child_connection,&l,&span);
        if(r.status==ELITE_NO_CAPACITY_OBSERVED)continue;
        CHECK(r.status==ELITE_OK);
        test_payload_write(span.data,CH_SENTINEL,0,CH_SENTINEL,child_cfg.seed);
        OK(elite_write_abort(child_connection,&l));
        if((++count & UINT64_C(255))==0)child_send(CH_PROGRESS,count,0,count,0);
    }
}
static int worker_main(void)
{
    parent_process=0;deadline=test_now()+UINT64_C(90000000000);
    io_read(STDIN_FILENO,&child_cfg,sizeof(child_cfg));install_signals((int)child_cfg.restart);
    OK(elite_attach(&child_cfg.grant,&child_connection));
    child_send(CH_READY,(uint64_t)(uintptr_t)child_connection->mapping,child_cfg.grant.segment_bytes,(uint64_t)(unsigned long)getpid(),0);
    elite_lease held;elite_read_span held_span;int holds=0;
    for(;;){struct ch_command_record cmd;io_read(STDIN_FILENO,&cmd,sizeof(cmd));
        switch(cmd.command){
        case CH_RUN:child_run(&cmd);break;
        case CH_RANDOM_ABORT:child_random_abort();break;
        case CH_VICTIM:child_victim();break;
        case CH_SEED_ONE:child_commit(CH_SENTINEL);child_send(CH_SEEDED,CH_SENTINEL,0,0,0);break;
        case CH_HOLD:OK(elite_read_borrow(child_connection,&held,&held_span));holds=1;
            CHECK(test_payload_check(held_span.data,CH_SENTINEL,0,CH_SENTINEL,child_cfg.seed));
            child_send(CH_HELD,CH_SENTINEL,child_connection->block,(uint64_t)(uintptr_t)held_span.data,0);break;
        case CH_RELEASE:CHECK(holds);CHECK(test_payload_check(held_span.data,CH_SENTINEL,0,CH_SENTINEL,child_cfg.seed));
            OK(elite_read_release(child_connection,&held));holds=0;child_send(CH_RELEASED,CH_SENTINEL,0,0,0);break;
        case CH_EXIT:{CHECK(!holds);struct ch_event_record e;memset(&e,0,sizeof(e));
            OK(elite_detach(&child_connection,&e.cleanup));e.event=CH_DETACHED;e.worker_id=child_cfg.worker_id;
            e.sequence=++child_sequence;e.tick=test_now();io_write(STDOUT_FILENO,&e,sizeof(e));return 0;}
        default:CHECK(0);
        }
    }
}
static struct worker *spawn_worker(const char *exe,uint32_t id,uint32_t role)
{
    CHECK(worker_count<CH_MAX_WORKERS*2u);struct worker *w=&workers[worker_count++];memset(w,0,sizeof(*w));
    w->id=id;w->role=role;w->cut_block=CH_NONE;w->forbidden_block=CH_NONE;
    int a[2],b[2];CHECK(pipe(a)==0&&pipe(b)==0);
    for(unsigned i=0;i<2;++i){CHECK(fcntl(a[i],F_SETFD,FD_CLOEXEC)==0);CHECK(fcntl(b[i],F_SETFD,FD_CLOEXEC)==0);}
    posix_spawn_file_actions_t actions;CHECK(posix_spawn_file_actions_init(&actions)==0);
    CHECK(posix_spawn_file_actions_adddup2(&actions,a[0],STDIN_FILENO)==0);
    CHECK(posix_spawn_file_actions_adddup2(&actions,b[1],STDOUT_FILENO)==0);
    char *args[]={(char *)exe,(char *)"--worker",NULL};
    CHECK(posix_spawn(&w->pid,exe,&actions,NULL,args,environ)==0);
    CHECK(posix_spawn_file_actions_destroy(&actions)==0);CHECK(close(a[0])==0&&close(b[1])==0);
    w->input=a[1];w->output=b[0];return w;
}
static struct ch_event_record receive_event(struct worker *w)
{
    struct ch_event_record e;io_read(w->output,&e,sizeof(e));
    CHECK(e.worker_id==w->id&&e.sequence==w->last_sequence+1&&e.tick>=w->last_tick);
    w->last_sequence=e.sequence;w->last_tick=e.tick;w->received_tick=test_now();
    event_begin("worker_event");printf(",\"worker_id\":%u,\"pid\":%ld,\"kind\":%u,\"sequence\":%"PRIu64",\"tick\":%"PRIu64",\"values\":[",
        w->id,(long)w->pid,e.event,e.sequence,e.tick);
    for(unsigned i=0;i<8;++i){printf("%s%"PRIu64,i?",":"",e.data[i]);}
    printf("],\"received_tick\":%"PRIu64,w->received_tick);event_end();
    if(e.event==CH_CUT){w->cut_ticket=e.data[1];w->cut_block=e.data[2];}
    if(e.event==CH_DONE){
        CHECK(e.data[2]<=(CH_MAX_MESSAGES+7)/8);w->bitmap_bytes=(size_t)e.data[2];
        w->bitmap=calloc(w->bitmap_bytes? w->bitmap_bytes:1,1);CHECK(w->bitmap!=NULL);
        if(w->bitmap_bytes)io_read(w->output,w->bitmap,w->bitmap_bytes);
        w->completed=e.data[0];w->specials=e.data[1];w->usr=e.data[4];w->alrm=e.data[5];w->done=1;
        event_begin("membership");printf(",\"worker_id\":%u,\"hex\":\"",w->id);
        for(size_t i=0;i<w->bitmap_bytes;++i){printf("%02x",w->bitmap[i]);}
        printf("\"");event_end();
    }
    return e;
}
static void expect_event(struct worker *w,uint32_t kind)
{struct ch_event_record e=receive_event(w);CHECK(e.event==kind);}
static void command(struct worker *w,uint32_t kind,uint64_t first,uint64_t quota,uint64_t total)
{
    struct ch_command_record c;memset(&c,0,sizeof(c));c.command=kind;c.reserved=(uint32_t)(kind==CH_RUN&&cohort_retirement);c.first=first;c.quota=quota;c.total=total;c.forbidden_block=w->forbidden_block;
    io_write(w->input,&c,sizeof(c));event_begin("command");printf(",\"worker_id\":%u,\"command\":%u,\"first\":%"PRIu64",\"quota\":%"PRIu64",\"total\":%"PRIu64,w->id,kind,first,quota,total);printf(",\"allow_retirement\":%s",c.reserved?"true":"false");event_end();
}
static struct generation *create_generation(const char *exe,uint32_t mode,uint32_t p,uint32_t c,uint32_t cut,int read_victim,int restart)
{
    CHECK(generation_count<4&&p+c<=CH_MAX_WORKERS);size_t gi=generation_count++;
    struct generation *g=&generations[gi];memset(g,0,sizeof(*g));g->fd=-1;g->count=p+c;g->producers=p;
    elite_endpoint_definition defs[CH_MAX_WORKERS];test_definitions(defs,p,c);
    for(uint32_t i=0;i<p+c;++i)test_id(defs[i].process_incarnation_id,UINT64_C(100000)+(uint64_t)(gi*100+i));
    elite_config cfg=test_config(mode,p,c,CH_CAPACITY);OK(elite_create(authority,&cfg,defs,&g->object));OK(elite_object_activate(g->object));
    OK(elite_object_info(g->object,&g->info));
    for(uint32_t i=0;i<g->count;++i){
        struct worker *w=spawn_worker(exe,(uint32_t)(gi*CH_MAX_WORKERS)+i,i<p?ELITE_PRODUCER:ELITE_CONSUMER);g->workers[i]=w;
        struct ch_config cc;memset(&cc,0,sizeof(cc));cc.worker_id=w->id;cc.seed=CH_SEED+(uint64_t)gi;
        cc.cut=(cut&&i==(read_victim?p:0))?cut:0;cc.restart=(uint32_t)restart;cc.forbidden_block=CH_NONE;
        OK(elite_object_register_process(g->object,i,w->pid));OK(elite_object_grant(g->object,i,&cc.grant));
        if(i==0){memcpy(g->name,cc.grant.name,ELITE_NAME_BYTES);g->fd=shm_open(g->name,O_RDWR,0);CHECK(g->fd>=0);
            CHECK(fcntl(g->fd,F_SETFD,FD_CLOEXEC)==0);
            g->mapping=mmap(NULL,(size_t)g->info.segment_bytes,PROT_READ|PROT_WRITE,MAP_SHARED,g->fd,0);CHECK(g->mapping!=MAP_FAILED);
            event_begin("generation");printf(",\"generation_id\":%zu,\"name\":\"%s\",\"capacity\":%"PRIu64",\"mode\":%u,\"producers\":%u,\"consumers\":%u,\"bytes\":%"PRIu64",\"observer_address\":%"PRIu64,gi,g->name,g->info.capacity,mode,p,c,g->info.segment_bytes,(uint64_t)(uintptr_t)g->mapping);
            printf(",\"immutable_prefix_hex\":\"");
            for(size_t hb=0;hb<512;++hb){printf("%02x",((const unsigned char *)g->mapping)[hb]);}
            printf("\"");event_end();}
        io_write(w->input,&cc,sizeof(cc));expect_event(w,CH_READY);
    }
    return g;
}
static void send_signal(struct worker *w,int sig,const char *name)
{
    CHECK(!w->reaped&&w->pid>0);CHECK(kill(w->pid,sig)==0);
    event_begin("signal_sent");printf(",\"worker_id\":%u,\"signal\":\"%s\",\"signal_number\":%d",w->id,name,sig);event_end();
}
static void stopped(struct worker *w)
{
    int status=0;pid_t p;
    do{check_time();p=waitpid(w->pid,&status,WNOHANG|WUNTRACED);if(p==0)delay_ns(100000);}while(p==0||(p<0&&errno==EINTR));
    CHECK(p==w->pid&&WIFSTOPPED(status));
    event_begin("stopped_observed");printf(",\"worker_id\":%u,\"wait_status\":%d",w->id,status);event_end();
    int terminal=0;elite_result r=elite_authority_reap_child(authority,w->pid,&terminal);CHECK(r.status==ELITE_NOT_READY);
    event_begin("stop_not_death");printf(",\"worker_id\":%u,\"status\":%u",w->id,r.status);event_end();
}
static void fence_killed(struct worker *w)
{
    int status=0;elite_result r;
    do{check_time();r=elite_authority_reap_child(authority,w->pid,&status);if(r.status==ELITE_NOT_READY)delay_ns(100000);}while(r.status==ELITE_NOT_READY);
    CHECK(r.status==ELITE_OK&&WIFSIGNALED(status)&&WTERMSIG(status)==SIGKILL);w->reaped=1;
    event_begin("terminal_observed");printf(",\"worker_id\":%u,\"wait_status\":%d,\"signal\":\"SIGKILL\",\"status\":%u",w->id,status,r.status);event_end();
}
static void finish_worker(struct generation *g,struct worker *w)
{
    if(w->reaped){return;}
    command(w,CH_EXIT,0,0,0);
    struct ch_event_record e=receive_event(w);CHECK(e.event==CH_DETACHED);OK(elite_object_ack_cleanup(g->object,&e.cleanup));
    int status=0;pid_t p;do{p=waitpid(w->pid,&status,0);}while(p<0&&errno==EINTR);
    CHECK(p==w->pid&&WIFEXITED(status)&&WEXITSTATUS(status)==0);w->reaped=1;
    event_begin("clean_exit");printf(",\"worker_id\":%u,\"wait_status\":%d",w->id,status);event_end();
}
static void run_cohort(struct generation *g,struct worker *excluded,uint64_t total,int storm)
{
    uint32_t np=0,nc=0;
    for(uint32_t i=0;i<g->count;++i)if(g->workers[i]!=excluded){if(g->workers[i]->role==ELITE_PRODUCER)++np;else ++nc;}
    CHECK(np&&nc&&total%np==0);uint64_t first=0;
    for(uint32_t i=0;i<g->count;++i){struct worker *w=g->workers[i];if(w==excluded)continue;
        w->done=0;command(w,CH_RUN,first,w->role==ELITE_PRODUCER?total/np:0,total);if(w->role==ELITE_PRODUCER)first+=total/np;}
    uint32_t pdone=0,done=0;int drain=0,retired=0;uint64_t next_signal=test_now();unsigned rounds=0;
    while(done<np+nc){check_time();
        struct pollfd f[CH_MAX_WORKERS];for(uint32_t i=0;i<g->count;++i){f[i].fd=(g->workers[i]==excluded||g->workers[i]->done)?-1:g->workers[i]->output;f[i].events=POLLIN;f[i].revents=0;}
        int n=poll(f,g->count,1);if(n<0&&errno==EINTR)continue;CHECK(n>=0);
        for(uint32_t i=0;i<g->count;++i)if(f[i].revents){struct worker *w=g->workers[i];struct ch_event_record e=receive_event(w);
            CHECK(e.event==CH_STARTED||e.event==CH_PROGRESS||e.event==CH_DONE);
            if(e.event==CH_DONE){++done;if(w->role==ELITE_PRODUCER)++pdone;}}
        if(!cohort_retirement&&!drain&&pdone==np){for(uint32_t i=0;i<g->count;++i)if(g->workers[i]!=excluded&&g->workers[i]->role==ELITE_CONSUMER)command(g->workers[i],CH_DRAIN,0,0,0);drain=1;}
        if(cohort_retirement&&!retired&&test_now()>=overdue_deadline){
            CHECK(overdue_worker!=NULL);
            event_begin("heartbeat_overdue");printf(",\"worker_id\":%u,\"last_sequence\":%"PRIu64",\"last_received_tick\":%"PRIu64",\"deadline_tick\":%"PRIu64",\"death_inferred\":false,\"slot_reclaimed\":false",overdue_worker->id,overdue_worker->last_sequence,overdue_worker->received_tick,overdue_deadline);event_end();
            OK(elite_object_quarantine(g->object));event_begin("quarantined");printf(",\"generation_id\":0,\"tokens_retained_by_generation\":%u",CH_CAPACITY);event_end();retired=1;
        }
        if(storm&&test_now()>=next_signal){
            for(uint32_t i=0;i<g->count;++i)if(!g->workers[i]->done){send_signal(g->workers[i],SIGUSR1,"SIGUSR1");send_signal(g->workers[i],SIGALRM,"SIGALRM");}
            struct worker *w=g->workers[rounds%g->count];if(!w->done){send_signal(w,SIGSTOP,"SIGSTOP");stopped(w);delay_ns(200000);send_signal(w,SIGCONT,"SIGCONT");}
            ++rounds;next_signal=test_now()+UINT64_C(1000000);
        }
    }
    size_t bytes=(size_t)((total+7)/8);unsigned char *bits=calloc(bytes,1);CHECK(bits!=NULL);
    uint64_t sent=0,received=0,special=0;
    for(uint32_t i=0;i<g->count;++i){struct worker *w=g->workers[i];if(w==excluded)continue;
        if(w->role==ELITE_PRODUCER)sent+=w->completed;
        else{received+=w->completed;special+=w->specials;CHECK(w->bitmap_bytes==bytes);
            for(size_t b=0;b<bytes;++b){CHECK((bits[b]&w->bitmap[b])==0);bits[b]|=w->bitmap[b];}}
    }
    if(!cohort_retirement){
        for(uint64_t i=0;i<total;++i)CHECK((bits[i/8]&(1u<<(unsigned)(i%8)))!=0);
        CHECK(sent==total&&received==total);
    }else{CHECK(retired&&sent<=total&&received<=sent);}
    free(bits);
    event_begin("cohort_reconciled");printf(",\"generation_id\":%zu,\"sent\":%"PRIu64",\"received\":%"PRIu64",\"sentinel_received\":%"PRIu64",\"storm_rounds\":%u",(size_t)(g-generations),sent,received,special,rounds);printf(",\"retired_during_work\":%s",cohort_retirement?"true":"false");event_end();
}
static void print_array(const char *name,const uint64_t *values,size_t n)
{printf(",\"%s\":[",name);for(size_t i=0;i<n;++i)printf("%s%"PRIu64,i?",":"",values[i]);printf("]");}
static void snapshot(struct generation *g)
{
    for(uint32_t i=0;i<g->count;++i)CHECK(g->workers[i]->reaped);
    struct elite_immutable_header info;OK(elite_validate_prefix(g->mapping,512,g->info.segment_bytes,&info));
    CHECK(memcmp(&info,&g->info,sizeof(info))==0);
    uint64_t status[CH_CAPACITY],epoch[CH_CAPACITY],ids[CH_CAPACITY];
    struct elite_slot_descriptor *ds=(struct elite_slot_descriptor *)((unsigned char *)g->mapping+info.descriptors_offset);
    for(unsigned i=0;i<CH_CAPACITY;++i){status[i]=atomic_load_explicit(&ds[i].status_word,memory_order_acquire);epoch[i]=ds[i].epoch;ids[i]=ds[i].message_id;CHECK(el_zero(ds[i].reserved_028,88));}
    event_begin("snapshot");printf(",\"generation_id\":%zu,\"scope\":\"ALL_WORKERS_TERMINAL\",\"capacity\":%u,\"header_crc32\":%u",(size_t)(g-generations),CH_CAPACITY,info.header_crc32);
    printf(",\"immutable_prefix_hex\":\"");
    for(size_t hb=0;hb<512;++hb){printf("%02x",((const unsigned char *)g->mapping)[hb]);}
    printf("\"");
    struct elite_cell64 *gate=(struct elite_cell64 *)((unsigned char *)g->mapping+512);
    printf(",\"admission\":%"PRIu64",\"failure\":%"PRIu64,atomic_load_explicit(&gate->value,memory_order_acquire),atomic_load_explicit(&(gate+1)->value,memory_order_acquire));
    if(info.layout_profile==ELITE_NCQ){struct elite_mpmc_ncq_header *h=g->mapping;
        uint64_t cursors[]={atomic_load(&h->qf_head.value),atomic_load(&h->qf_tail.value),atomic_load(&h->qr_head.value),atomic_load(&h->qr_tail.value)};
        uint64_t f[CH_CAPACITY],r[CH_CAPACITY];struct elite_ncq_entry_cell *qf=(void *)((unsigned char *)g->mapping+info.qf_entries_offset),*qr=(void *)((unsigned char *)g->mapping+info.qr_entries_offset);
        for(unsigned i=0;i<CH_CAPACITY;++i){f[i]=atomic_load(&qf[i].cycle_index);r[i]=atomic_load(&qr[i].cycle_index);}
        print_array("cursors",cursors,4);print_array("qf_entries",f,CH_CAPACITY);print_array("qr_entries",r,CH_CAPACITY);
    }else{struct elite_spsc_ring_header *h=g->mapping;uint64_t a[]={atomic_load(&h->published.value),atomic_load(&h->reclaimed.value)};print_array("cursors",a,2);}
    print_array("status_words",status,CH_CAPACITY);print_array("epochs",epoch,CH_CAPACITY);print_array("message_ids",ids,CH_CAPACITY);event_end();
}
static void destroy_generation(struct generation *g)
{
    snapshot(g);CHECK(munmap(g->mapping,(size_t)g->info.segment_bytes)==0);g->mapping=NULL;CHECK(close(g->fd)==0);g->fd=-1;
    OK(elite_object_destroy(&g->object));int fd=shm_open(g->name,O_RDWR,0);CHECK(fd<0&&errno==ENOENT);
    event_begin("destroyed");printf(",\"generation_id\":%zu,\"name_absent\":true",(size_t)(g-generations));event_end();
    for(uint32_t i=0;i<g->count;++i){struct worker *w=g->workers[i];CHECK(close(w->input)==0&&close(w->output)==0);free(w->bitmap);w->bitmap=NULL;}
    g->name[0]='\0';
}
static uint32_t scenario_cut(const char *name)
{
#ifdef ELITE_TESTING
    if(strcmp(name,"write_claim")==0)return ELITE_HOOK_QF_CLAIM;
    if(strcmp(name,"write_reserved")==0)return ELITE_HOOK_RESERVED;
    if(strcmp(name,"write_committed")==0)return ELITE_HOOK_COMMITTED;
    if(strcmp(name,"write_published")==0)return ELITE_HOOK_QR_AFTER_INSTALL;
    if(strcmp(name,"read_claim")==0)return ELITE_HOOK_QR_CLAIM;
    if(strcmp(name,"read_returning")==0)return ELITE_HOOK_QF_BEFORE_INSTALL;
    if(strcmp(name,"read_returned")==0)return ELITE_HOOK_QF_AFTER_INSTALL;
    if(strcmp(name,"late_publication")==0)return ELITE_HOOK_QR_BEFORE_INSTALL;
#endif
    if(strcmp(name,"write_partial")==0||strcmp(name,"resume_write")==0||strcmp(name,"async_resume_write")==0)return CH_CUT_MID;
    if(strcmp(name,"read_held")==0||strcmp(name,"resume_read")==0)return CH_CUT_READ;
    return 0;
}
int main(int argc,char **argv)
{
    if(argc==2&&strcmp(argv[1],"--worker")==0)return worker_main();
    CHECK(argc==7);const char *scenario=argv[1];uint32_t mode=strcmp(argv[2],"spsc")==0?ELITE_SPSC:ELITE_NCQ;
    CHECK(mode==ELITE_SPSC||strcmp(argv[2],"ncq")==0);
    char *end=NULL;uint64_t count=strtoull(argv[3],&end,10);CHECK(end&&*end=='\0'&&count>0&&count<=CH_MAX_MESSAGES);
    uint64_t hold_ms=strtoull(argv[4],&end,10);CHECK(end&&*end=='\0'&&hold_ms>0&&hold_ms<=1000);
    unsigned long pv=strtoul(argv[5],&end,10);CHECK(end&&*end=='\0'&&pv>=1&&pv<=16);
    unsigned long cv=strtoul(argv[6],&end,10);CHECK(end&&*end=='\0'&&cv>=1&&cv<=16);
    parent_process=1;deadline=test_now()+UINT64_C(60000000000);CHECK(atexit(cleanup)==0);install_signals(0);
    struct rlimit core={0,0};CHECK(setrlimit(RLIMIT_CORE,&core)==0);
    int storm=strcmp(scenario,"storm")==0||strcmp(scenario,"storm_restart")==0;
    int asynchronous=strcmp(scenario,"async_resume_write")==0;
    int random_kill=strcmp(scenario,"random_abort_kill")==0;
    int resume=strncmp(scenario,"resume_",7)==0||strcmp(scenario,"late_publication")==0||asynchronous;
    int read_victim=strncmp(scenario,"read_",5)==0||strcmp(scenario,"resume_read")==0;
    uint32_t cut=scenario_cut(scenario);CHECK(storm||cut!=0||random_kill);
    if(mode==ELITE_SPSC){CHECK(pv==1&&cv==1);CHECK(storm||cut==CH_CUT_MID||cut==CH_CUT_READ);}
    uint32_t p=(uint32_t)pv,c=(uint32_t)cv;
    if(!storm&&mode==ELITE_NCQ){p=read_victim?2u:3u;c=read_victim?3u:2u;}
    struct timespec res;CHECK(clock_getres(CLOCK_MONOTONIC,&res)==0);
#ifdef __APPLE__
    const char *wait_status_abi = "darwin";
#else
    const char *wait_status_abi = "linux";
#endif
    event_begin("plan");printf(",\"wait_status_abi\":\"%s\"",wait_status_abi);printf(",\"schema\":\"elite-chaos-v1\",\"scenario\":\"%s\",\"mode\":%u,\"messages\":%"PRIu64",\"hold_ms\":%"PRIu64",\"producers\":%u,\"consumers\":%u,\"clock\":\"CLOCK_MONOTONIC\",\"timebase_numer\":1,\"timebase_denom\":1,\"reported_resolution_ns\":%"PRIu64",\"hooks\":%s",scenario,mode,count,hold_ms,p,c,(uint64_t)res.tv_sec*UINT64_C(1000000000)+(uint64_t)res.tv_nsec,
#ifdef ELITE_TESTING
        "true"
#else
        "false"
#endif
    );event_end();test_authority(&authority);
    struct generation *old=create_generation(argv[0],mode,p,c,cut,read_victim,strcmp(scenario,"storm_restart")==0);
    if(storm){run_cohort(old,NULL,count,1);for(uint32_t i=0;i<old->count;++i)finish_worker(old,old->workers[i]);destroy_generation(old);}
    else{
        struct worker *victim=old->workers[read_victim?p:0];
        if(read_victim){command(old->workers[0],CH_SEED_ONE,0,0,0);expect_event(old->workers[0],CH_SEEDED);}
        if(random_kill){
            command(victim,CH_RANDOM_ABORT,0,0,0);expect_event(victim,CH_PROGRESS);
            delay_ns((hold_ms*UINT64_C(7919))%UINT64_C(1000000));
            send_signal(victim,SIGKILL,"SIGKILL");simple_event("random_cut_interval_unknown");
        }else{
            command(victim,CH_VICTIM,0,0,0);expect_event(victim,CH_CUT);stopped(victim);
            int unavailable=strcmp(scenario,"write_published")!=0&&strcmp(scenario,"read_returned")!=0;
            if(unavailable)for(uint32_t wi=0;wi<old->count;++wi)old->workers[wi]->forbidden_block=victim->cut_block;
        }
        uint64_t last=victim->received_tick;
        if(!asynchronous){delay_ns(hold_ms*UINT64_C(1000000));
        event_begin("heartbeat_overdue");printf(",\"worker_id\":%u,\"last_sequence\":%"PRIu64",\"last_received_tick\":%"PRIu64",\"deadline_tick\":%"PRIu64",\"death_inferred\":false,\"slot_reclaimed\":false",victim->id,victim->last_sequence,last,last+hold_ms*UINT64_C(1000000));event_end();}
        if(asynchronous){cohort_retirement=1;overdue_worker=victim;overdue_deadline=last+hold_ms*UINT64_C(1000000);}
        if(!resume&&!random_kill)send_signal(victim,SIGKILL,"SIGKILL");
        if(mode==ELITE_NCQ)run_cohort(old,victim,count,0);
        else simple_event("spsc_old_service_unavailable_no_role_replacement");
        /* Notification deliberately deferred until after independent NCQ work. */
        if(!resume)fence_killed(victim);
        if(!asynchronous){OK(elite_object_quarantine(old->object));event_begin("quarantined");printf(",\"generation_id\":0,\"tokens_retained_by_generation\":%u",CH_CAPACITY);event_end();}
        cohort_retirement=0;
        if(resume){elite_result r=elite_object_destroy(&old->object);CHECK(r.status==ELITE_BUSY);event_begin("unfenced_destroy_blocked");printf(",\"status\":%u",r.status);event_end();}
        for(uint32_t i=0;i<old->count;++i)if(old->workers[i]!=victim)finish_worker(old,old->workers[i]);
        struct generation *next=create_generation(argv[0],mode,1,1,0,0,0);
        CHECK(strcmp(next->name,old->name)!=0&&next->mapping!=old->mapping);
        command(next->workers[0],CH_SEED_ONE,0,0,0);expect_event(next->workers[0],CH_SEEDED);
        command(next->workers[1],CH_HOLD,0,0,0);expect_event(next->workers[1],CH_HELD);
        simple_event("successor_holds_validated_sentinel");
        if(resume){send_signal(victim,SIGCONT,"SIGCONT");expect_event(victim,CH_VICTIM_DONE);finish_worker(old,victim);}
        command(next->workers[1],CH_RELEASE,0,0,0);expect_event(next->workers[1],CH_RELEASED);
        simple_event("successor_sentinel_unchanged");run_cohort(next,NULL,count,0);
        for(uint32_t i=0;i<next->count;++i)finish_worker(next,next->workers[i]);
        destroy_generation(old);destroy_generation(next);
    }
    OK(elite_authority_destroy(&authority));event_begin("complete");printf(",\"status\":\"PASS_WITHIN_SCOPE\",\"all_children_reaped\":true,\"all_owned_names_absent\":true");event_end();return 0;
}
