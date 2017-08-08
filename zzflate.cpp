#include <cassert> 
#include <gtest/gtest.h> 
#include <memory>
#include <thread>
#include <future>

  
#include "encoderstate.h"

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
	EncoderState::buildLengthLookup();
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

	auto state = std::make_unique<EncoderState>(level, dest, *destLen);
	 
	auto header = getHeader();
	state->stream.WriteU8(header.CMF);
	state->stream.WriteU8(header.FLG);

	uint32_t adler = 1; 
	
	state->AddData(source, source + sourceLen, adler, true);

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

	auto state = std::make_unique<EncoderState>(level);

	auto header = getHeader();
	state->stream.WriteU8(header.CMF);
	state->stream.WriteU8(header.FLG);

	uint32_t adler = 1;

	state->AddData(source, source + sourceLen, adler, true);

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



void ZzFlateEncodeThreaded(uint8_t *dest, unsigned long *destLen, const uint8_t *source, size_t sourceLen, int level)
{
//	int threadCount = std::thread::hardware_concurrency();

	if (level < 0 || level >3)
	{
		*destLen = ~0ul;
		return;
	}
	int divides = 2;

	int outputPartition = (*destLen + divides - 1) / divides;

	struct results { int64_t Count; uint32_t Adler; };
	auto doPacket = [level, source, sourceLen, dest, destLen, divides, outputPartition](int n)->results
	
	{ 
		auto dstStart = getPos(*destLen, n, divides);
		auto dstEnd = getPos(*destLen, n + 1, divides);
		 
		auto state = std::make_unique<EncoderState>(level, dest + dstStart, safecast(dstEnd - dstStart));

		if (n == 0)
		{
			auto header = getHeader();
			state->stream.WriteU8(header.CMF);
			state->stream.WriteU8(header.FLG); 
		}
		
		uint32_t adler = n == 0;
		auto srcStart = getPos(sourceLen, n, divides);
		auto srcEnd = getPos(sourceLen, n + 1, divides);

		state->AddData(source + srcStart, source + srcEnd - 1, adler, false);
		state->SetLevel(0);
		state->AddData(source + srcEnd - 1, source + srcEnd, adler, srcEnd == sourceLen);

		state->stream.Flush();

		return { state->stream.byteswritten(), adler };
	};
	 
	auto resultA = doPacket(0);
	auto resultB = doPacket(1);
	memmove(dest + resultA.Count, dest + getPos(*destLen, 1, divides), resultB.Count);
	auto adler = combine(resultA.Adler, resultB.Adler, sourceLen - getPos(sourceLen, 1, divides));
		//	auto future = std::async(doPacket, 1);
	auto written = resultA.Count + resultB.Count;	
	auto tempState = std::make_unique<EncoderState>(0, dest + written, safecast(destLen - written));

	tempState->stream.WriteBigEndianU32(adler);
	tempState->stream.Flush();
	 
	*destLen = safecast(written + 4);
}
