#ifndef _ZZGENERIC
#define _ZZGENERIC
#include <cstdint>

const uint64_t magic = 0x022fdd63cc95386d; // the 4061955.

const unsigned int magictable[64] =
{
	0,  1,  2, 53,  3,  7, 54, 27,
	4, 38, 41,  8, 34, 55, 48, 28,
	62,  5, 39, 46, 44, 42, 22,  9,
	24, 35, 59, 56, 49, 18, 29, 11,
	63, 52,  6, 26, 37, 40, 33, 47,
	61, 45, 43, 21, 23, 58, 17, 10,
	51, 25, 36, 32, 60, 20, 57, 16,
	50, 31, 19, 15, 30, 14, 13, 12,
};

unsigned int bitScanForward(uint64_t b) {
	return magictable[(uint64_t(b&-int64_t(b))*magic) >> 58];
}



__forceinline int ZeroCount(uint64_t delta)
{ 
	return bitScanForward(delta) >> 3; 
}

#endif