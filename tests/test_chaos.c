/* Owned-child failure tests; never signal an unrelated process. */
#include "test_support.h"
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
extern char **environ;
struct child_config {elite_grant grant;uint32_t scenario;};
static void report_pause(void)
{unsigned char reached=1;test_write_all(STDOUT_FILENO,&reached,1);CHECK(raise(SIGSTOP)==0);}
static void post_publish_pause(void *arg,uint32_t point,uint64_t t,uint64_t b)
{(void)arg;(void)t;(void)b;if(point==ELITE_HOOK_QR_AFTER_INSTALL)report_pause();}
static int run_child(void)
{
    (void)alarm(120);struct child_config cfg;test_read_all(STDIN_FILENO,&cfg,sizeof(cfg));
    elite_connection *c=NULL;OK(elite_attach(&cfg.grant,&c));
    if(cfg.scenario==1)OK(elite_test_set_hook(c,post_publish_pause,NULL));
    elite_lease l;elite_write_span w;OK(elite_write_reserve(c,&l,&w));
    uint64_t *words=w.data;words[0]=777;words[1]=~UINT64_C(777);words[2]=0;words[3]=777;
    if(cfg.scenario!=1)report_pause(); /* Exactly mid-write, before COMMITTED. */
    test_payload_write(w.data,777,0,777,29);
    elite_result r=elite_write_commit(c,&l,64,7,777);
    if(cfg.scenario==2){CHECK(r.status==ELITE_RETIRED&&r.outcome==ELITE_RETAINED);OK(elite_abandon_retained(c,&l));}
    else CHECK(r.status==ELITE_OK&&r.outcome==ELITE_PUBLISHED);
    elite_cleanup_receipt receipt;OK(elite_detach(&c,&receipt));test_write_all(STDOUT_FILENO,&receipt,sizeof(receipt));return 0;
}
static void send_message(elite_connection *p,uint64_t id)
{
    elite_lease l;elite_write_span w;OK(elite_write_reserve(p,&l,&w));test_payload_write(w.data,id,0,id,29);OK(elite_write_commit(p,&l,64,7,id));
}
static void receive_message(elite_connection *c,uint64_t id)
{
    elite_lease l;elite_read_span r;OK(elite_read_borrow(c,&l,&r));CHECK(r.message_id==id&&test_payload_check(r.data,id,0,id,29));OK(elite_read_release(c,&l));
}
static void scenario(const char *exe,uint32_t which)
{
    elite_authority *a=NULL;elite_object *old=NULL,*next=NULL;test_authority(&a);
    elite_endpoint_definition d[3];test_definitions(d,2,1);test_id(d[0].process_incarnation_id,7777);
    elite_config cfg=test_config(ELITE_NCQ,2,1,32);OK(elite_create(a,&cfg,d,&old));OK(elite_object_activate(old));
    int input[2],output[2];CHECK(pipe(input)==0&&pipe(output)==0);
    for(unsigned i=0;i<2;++i){CHECK(fcntl(input[i],F_SETFD,FD_CLOEXEC)==0);CHECK(fcntl(output[i],F_SETFD,FD_CLOEXEC)==0);}
    posix_spawn_file_actions_t actions;CHECK(posix_spawn_file_actions_init(&actions)==0);
    CHECK(posix_spawn_file_actions_adddup2(&actions,input[0],STDIN_FILENO)==0);
    CHECK(posix_spawn_file_actions_adddup2(&actions,output[1],STDOUT_FILENO)==0);
    char *args[]={(char *)exe,(char *)"child",NULL};pid_t pid=0;
    CHECK(posix_spawn(&pid,exe,&actions,NULL,args,environ)==0);CHECK(posix_spawn_file_actions_destroy(&actions)==0);
    CHECK(close(input[0])==0&&close(output[1])==0);
    struct child_config child;memset(&child,0,sizeof(child));child.scenario=which;
    OK(elite_object_register_process(old,0,pid));OK(elite_object_grant(old,0,&child.grant));
    elite_connection *p=NULL,*c=NULL;elite_grant pg,cg;test_attach_self(old,1,&p,&pg);test_attach_self(old,2,&c,&cg);
    test_write_all(input[1],&child,sizeof(child));unsigned char reached=0;test_read_all(output[0],&reached,1);CHECK(reached==1);
    int stopped=0;CHECK(waitpid(pid,&stopped,WUNTRACED)==pid&&WIFSTOPPED(stopped));
    int terminal=0;CHECK(elite_authority_reap_child(a,pid,&terminal).status==ELITE_NOT_READY);
    if(which==1)receive_message(c,777);
    for(uint64_t i=0;i<10000;++i){send_message(p,1000+i);receive_message(c,1000+i);}
    if(which!=2) {
        CHECK(kill(pid,SIGKILL)==0);
        elite_result r;
        do{r=elite_authority_reap_child(a,pid,&terminal);if(r.status==ELITE_NOT_READY)(void)sched_yield();}while(r.status==ELITE_NOT_READY);
        CHECK(r.status==ELITE_OK&&WIFSIGNALED(terminal)&&WTERMSIG(terminal)==SIGKILL);
        test_detach(old,&p);test_detach(old,&c);OK(elite_object_destroy(&old));
        printf("PASS native_SIGKILL cut=%s healthy_lifecycles_before_fencing=10000 no_token_theft=yes\n",which==0?"MID_WRITE":"POST_PUBLICATION_PRE_TAIL");
    } else {
        OK(elite_object_quarantine(old));
        test_detach(old,&p);test_detach(old,&c);
        CHECK(elite_object_destroy(&old).status==ELITE_BUSY); /* paused child retains a mapping */
        elite_endpoint_definition nd[2];test_definitions(nd,1,1);cfg=test_config(ELITE_SPSC,1,1,32);
        OK(elite_create(a,&cfg,nd,&next));OK(elite_object_activate(next));
        elite_connection *np=NULL,*nc=NULL;elite_grant ngp,ngc;test_attach_self(next,0,&np,&ngp);test_attach_self(next,1,&nc,&ngc);
        CHECK(memcmp(child.grant.session_id,ngp.session_id,16)!=0);
        send_message(np,999999);elite_lease lease;elite_read_span span;OK(elite_read_borrow(nc,&lease,&span));
        CHECK(kill(pid,SIGCONT)==0);elite_cleanup_receipt receipt;test_read_all(output[0],&receipt,sizeof(receipt));
        OK(elite_object_ack_cleanup(old,&receipt));CHECK(waitpid(pid,&terminal,0)==pid&&WIFEXITED(terminal)&&WEXITSTATUS(terminal)==0);
        CHECK(span.message_id==999999&&test_payload_check(span.data,999999,0,999999,29));OK(elite_read_release(nc,&lease));
        test_detach(next,&np);test_detach(next,&nc);OK(elite_object_destroy(&old));OK(elite_object_destroy(&next));
        puts("PASS native_SIGSTOP_successor_SIGCONT old_writer_did_not_modify_successor");
    }
    CHECK(close(input[1])==0&&close(output[0])==0);OK(elite_authority_destroy(&a));
}
int main(int argc,char **argv)
{
    (void)alarm(120);if(argc==2&&strcmp(argv[1],"child")==0)return run_child();
    scenario(argv[0],0);scenario(argv[0],1);scenario(argv[0],2);puts("PASS native_chaos_scenarios=3");return 0;
}
