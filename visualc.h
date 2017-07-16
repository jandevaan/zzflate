#ifndef _VISUALC
#define _VISUALC

#include <intrin.h>
 
__forceinline int ZeroCount(unsigned long long delta)
{
	unsigned long index;
	_BitScanForward64(&index, delta);
	return index >> 3;
}

#endif
