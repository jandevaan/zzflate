#pragma once


void buildLengthLookup();

void ZzFlateEncode(unsigned char *dest, unsigned long *destLen, const unsigned char *source, size_t sourceLen, int level);
