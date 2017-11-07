#ifndef _ZZFLATE
#define _ZZFLATE

#include <stdint.h>
#include <memory>
#include <functional>
  
enum Format {Zlib, Gzip, Deflate};

struct Config
{
	Format format;
	uint8_t level;
	bool threaded;
};


 
void StaticInit();
 
void ZzFlateEncode(uint8_t *dest, size_t*destLen, const uint8_t *source, size_t sourceLen, const Config* config);
 
void ZzFlateEncodeToCallback(const uint8_t *source, size_t sourceLen, const Config* config, std::function<bool(const uint8_t*, size_t)> callback);
 

#endif
