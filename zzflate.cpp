#include <cassert> 
#include <gtest/gtest.h> 
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
	auto h = header{ { DEFLATE, 7 } };
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
	case Deflate: return {};
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

 

std::vector<int> divideInRanges(size_t total, int count)
{
	auto results = std::vector<int>(count + 1);
	auto step = (total + count - 1) / count;
	for (int i = 1; i < count; ++i)
	{
		results[i] = step * i;
	}
	results[count] = total;

	return results;
}


int64_t DeflateImpl(uint8_t *dest, unsigned long destLen, const uint8_t *source, size_t sourceLen, const Config* config)
{
	auto level = config->level;
	if (!config->threaded)
	{
		auto state = std::make_unique<Encoder>(config->level, dest, destLen);

		state->AddData(source, source + sourceLen, true);
		state->stream.Flush();
		return state->stream.byteswritten();
	}
	int taskCount = std::thread::hardware_concurrency();

	auto srcRanges = divideInRanges(sourceLen, taskCount);
	auto dstRanges = divideInRanges(destLen, taskCount);

	auto doPacket = [level, source, srcRanges, dest, dstRanges](int n) -> int64_t
	{
		auto encoder = std::make_unique<Encoder>(level, dest + dstRanges[n], safecast(dstRanges[n + 1] - dstRanges[n]));
		 
		auto srcStart = source + srcRanges[n];
		auto srcEnd = source + srcRanges[n + 1];

		encoder->AddData(srcStart, srcEnd - 1, false);

		//byte align stream by writing 1 byte uncompressed 
		encoder->SetLevel(0);
		encoder->AddData(srcEnd - 1, srcEnd, n == srcRanges.size() - 2);

		encoder->stream.Flush();

		return encoder->stream.byteswritten();
	};

	auto futures = std::vector<std::future<int64_t>>();

	for (int n = 1; n < taskCount; ++n)
	{
		futures.push_back(std::async(doPacket, n));
	}
	
	auto resultA = doPacket(0);

	auto countSofar = resultA;
	 
	int n = 1;
	for (auto& f : futures)
	{
		auto count = f.get();
		memmove(dest + countSofar, dest + dstRanges[n], count);
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

 

int AppendChecksum(const Config* cfg, const uint8_t * source, const size_t &sourceLen, outputbitstream &tempStream)
{
	switch (cfg->format)
	{
	case Zlib:
	{
		auto adler = adler32x(1, source, sourceLen);
		tempStream.WriteBigEndianU32(adler);
		return 4;
	}
	case Gzip:
	{
		auto crc = crc32(source, sourceLen);
		tempStream.WriteU32(crc);
		tempStream.WriteU32((uint32_t)sourceLen);
		return 8;
	}
	case Deflate:
		return 0;
	}

}




bool ZzFlateEncode2(const uint8_t *source, size_t sourceLen, const Config* config, std::function<bool(const bufferHelper&)> callback)
{  
	auto level = config->level;

	if (level < 0 || level >3)
 		return false;
 
	auto state = std::make_unique<Encoder>(level);
	 
	auto header = selectHeader(config);
	for (auto b : header)
	{
		state->stream.WriteU8(b);
	}
	state->AddData(source, source + sourceLen, true);
	
	AppendChecksum(config, source, sourceLen, state->stream);

	state->stream.Flush();

	for (auto& buf : state->stream.buffers)
	{
		callback(*buf);
	}

}


void ZzFlateEncode(uint8_t *dest, unsigned long *destLen, const uint8_t *source, size_t sourceLen, const Config* config)
{
	auto level = config->level;

	auto hLen = WriteHeader(dest, *destLen, config);
	if (level < 0 || level >3 || hLen < 0)
	{
		*destLen = ~0ul;
		return;
	}
	 

	auto countSofar = DeflateImpl(dest + hLen, *destLen - hLen, source, sourceLen, config) + hLen;
	 
	outputbitstream tempStream(dest + countSofar, safecast(*destLen - countSofar));

	int len = AppendChecksum(config, source, sourceLen, tempStream);
	
	
	tempStream.Flush();

	*destLen = safecast(countSofar + len);
}


 