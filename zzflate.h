#ifndef _ZZFLATE
#define _ZZFLATE

#include "outputbitstream.h"

void StaticInit();

void ZzFlateEncode(uint8_t *dest, unsigned long *destLen, const uint8_t *source, size_t sourceLen, int level);
void ZzFlateEncodeThreaded(uint8_t *dest, unsigned long *destLen, const uint8_t *source, size_t sourceLen, int level);

void ZzFlateEncode2(const uint8_t *source, size_t sourceLen, int level, std::function<bool(const bufferHelper&)> callback);
void GzipEncode(uint8_t *dest, unsigned long *destLen, const uint8_t *source, size_t sourceLen, int level);



#endif
