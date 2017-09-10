#ifndef _ZZFLATE
#define _ZZFLATE


#include <cstdint>
 
#include <functional>
  
enum Format {Zlib, Gzip, Deflate};

struct Config
{
	Format format;
	uint8_t level;
	bool threaded;
};


 
void StaticInit();
 
void ZzFlateEncode(uint8_t *dest, unsigned long *destLen, const uint8_t *source, size_t sourceLen, const Config* config);
 
void ZzFlateEncodeToCallback(const uint8_t *source, size_t sourceLen, const Config* config, std::function<bool(const uint8_t*, int32_t)> callback);
 

#endif
