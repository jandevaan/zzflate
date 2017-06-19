

#include <vector>
#include <cassert> 
#include <gtest/gtest.h>
#include "huffman.h"
#include <numeric>
#include "encoderstate.h"


TEST(ZzFlate, TestDistanceSearch)
{
	EncoderState state(0, nullptr);

	int failCount = 0;

	for (int distance = 0; distance < 32768; ++distance)
	{
		int a = state.FindDistance(distance);
		int b = distanceLut[distance];

		if (a != b)
		{
			if (failCount == 0)
			{
				std::cout << "Fail " << distance;
			}
			failCount++;			
		} 
	}
	 
	EXPECT_EQ(failCount, 0);
}


TEST(ZzFlate, GenerateHuffman)
{
	auto lengths = huffman::defaultTableLengths();
	auto result = huffman::generate(lengths);

	EXPECT_EQ(result[0].bits, huffman::reverse(0b00110000, lengths[0]));
	EXPECT_EQ(result[143].bits, huffman::reverse(0b10111111, lengths[143]));
	EXPECT_EQ(result[144].bits, huffman::reverse(0b110010000, lengths[144]));
	EXPECT_EQ(result[255].bits, huffman::reverse(0b111111111, lengths[255]));
	EXPECT_EQ(result[256].bits, huffman::reverse(0, lengths[256]));
	EXPECT_EQ(result[279].bits, huffman::reverse(0b0010111, lengths[279]));
	EXPECT_EQ(result[280].bits, huffman::reverse(0b11000000, lengths[280]));
	EXPECT_EQ(result[287].bits, huffman::reverse(0b11000111, lengths[287]));
	//EXPECT_EQ(result[0], 0);
}