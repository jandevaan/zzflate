#include <cassert> 
#include <gtest/gtest.h> 
#include <memory>


#include "encoderstate.h"

#include "zzflate.h"

lengthRecord  Encoder::lengthTable[259];

code Encoder::codes_f[286]; // literals
code Encoder::lcodes_f[259]; // table to send lengths (symbol + extra bits for all 258)
code Encoder::dcodes_f[30];
const uint8_t Encoder::order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

const uint8_t Encoder::extraDistanceBits[30] = { 0,0,0,0, 1,1,2,2, 3,3,4,4, 5,5,6,6, 7,7,8,8, 9,9,10,10, 11,11,12,12, 13,13 };

const uint8_t Encoder::extraLengthBits[286] =
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

const uint16_t Encoder::distanceTable[30]{
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
	24577 
};

uint8_t Encoder::distanceLut[32769];

int Encoder::FindDistance(int offset)
{
	for (int n = 1; n < std::size(distanceTable); ++n)
	{
		if (offset < distanceTable[n])
		{
			return n - 1;
		}
	}
	return offset <= 32768 ? 29 : -1;
}


void Encoder::buildLengthLookup()
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


 void Encoder::InitFixedHuffman()
{
	huffman::generate<code>(huffman::defaultTableLengths(), codes_f);

	for (int i = 0; i < 30; ++i)
	{
		dcodes_f[i] = code(5, huffman::reverse(i, 5));
	}

	CreateMergedLengthCodes(lcodes_f, codes_f);

}

