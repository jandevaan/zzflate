#ifndef _ZZVISUALC
#define _ZZVISUALC

#include <intrin.h>
 
typedef uint64_t compareType;

__forceinline int ZeroCount(uint64_t delta)
{
	unsigned long index;
	_BitScanForward64(&index, delta);
	return index >> 3;
}

#endif
