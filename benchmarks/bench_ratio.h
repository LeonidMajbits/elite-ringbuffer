#ifndef ELITE_BENCH_RATIO_H
#define ELITE_BENCH_RATIO_H
#include <stdio.h>
#include <stdint.h>
/* Exact 64x64 / 64x64 decimal export with 80 fractional digits. No compiler
 * extended integer or floating type is needed. Not a timer-resolution claim. */
int bench_json_ratio(FILE *out,uint64_t n1,uint64_t n2,uint64_t d1,uint64_t d2);
#endif
