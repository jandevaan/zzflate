

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
		int b = state.FindDistance2(distance);

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
	auto result = huffman::generate(huffman::defaultTableLengths());

	EXPECT_EQ(result[0].bits, huffman::reverse(0b00110000, huffman::defaultTableLengths()[0]));
	EXPECT_EQ(result[143].bits, huffman::reverse(0b10111111, huffman::defaultTableLengths()[143]));
	EXPECT_EQ(result[144].bits, huffman::reverse(0b110010000, huffman::defaultTableLengths()[144]));
	EXPECT_EQ(result[255].bits, huffman::reverse(0b111111111, huffman::defaultTableLengths()[255]));
	EXPECT_EQ(result[256].bits, huffman::reverse(0, huffman::defaultTableLengths()[256]));
	EXPECT_EQ(result[279].bits, huffman::reverse(0b0010111, huffman::defaultTableLengths()[279]));
	EXPECT_EQ(result[280].bits, huffman::reverse(0b11000000, huffman::defaultTableLengths()[280]));
	EXPECT_EQ(result[287].bits, huffman::reverse(0b11000111, huffman::defaultTableLengths()[287]));
	//EXPECT_EQ(result[0], 0);
}