#ifndef ZZ_CRC
#define ZZ_CRC

#include <stdint.h>
 
uint32_t crc32(const uint8_t* buffer, int length, uint32_t startValue = 0);

#endif