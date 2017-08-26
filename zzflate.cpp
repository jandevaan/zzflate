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



const int DEFLATE = 8;


header getHeader()
{
	auto h = header{ { DEFLATE, 7 } };
	auto rem = (h.CMF * 0x100 + h.FLG) % 31;
	h.fieldsFLG.FCHECK = 31 - rem;
	return h;
}


void ZzFlateEncode(uint8_t *dest, unsigned long *destLen, const uint8_t *source, size_t sourceLen, int level)
{
	if (level < 0 || level >3)
	{
		*destLen = ~0ul;
		return;
	}

	auto state = std::make_unique<Encoder>(level, dest, *destLen);
	 
	auto header = getHeader();
	state->stream.WriteU8(header.CMF);
	state->stream.WriteU8(header.FLG);

	 
	state->AddData(source, source + sourceLen, true);
	auto adler = adler32x(1, source, sourceLen);

	state->stream.WriteBigEndianU32(adler);
	state->stream.Flush();

	*destLen = safecast(state->stream.byteswritten());
}

 

void ZzFlateEncode2(const uint8_t *source, size_t sourceLen, int level, std::function<bool(const bufferHelper&)> callback)
{
	if (level < 0 || level >3)
	{
		//*destLen = ~0ul;
		return;
	}

	auto state = std::make_unique<Encoder>(level);

	auto header = getHeader();
	state->stream.WriteU8(header.CMF);
	state->stream.WriteU8(header.FLG);
	 
	state->AddData(source, source + sourceLen,  true);
	auto adler = adler32x(1, source, sourceLen);
	state->stream.WriteBigEndianU32(adler);
	state->stream.Flush();

	for (auto& buf : state->stream.buffers)
	{
		callback(*buf);
	}
	 
}

int getPos(int totalLength, int n, int divides)
{
	int partSize = (totalLength + divides - 1) / divides;

	return std::min(n * partSize, totalLength);

}

void GzipEncode(uint8_t *dest, unsigned long *destLen, const uint8_t *source, size_t sourceLen, int level)
{
	if (level < 0 || level >3)
	{
		*destLen = ~0ul;
		return;
	}
	 
	auto state = std::make_unique<Encoder>(level, dest, *destLen);

	state->stream.WriteU8(0x1f); // ID1
	state->stream.WriteU8(0x8b); // ID2

	state->stream.WriteU8(0x08); // 8 = Deflate
	state->stream.WriteU8(0);

	state->stream.WriteU32(0); // time

	state->stream.WriteU8(0);
	state->stream.WriteU8(255); // OS
	 
	state->AddData(source, source + sourceLen, true);
	auto crc = crc32(source, sourceLen);
	state->stream.Flush();
// add CRC
	
	state->stream.WriteU32( crc);
	state->stream.WriteU32((uint32_t)sourceLen);

}

std::vector<int> partitionRange(size_t total, int count)
{
	auto results = std::vector<int>(count + 1);
	int step = (total + count - 1) / count;
	for (int i = 1; i < count; ++i)
	{
		results[i] = step * i;
	}
	results[count] = total;

	return results;
}

void ZzFlateEncodeThreaded(uint8_t *dest, unsigned long *destLen, const uint8_t *source, size_t sourceLen, int level)
{
	if (level < 0 || level >3)
	{
		*destLen = ~0ul;
		return;
	} 

	int partitions = std::thread::hardware_concurrency();

	auto sourcePartitions = partitionRange(sourceLen, partitions);
	auto destpartitions = partitionRange(*destLen, partitions);

	 
	auto doPacket = [level, source, sourcePartitions, dest, destpartitions](int n) -> int64_t
	{  
		auto encoder = std::make_unique<Encoder>(level, dest + destpartitions[n], safecast(destpartitions[n + 1] - destpartitions[n]));

		if (n == 0)
		{
			auto header = getHeader();
			encoder->stream.WriteU8(header.CMF);
			encoder->stream.WriteU8(header.FLG); 
		}
		
	 	auto srcStart = sourcePartitions[n];
		auto srcEnd = sourcePartitions[n + 1];
		  
		encoder->AddData(source + srcStart, source + srcEnd - 1, false);
		
		//byte align stream by writing 1 byte uncompressed 
		encoder->SetLevel(0);
		encoder->AddData(source + srcEnd - 1, source + srcEnd, n == sourcePartitions.size() -2);

		encoder->stream.Flush();

		return {   encoder->stream.byteswritten() };
	};

	auto futures = std::vector<std::future<int64_t>>( );

	for (int n = 1; n < partitions; ++n)
	{
		futures.push_back(std::async(doPacket, n));
	}
	auto resultA = doPacket(0);

	auto countSofar = resultA ;
 

	int n = 1;
	for (auto& f : futures)
	{
		auto count = f.get();
		memmove(dest + countSofar, dest + destpartitions[n], count);
	  	countSofar += count;
 		n++;
	} 

	auto adler = adler32x(1, source, sourceLen); 

	outputbitstream tempStream(dest + countSofar, safecast(destLen - countSofar));
	tempStream.WriteBigEndianU32(adler);
	tempStream.Flush();
	 
	*destLen = safecast(countSofar + 4);
}
