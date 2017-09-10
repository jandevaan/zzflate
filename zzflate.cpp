#include <cassert> 
#include <memory>
#include <thread>
#include <future>

#include "crc.h"
#include "encoder.h"
#include "zzflate.h"
 
struct header
{
	union
	{
		struct { uint8_t CM : 4, CINFO : 4; } fieldsCMF;
		uint8_t CMF;
	};
	union
	{
		struct { uint8_t FCHECK : 4, FDICT : 1, FLEVEL : 3; } fieldsFLG;
		uint8_t FLG;
	};
};


void StaticInit()
{
	Encoder::buildLengthLookup();
}
 
const unsigned char DEFLATE = 8;

std::vector<uint8_t> gzipHeader = { 0x1f, 0x8b, DEFLATE, 0, 0,0,0,0, 0, 0xFF };

std::vector<uint8_t> getHeader( )
{ 
	auto h = header{  DEFLATE, 7 };
	auto rem = (h.CMF * 0x100 + h.FLG) % 31;
	h.fieldsFLG.FCHECK = 31 - rem;
	return { h.CMF, h.FLG };
}


std::vector<uint8_t> selectHeader(const Config* config)
{
	switch (config->format)
	{
	case Zlib: return getHeader();
	case Gzip: return gzipHeader;
	case Deflate:
	default: return {};
	}
}


int64_t WriteHeader(uint8_t *dest, unsigned long destLen, const Config* config)
{
	auto header = selectHeader(config);
	if (destLen < header.size())
		return -1; // force error

	for (int i = 0; i < header.size(); i++)
	{
		dest[i] = header[i];
	}

	return header.size();
}

 

std::vector<size_t> divideInRanges(size_t total, int count)
{
	auto results = std::vector<size_t>(count + 1);
	auto step = (total + count - 1) / count;
	for (int i = 1; i < count; ++i)
	{
		results[i] = step * i;
	}
	results[count] = total;

	return results;
}
 

int64_t WriteDeflateStream(uint8_t *dest, unsigned long destLen, const uint8_t *source, size_t sourceLen, const Config* config, std::function<void(Encoder*)> callback = nullptr)
{
	auto level = config->level;
	if (!config->threaded || sourceLen < 100 * std::thread::hardware_concurrency())
	 
	{
		auto encoder = std::make_unique<Encoder>(config->level, dest, destLen);
		encoder->AddData(source, source + sourceLen, true);
		encoder->stream.Flush();
		if (callback != nullptr)
		{
			callback(&*encoder);
		}

		return encoder->stream.byteswritten();
	}
	
	auto taskCount = int(std::thread::hardware_concurrency());
	auto srcRanges = divideInRanges(sourceLen, taskCount);
	auto dstRanges = divideInRanges(destLen, taskCount);

	auto doPacket = [level, source, srcRanges, dest, dstRanges](int n) -> std::unique_ptr<Encoder>
	{
		auto encoder = std::make_unique<Encoder>(level, dest + dstRanges[n], safecast(dstRanges[n + 1] - dstRanges[n]));
		 
		auto srcStart = source + srcRanges[n];
		auto srcEnd = source + srcRanges[n + 1];

		bool final = n == srcRanges.size() - 2;
		 
		if (final)		
		{
			encoder->AddData(srcStart, srcEnd, final);
		}
		else
		{		 
			encoder->AddData(srcStart, srcEnd - 1, false);

			//byte align stream by writing 1 byte uncompressed 
			encoder->SetLevel(0);
			encoder->AddData(srcEnd - 1, srcEnd, final);
		}
		encoder->stream.Flush();
		//std::cout << "Bytes Written: " << encoder->stream.byteswritten() << "\r\n";
		return encoder;
	};

	auto futures = std::vector<std::future<std::unique_ptr<Encoder>>>();

	for (int n = 0; n < taskCount; ++n)
	{
		futures.push_back(std::async(n== 0 ? std::launch::deferred : std::launch::async, doPacket, n));
	}
	  
	uint64_t countSofar = 0;
	int32_t n = 0;
	for (auto& f : futures)
	{
		auto encoder = f.get();
		if (callback != nullptr)
		{
			callback(&*encoder);
		} 

		if (dest == nullptr)
			continue;

		auto count = encoder->stream.byteswritten();
		if (dstRanges[n] != countSofar)
		{
			memmove(dest + countSofar, dest + dstRanges[n], count);
		}
		countSofar += count;
		n++;
	}
	return countSofar;
 }

 

int getPos(int totalLength, int n, int divides)
{
	int partSize = (totalLength + divides - 1) / divides;

	return std::min(n * partSize, totalLength);

}

 

int AppendChecksum(const Config* cfg, const uint8_t * source, const size_t &sourceLen, outputbitstream &stream)
{
	switch (cfg->format)
	{
	case Zlib:
	{
		auto adler = adler32x(1, source, sourceLen);
		stream.WriteBigEndianU32(adler);
		return 4;
	}
	case Gzip:
	{
		auto crc = crc32(source, sourceLen);
		stream.WriteU32(crc);
		stream.WriteU32((uint32_t)sourceLen);
		return 8;
	}
	case Deflate:
	default:
		return 0;
	}

}




void ZzFlateEncodeToCallback(const uint8_t *source, size_t sourceLen, const Config* config, std::function<bool(const uint8_t*, int32_t)> callback)
{  
	auto level = config->level;

	if (level < 0 || level >3)
		return;
  
	auto header = selectHeader(config);
	callback(&header[0], header.size());

	WriteDeflateStream(nullptr, 0, source, sourceLen, config, [&callback] (auto e) ->  auto 
	{ 
		auto& stream = e->stream;
		for (int i = 0; i < stream.buffers.size(); i++)
		{
			bufferHelper*  h = stream.buffers[i].get();
			callback(h->buffer, h->bytesStored);
		}	
	});

	header.resize(4);
	outputbitstream tempStream(&header[0], header.size());	 
  	AppendChecksum(config, source, sourceLen, tempStream);
	tempStream.Flush();
	callback(&header[0], tempStream.byteswritten()); 
}


void ZzFlateEncode(uint8_t *dest, unsigned long *destLen, const uint8_t *source, size_t sourceLen, const Config* config)
{
	auto level = config->level;

	int64_t countSoFar = WriteHeader(dest, *destLen, config);
	if (level < 0 || level >3 || countSoFar < 0)
	{
		*destLen = ~0ul;
		return;
	}
	  
	countSoFar += WriteDeflateStream(dest + countSoFar, *destLen - countSoFar, source, sourceLen, config);
	 
	outputbitstream tempStream(dest + countSoFar, safecast(*destLen - countSoFar));

	countSoFar += AppendChecksum(config, source, sourceLen, tempStream);
	
	
	tempStream.Flush();

	*destLen = safecast(countSoFar);
}


 