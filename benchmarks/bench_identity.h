#ifndef ELITE_BENCH_IDENTITY_H
#define ELITE_BENCH_IDENTITY_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
/* Standalone, byte-oriented SHA-256 identity; not an authentication protocol. */
int bench_sha256_file(const char *path, char hex[65], uint64_t *bytes);
void bench_sha256_bytes(const void *data, size_t length, char hex[65]);
int bench_executable_path(char *buffer, size_t capacity);
void bench_build_identity_json(FILE *file);
#endif
