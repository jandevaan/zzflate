
#include "../huffman.h"
#include "../outputbitstream.h"
#include <gtest/gtest.h>


TEST(BitOutput, TestSimple)
{
	std::vector<uint8_t> buffer;
	buffer.resize(100);
	outputbitstream strm(&buffer[0], 100);

	strm.AppendToBitStream(1, 1);
	strm.AppendToBitStream(0, 2);
	
	EXPECT_EQ(buffer[0], 0);

	strm.Flush();

	EXPECT_EQ(buffer[0], 1);
}



TEST(BitOutput, TestSimple2)
{
	std::vector<uint8_t> buffer(100);
	outputbitstream strm(&buffer[0], 100);

	strm.AppendToBitStream(3, 2);
	strm.AppendToBitStream(0, 2);
	strm.AppendToBitStream(15, 4);
	strm.Flush();

	EXPECT_EQ(buffer[0], 0xF3);
}



TEST(BitOutput, TrivHuffman)
{
	std::vector <int> buffer = {2, 1, 3, 3};
	
	std::vector<code> codes(buffer.size());
	huffman::generate<code>(buffer, &codes[0]);

	EXPECT_EQ(codes[1].bits,0);
}