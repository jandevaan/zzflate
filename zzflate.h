#ifndef _ZZFLATE
#define _ZZFLATE

#include "outputbitstream.h"

void buildLengthLookup();

void ZzFlateEncode(unsigned char *dest, unsigned long *destLen, const unsigned char *source, size_t sourceLen, int level);
void ZzFlateEncode2(const unsigned char *source, size_t sourceLen, int level, std::function<bool(const bufferHelper&)> callback);


#endif
