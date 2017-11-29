
#include <vector>
#include <cassert> 
#include <numeric>
#include <gtest/gtest.h>
#include "../zzflate/huffman.h"
#include "../zzflate/encoder.h"


TEST(ZzFlate, TestDistanceSearch)
{
	Encoder state(0, nullptr, 0);

	int failCount = 0;

	for (int distance = 1; distance <= 32768; ++distance)
	{
		int a = Encoder::FindDistance(distance);
		int b = Encoder::ReadLut(distance);

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
	std::vector<code> result(lengths.size());
	
	huffman::generate<code>(lengths, & result[0]);
	
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