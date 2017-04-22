
#include "outputbitstream.h"
#include <gtest/gtest.h>


TEST(BitOutput, TestSimple)
{
	std::vector<unsigned char> buffer;
	buffer.resize(100);
	outputbitstream strm(&buffer[0]);

	strm.AppendToBitStream(1, 1);
	strm.AppendToBitStream(0, 2);
	
	EXPECT_EQ(buffer[0], 0);

	strm.Flush();

	EXPECT_EQ(buffer[0], 1);
}



TEST(BitOutput, TestSimple2)
{
	std::vector<unsigned char> buffer(100);
	outputbitstream strm(&buffer[0]);

	strm.AppendToBitStream(3, 2);
	strm.AppendToBitStream(0, 2);
	strm.AppendToBitStream(15, 4);

	EXPECT_EQ(buffer[0], 0xF3);
}
