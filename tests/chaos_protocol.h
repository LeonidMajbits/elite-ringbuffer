#ifndef ELITE_CHAOS_PROTOCOL_H
#define ELITE_CHAOS_PROTOCOL_H
/* Verification control records are native same-executable pipe messages.
 * They are NOT wire-ABI additions and never carry an application payload. */
#include "elite_ringbuffer.h"
#include <stdint.h>
#define CH_MAX_WORKERS 32u
#define CH_CAPACITY 64u
#define CH_SENTINEL (UINT64_MAX-UINT64_C(7))
#define CH_SEED UINT64_C(0x197ac05e)
#define CH_NONE UINT64_MAX
#define CH_CUT_MID 100u
#define CH_CUT_READ 101u
#define CH_CUT_WRITE 102u
#define CH_MAX_MESSAGES UINT64_C(4000000)
enum ch_command { CH_RUN=1, CH_VICTIM, CH_SEED_ONE, CH_EXIT, CH_DRAIN,
    CH_HOLD, CH_RELEASE, CH_RANDOM_ABORT };
enum ch_event { CH_READY=1, CH_PROGRESS, CH_DONE, CH_CUT, CH_VICTIM_DONE,
    CH_SEEDED, CH_DETACHED, CH_HELD, CH_RELEASED, CH_STARTED };
struct ch_config { elite_grant grant; uint32_t worker_id; uint32_t cut;
    uint64_t seed; uint64_t forbidden_block; uint32_t restart; uint32_t reserved; };
struct ch_command_record { uint32_t command; uint32_t reserved;
    uint64_t first; uint64_t quota; uint64_t total; uint64_t forbidden_block; };
struct ch_event_record { uint32_t event; uint32_t worker_id;
    uint64_t sequence; uint64_t tick; uint64_t data[8]; elite_cleanup_receipt cleanup; };
#endif
