#ifndef ELITE_BENCH_COMMON_H
#define ELITE_BENCH_COMMON_H
#include "elite_ringbuffer.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/resource.h>

/* Private verification ABI: not part of LE128-V1. Same executable on both
 * ends of owned pipes. These native records are NOT an application wire ABI. */
#define BENCH_MAX_WORKERS 32u
#define BENCH_PATH 1024u
#define BENCH_MAX_ITEMS UINT64_C(100000000)
#define BENCH_SEED UINT64_C(0x5bb43c0039172ae1)
#define BENCH_WARM_SEED UINT64_C(0x261af491ff203043)
#define BENCH_REPLY_SEED UINT64_C(0xe414ca61853b92d7)
#define BENCH_TYPE 7u
#define BENCH_CTRL_BYTES 16384u
#define B_CHECK(x) do { if(!(x)) bench_fail(__FILE__,__LINE__,#x); } while(0)
#define B_OK(x) do { elite_result _b_result=(x); if(_b_result.status!=ELITE_OK) bench_elite_fail(__FILE__,__LINE__,#x,_b_result); } while(0)

struct bench_options {
    uint32_t mode, producers, consumers, trials;
    uint64_t count, warmup, capacity, calibration;
    unsigned timeout_seconds;
    int cpus[BENCH_MAX_WORKERS];
    uint32_t cpu_count;
    int affinity_tag, qos;
    char output[BENCH_PATH];
};
struct bench_clock { uint32_t numer, denom; uint64_t reported_resolution_ns; };
struct bench_placement {
    int32_t requested_cpu, affinity_error, verified_cpu, end_verified_cpu;
    int32_t affinity_tag, tag_set_error, tag_get_error, tag_readback;
    int32_t qos_requested, qos_error;
    uint32_t affinity_enforced;
};
struct bench_stats {
    uint64_t count, zeros, minimum_nonzero, q[6];
};
struct bench_cell {
    _Alignas(128) _Atomic uint32_t drain;
    unsigned char reserved[124];
};
struct bench_control { struct bench_cell cell[BENCH_MAX_WORKERS]; };
_Static_assert(sizeof(struct bench_control)<=BENCH_CTRL_BYTES,"control file extent");
_Static_assert(sizeof(struct bench_cell)==128,"bench cell isolation");
struct bench_config {
    elite_grant grant[2];
    uint32_t grants, index, producers, consumers;
    uint64_t count, warmup, calibration;
    int32_t requested_cpu, affinity_tag, qos;
    char control_path[BENCH_PATH];
    char directory[BENCH_PATH];
};
struct bench_result {
    uint32_t event, index;
    uint64_t count, start_tick, end_tick, empty_polls, bitmap_bytes;
    struct bench_clock clock;
    struct bench_placement placement;
    struct bench_stats latency, calibration_before, calibration_after;
    struct rusage usage_before, usage_after;
    elite_cleanup_receipt receipts[2];
};
struct bench_child { pid_t pid; int command_fd, result_fd; };
struct bench_cohort {
    elite_authority *authority[2];
    elite_object *object[2];
    elite_grant first[2];
    struct bench_child child[BENCH_MAX_WORKERS];
    struct bench_control *control;
    uint32_t workers, objects;
    uint64_t deadline_ns;
    char directory[BENCH_PATH], control_path[BENCH_PATH];
};

enum { B_READY=1, B_WARM_DONE=2, B_MEASURE_DONE=3, B_FINAL=4 };

_Noreturn void bench_fail(const char *file,int line,const char *message);
_Noreturn void bench_elite_fail(const char *file,int line,const char *operation,elite_result result);
void bench_process_init(const char *directory);
void bench_parse(int argc,char **argv,bool latency,struct bench_options *options);
void bench_path(char out[BENCH_PATH],const char *directory,const char *name);
void bench_mkdir(const char *path);
void bench_id(uint8_t id[16]);
void bench_write_all(int fd,const void *data,size_t bytes);
void bench_read_all(int fd,void *data,size_t bytes);
void bench_save(const char *directory,const char *name,const void *data,size_t bytes);
FILE *bench_json_open(const char *directory,const char *name);
void bench_json_close(FILE *file);
void bench_json_string(FILE *file,const char *text);
void bench_clock_json(FILE *file,const struct bench_clock *clock);
bool bench_cancelled(void);
void bench_clock_init(struct bench_clock *clock);
uint64_t bench_tick(void);
uint64_t bench_ordered_tick(void);
uint64_t bench_watch_ns(void);
long double bench_ns(const struct bench_clock *clock,uint64_t ticks);
uint64_t bench_ns_ceil(const struct bench_clock *clock,uint64_t ticks);
uint64_t bench_future_tick(const struct bench_clock *clock,uint64_t ns);
void bench_wait_start(uint64_t tick);
void bench_statistics(uint64_t *values,uint64_t count,struct bench_stats *stats);
void bench_calibrate(uint64_t *values,uint64_t count,struct bench_stats *stats);
void bench_placement_apply(int cpu,int tag,int qos,struct bench_placement *placement);
void bench_placement_finish(struct bench_placement *placement);
void bench_placement_json(FILE *f,const struct bench_placement *placement);
void bench_stats_json(FILE *f,const struct bench_stats *stats,const struct bench_clock *clock);
void bench_machine_json(const char *directory);
void bench_hardware_topology_json(FILE *file);
void bench_cohort_init(struct bench_cohort *cohort,const char *directory,uint32_t workers,unsigned timeout);
void bench_cohort_create_ex(struct bench_cohort *cohort,uint32_t which,uint32_t mode,uint32_t producers,uint32_t consumers,uint64_t capacity,uint32_t payload,uint32_t checksum,uint64_t limit);
void bench_cohort_create(struct bench_cohort *cohort,uint32_t which,uint32_t mode,uint32_t producers,uint32_t consumers,uint64_t capacity);
void bench_spawn(struct bench_cohort *cohort,uint32_t index,const char *executable);
void bench_issue(struct bench_cohort *cohort,uint32_t object,uint32_t endpoint,uint32_t child,elite_grant *grant);
void bench_get_event(struct bench_cohort *cohort,uint32_t index,uint32_t event,struct bench_result *result);
void bench_cohort_finish(struct bench_cohort *cohort);
void bench_reconcile(const elite_grant *grant,uint64_t publications,const char *directory,const char *name);
struct bench_control *bench_control_open(const char *path);
void bench_control_close(struct bench_control *control);
void bench_worker_command(uint64_t *command);
void bench_worker_event(struct bench_result *result,uint32_t event);
void bench_usage_json(FILE *f,const struct rusage *before,const struct rusage *after);
void bench_send(elite_connection *connection,uint64_t id,uint64_t producer,uint64_t sequence,uint64_t seed,uint64_t *polls);
void bench_receive_exact(elite_connection *connection,uint64_t id,uint64_t producer,uint64_t sequence,uint64_t seed,uint64_t *polls);
bool bench_payload_check(const void *data,uint64_t id,uint64_t producer,uint64_t sequence,uint64_t seed);
void bench_payload_write(void *data,uint64_t id,uint64_t producer,uint64_t sequence,uint64_t seed);
void bench_relax(void);
#endif
