#include <cassert> 
#include <gtest/gtest.h> 


#include "encoderstate.h"


lengthRecord  lengthTable[259];

 scode EncoderState::codes_f[288]; // literals
 code EncoderState::lcodes_f[259]; // table to send lengths (symbol + extra bits for all 258)
 code EncoderState::dcodes_f[32];


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



const distanceRecord distanceTable[32]{
	0,1,
	0,2,
	0,3,
	0,4,
	1,5,
	1,7,
	2,9,
	2,13,
	3,17,
	3,25,
	4,33,
	4,49,
	5,65,
	5,97,
	6,129,
	6,193,
	7,257,
	7,385,
	8,513,
	8,769,
	9,1025,
	9,1537,
	10,2049,
	10,3073,
	11,4097,
	11,6145,
	12,8193,
	12, 12289,
	13, 16385,
	13, 24577,
	0, 32768,
	0, 32768,
};

unsigned char distanceLut[32769];

 

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
			lengthTable[offset++] = { safecast(code), safecast(j), safecast(bits) };
		}
	}

	lengthTable[258] = { 285, 0,0 };

	for (int i = 1; i < 32769; ++i)
	{
		distanceLut[i] = safecast(EncoderState::FindDistance(i));
	} 

	EncoderState::InitFixedHuffman();
}


void ZzFlateEncode(unsigned char *dest, unsigned long *destLen, const unsigned char *source, size_t sourceLen, int level)
{
	if (level < 0 || level >3)
	{
		*destLen = ~0ul;
		return;
	}

	EncoderState state(level, dest);

	auto header = getHeader();
	state.stream.WriteU8(header.CMF);
	state.stream.WriteU8(header.FLG);

	uint32_t adler = 1; 

	state.AddData(source, source + sourceLen, adler);
	 
	state.stream.WriteBigEndianU32(adler);
	state.stream.Flush();

	*destLen = safecast(state.stream.byteswritten());
}
