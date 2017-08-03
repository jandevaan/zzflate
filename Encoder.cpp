#include <cassert> 
#include <gtest/gtest.h> 
#include <memory>


#include "encoderstate.h"

#include "zzflate.h"

lengthRecord  EncoderState::lengthTable[259];

code EncoderState::codes_f[286]; // literals
code EncoderState::lcodes_f[259]; // table to send lengths (symbol + extra bits for all 258)
code EncoderState::dcodes_f[32];
const uint8_t EncoderState::order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

const uint8_t EncoderState::extraDistanceBits[32] = { 0,0,0,0, 1,1,2,2, 3,3,4,4, 5,5,6,6, 7,7,8,8, 9,9,10,10, 11,11,12,12, 13,13,0,0 };

const uint8_t EncoderState::extraLengthBits[286] =
{
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	0, 0,0,0,0, 0,0,0,0, 1,1,1,1, 2,2,2,2, 3,3,3,3, 4,4,4,4, 5,5,5,5, 0
};

const uint16_t EncoderState::distanceTable[32]{
	1,
	2,
	3,
	4,
	5,
	7,
	9,
	13,
	17,
	25,
	33,
	49,
	65,
	97,
	129,
	193,
	257,
	385,
	513,
	769,
	1025,
	1537,
	2049,
	3073,
	4097,
	6145,
	8193,
	12289,
	16385,
	24577,
	32768,
	32769,
};

uint8_t EncoderState::distanceLut[32769];


void EncoderState::buildLengthLookup()
{
	int offset = 3;

	lengthTable[0] = {};
	lengthTable[1] = {};
	lengthTable[2] = {};

	for (int i = 0; i < 28; ++i)
	{
		int calccode = 257 + i;
		int bits = extraLengthBits[calccode];

		for (int j = 0; j < (1 << bits); ++j)
		{
			lengthTable[offset++] = { safecast(calccode), safecast(j), safecast(bits) };
		}
	}

	lengthTable[258] = { 285, 0,0 };

	for (int i = 1; i < 32769; ++i)
	{
		distanceLut[i] = safecast(FindDistance(i));
	}

	InitFixedHuffman();
}


 void EncoderState::InitFixedHuffman()
{
	huffman::generate<code>(huffman::defaultTableLengths(), codes_f);

	for (int i = 0; i < 32; ++i)
	{
		dcodes_f[i] = code(5, huffman::reverse(i, 5));
	}

	CreateMergedLengthCodes(lcodes_f, codes_f);

}

