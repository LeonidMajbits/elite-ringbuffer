#ifndef ELITE_PMC_H
#define ELITE_PMC_H
/* Optional diagnostics only; not part of LE128-V1 or the ring's hot path.
 * A context is owned by the creating thread. No sampling interrupt is enabled.
 * Unsupported/denied hardware is data, not a fabricated zero count. */
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define ELITE_PMC_GROUPS 3u
#define ELITE_PMC_EVENTS 6u
#define ELITE_PMC_FRAME_WORDS 7u

typedef enum elite_pmc_status {
    ELITE_PMC_NOT_REQUESTED = 0, ELITE_PMC_READY, ELITE_PMC_OK,
    ELITE_PMC_MULTIPLEXED, ELITE_PMC_RESTRICTED_PARANOID,
    ELITE_PMC_RESTRICTED_ACCESS, ELITE_PMC_UNSUPPORTED_EVENT,
    ELITE_PMC_UNSUPPORTED_PLATFORM, ELITE_PMC_OPEN_ERROR,
    ELITE_PMC_START_ERROR, ELITE_PMC_STOP_ERROR, ELITE_PMC_READ_ERROR,
    ELITE_PMC_NEVER_SCHEDULED, ELITE_PMC_INVALID_READING
} elite_pmc_status;

typedef struct elite_pmc_group_result {
    elite_pmc_status status;
    int os_error;
    uint32_t type[2];
    uint64_t config[2], ids[2];
    uint64_t before[ELITE_PMC_FRAME_WORDS], after[ELITE_PMC_FRAME_WORDS];
    uint64_t raw_delta[2], time_enabled_ns, time_running_ns;
    bool before_valid, after_valid;
} elite_pmc_group_result;

typedef struct elite_pmc_result {
    bool requested;
    int paranoid; /* -999 means unavailable, not a permissive setting. */
    uint64_t owner_thread_id;
    elite_pmc_group_result groups[ELITE_PMC_GROUPS];
} elite_pmc_result;

typedef struct elite_pmc_context elite_pmc_context;
/* Allocates only during setup. All denied groups remain in the result. */
elite_pmc_context *elite_pmc_create(bool enabled);
/* One start/stop per context. Wrong thread or order returns -1 / EINVAL. */
int elite_pmc_start(elite_pmc_context *context);
int elite_pmc_stop(elite_pmc_context *context);
const elite_pmc_result *elite_pmc_get_result(const elite_pmc_context *context);
void elite_pmc_destroy(elite_pmc_context *context);
const char *elite_pmc_status_name(elite_pmc_status status);
const char *elite_pmc_event_name(unsigned index);
/* Pure, tested frame decoder. Does not infer support from a zero count. */
int elite_pmc_decode_group(elite_pmc_group_result *group);
elite_pmc_status elite_pmc_classify_open_error(int error_number, int paranoid);

typedef struct elite_cpu_snapshot {
    int error;
    uint64_t user_us, system_us;
    int64_t minor_faults, major_faults, voluntary_switches, involuntary_switches;
    int32_t instant_usage, instant_usage_scale; /* Darwin THREAD_BASIC_INFO only. */
} elite_cpu_snapshot;
int elite_cpu_thread_snapshot(elite_cpu_snapshot *snapshot);
int elite_cpu_process_snapshot(elite_cpu_snapshot *snapshot);

/* Apple OS-accounted process cycles/instructions, NEVER per-worker counts.
 * These snapshots cover the whole process envelope, including controller work. */
typedef struct elite_process_counts {
    int error;
    uint64_t cycles, instructions;
} elite_process_counts;
int elite_pmc_process_counts(elite_process_counts *counts);

/* CNTVCT is a system timer, NOT a retired/core-cycle PMC. Raw instructions
 * must be preflighted in a disposable child before enabling in live workers.
 * On unsupported targets these calls return -1; no fake 24 MHz is supplied. */
typedef struct elite_virtual_sample {
    uint64_t mach_before, count, mach_after;
} elite_virtual_sample;
int elite_pmc_virtual_read(elite_virtual_sample *sample);
int elite_pmc_virtual_frequency(uint64_t *cntfrq_hz);
int elite_pmc_sysctl_tbfrequency(uint64_t *frequency_hz);
#ifdef __cplusplus
}
#endif
#endif
