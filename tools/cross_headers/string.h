/* Declaration-only shim for assembly generation of the ACTUAL queue files.
 * Does not implement or link libc and does not certify a Darwin SDK build. */
#ifndef ELITE_ASM_SHIM_STRING_H
#define ELITE_ASM_SHIM_STRING_H
#include <stddef.h>
void *memcpy(void *, const void *, size_t);
void *memset(void *, int, size_t);
int memcmp(const void *, const void *, size_t);
#endif
