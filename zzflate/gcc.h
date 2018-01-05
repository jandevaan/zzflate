#ifndef _ZZGCC
#define _ZZGCC
#include <stdint.h>

#define ZZINLINE __attribute__((always_inline)) inline

typedef uint64_t compareType;


ZZINLINE int ZeroCount(uint64_t delta)
{
	return __builtin_ctzl(delta) >> 3;
}

#endif
