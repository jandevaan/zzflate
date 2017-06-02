

#include <vector>
#include <cassert> 
#include <gtest/gtest.h>
#include "outputbitstream.h"
#include "huffman.h"
#include <fstream>
#include <numeric>


struct lengthRecord
{
	short code;
	char extraBits;
	char extraBitLength;
};
struct distanceRecord
{
	int code;
	int bits;
	int distanceStart;
};

const int lengthCodeTable[] = {
	257,  0,
	258,  0,
	259,  0,
	260,  0,
	261,  0,
	262,  0,
	263,  0,
	264,  0,
	265,  1,
	266,  1,
	267,  1,
	268,  1,
	269,  2,
	270,  2,
	271,  2,
	272,  2,
	273,  3,
	274,  3,
	275,  3,
	276,  3,
	277,  4,
	278,  4,
	279,  4,
	280,  4,
	281,  5,
	282,  5,
	283,  5,
	284,  5
};



distanceRecord distanceTable[32]{
	0,0,1,
	1,0,2,
	2,0,3,
	3,0,4,
	4,1,5,
	5,1,7,
	6,2,9,
	7,2,13,
	8,3,17,
	9,3,25,
	10,4,33,
	11,4,49,
	12,5,65,
	13,5,97,
	14,6,129,
	15,6,193,
	16,7,257,
	17,7,385,
	18,8,513,
	19,8,769,
	20,9,1025,
	21,9,1537,
	22,10,2049,
	23,10,3073,
	24,11,4097,
	25,11,6145,
	26,12,8193,
	27,12, 12289,
	28,13, 16385,
	29,13, 24577,
	30,0, 32768,
	30,0, 32768,
};


lengthRecord  lengthTable[259];

code  lengthCodes[259];

#include "encoderstate.h"



struct header
{
	union
	{
		struct { unsigned char CM : 4, CINFO : 4; } fieldsCMF;
		unsigned char CMF;
	};
	union
	{
		struct { unsigned char FCHECK : 4, FDICT : 1, FLEVEL : 3; } fieldsFLG;
		unsigned char FLG;
	};
};





const int DEFLATE = 8;


header getHeader()
{
	auto h = header{ { DEFLATE, 7 } };
	auto rem = (h.CMF * 0x100 + h.FLG) % 31;
	h.fieldsFLG.FCHECK = 31 - rem;
	return h;
}


void buildLengthLookup()
{
	int offset = 3;

	lengthTable[0] = {};
	lengthTable[1] = {};
	lengthTable[2] = {};

	for (int i = 0; i < 28; ++i)
	{
		auto code = lengthCodeTable[i * 2];
		auto bits = lengthCodeTable[i * 2 + 1];

		for (int j = 0; j < (1 << bits); ++j)
		{
			lengthTable[offset++] = { (short)code, (char)j, (char)bits };
		}
	}

	lengthTable[258] = { 285, 0,0 };



}


const int ChooseRunCount(int repeat_count)
{
	if (repeat_count <= 258)
		return repeat_count;

	return std::min(repeat_count - 3, 258);
}

void WriteDeflateBlock(EncoderState& state, int final)
{
	if (state._level == 0)
	{
		state.writeUncompressedBlock(final);
	}
	else if (state._level == 1)
	{
		state.WriteBlockFixedHuff(FixedHuffman, final);
	}
	else if (state._level > 1)
	{
		state.WriteBlockV(UserDefinedHuffman, final);
	}

	state.EndBlock();
}

void ZzFlateEncode(unsigned char *dest, unsigned long *destLen, const unsigned char *source, size_t sourceLen, int level)
{
	if (level < 0 || level > 2)
	{
		*destLen = -1;
		return;
	}

	EncoderState state(level, dest);

	auto header = getHeader();
	state.stream.WriteU8(header.CMF);
	state.stream.WriteU8(header.FLG);

	auto end = source + sourceLen;
	state.source = source;
	
	if (level == 1)
	{
		state.codes = huffman::generate(huffman::defaultTableLengths());
		state.dcodes = std::vector<code>(32);
		for (int i = 0; i < 32; ++i)
		{
			state.dcodes[i] = { 5, (int)huffman::reverse(i, 5) };
		}

		for (int i = 0; i < 259; ++i)
		{
			auto lengthRecord = lengthTable[i];
			
			auto code = state.codes[lengthRecord.code]; 
			state.lcodes[i] = { 
				code.length + lengthRecord.extraBitLength,
				(lengthRecord.extraBits << code.length) | code.bits };
		}

	}

	auto adler = adler32x(source, sourceLen);

	while (state.source != end)
	{
		int bytes = std::min(end - state.source, 65536ll);
		state.length = bytes;
		WriteDeflateBlock(state, state.source + bytes == end ? 1 : 0);
		state.source += bytes;
	}



	// end of zlib stream (not block!)	
	state.stream.WriteBigEndianU32(adler);
	state.stream.Flush();

	*destLen = (int)state.stream.byteswritten();
}
