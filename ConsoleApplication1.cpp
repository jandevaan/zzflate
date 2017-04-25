// ConsoleApplication1.cpp : Defines the entry point for the console application.
//
 
#include <zlib.h>

 
#include <vector>
#include <cassert> 
#include <gtest/gtest.h>
#include "outputbitstream.h"
#include "huffman.h"

int uncompress(unsigned char *dest, size_t *destLen, const unsigned char *source, size_t sourceLen) {
	z_stream stream = {};
	int err;
	const unsigned int max = (unsigned int)0 - 1;
	size_t left;
	unsigned char buf[1];    /* for detection of incomplete stream when *destLen == 0 */

	if (*destLen) {
		left = *destLen;
		*destLen = 0;
	}
	else {
		left = 1;
		dest = buf;
	}

	stream.next_in = (unsigned char *)source;
	stream.avail_in = 0;

	err = inflateInit(&stream);
	if (err != Z_OK) return err;

	stream.next_out = dest;
	stream.avail_out = 0;

	do {
		if (stream.avail_out == 0) {
			stream.avail_out = left > (unsigned long)max ? max : (unsigned int)left;
			left -= stream.avail_out;
		}
		if (stream.avail_in == 0) {
			stream.avail_in = sourceLen > (unsigned long)max ? max : (unsigned int)sourceLen;
			sourceLen -= stream.avail_in;
		}
		err = inflate(&stream, Z_NO_FLUSH);
	} while (err == Z_OK);

	if (dest != buf)
		*destLen = stream.total_out;
	else if (stream.total_out && err == Z_BUF_ERROR)
		left = 1;

	inflateEnd(&stream);
	return err == Z_STREAM_END ? Z_OK :
		err == Z_NEED_DICT ? Z_DATA_ERROR :
		err == Z_BUF_ERROR && left + stream.avail_out ? Z_DATA_ERROR :
		err;
}
 
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
	auto h = header{ {DEFLATE, 7} };
	auto rem = (h.CMF* 0x100 + h.FLG) % 31;
	h.fieldsFLG.FCHECK = 31 - rem;
	return h;
}


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


struct distanceRecord
{
		int code;
		int bits;
		int distanceStart;
};

distanceRecord distanceTable[30]{
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
	23,10,3037,
	24,11,4097,
	25,11,6145,
	26,12,8193,
	27,12, 12289,
	28,13, 16385,
	29,13, 24577
};

struct lengthRecord
{
	short code;
	char extraBits;
	char extraBitLength;
};
lengthRecord  lengthTable[259];

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


void writeRun(outputbitstream& stream, std::vector<code>& codes, int last_value, int& repeat_count)
{
	if (repeat_count < 3)
	{
		for (int i = 0; i < repeat_count; ++i)
		{
			stream.AppendToBitStream(codes[last_value]);
		}
		repeat_count = 0;
		return;
	}
	while (repeat_count != 0)
	{
		int runLength = std::min(repeat_count, 258);
		auto lengthRecord = lengthTable[runLength];
		auto code = lengthRecord.code;

		// length
		stream.AppendToBitStream(codes[code]);
		stream.AppendToBitStream(lengthRecord.extraBits, lengthRecord.extraBitLength);
		
		// distance
		stream.AppendToBitStream(0, 5); // fixed 5 bit table, no extras
	
		repeat_count -= runLength;
	}


	repeat_count = 0;
}

void WriteBlock(const unsigned char* source, size_t sourceLen, outputbitstream& stream, int final)
{
	stream.AppendToBitStream(final, 1); // final
	stream.AppendToBitStream(0b01, 2); // fixed huffman table		
	 
	auto codes = huffman::generate(huffman::defaultTableLengths());

	int lastValue = -1;
	int repeatCount = 0;
	for(int i = 0; i < sourceLen; ++i)
	{
		auto value = source[i];
		if (lastValue == value)
		{
			repeatCount++;
			continue;
		}		
		if (repeatCount != 0)
		{
			writeRun(stream, codes, lastValue, repeatCount);
		}
		stream.AppendToBitStream(codes[value]);
		lastValue = value;
	}
	writeRun(stream, codes, lastValue, repeatCount);
	stream.AppendToBitStream(codes[256]);

	uint32_t adler = adler32x(source, sourceLen);
	stream.Flush();
	stream.writeuint32_bigendian(adler);
}

void compressNew(unsigned char *dest, unsigned long *destLen, const unsigned char *source, size_t sourceLen, int level)
{
	auto h = getHeader();
	auto length = *destLen;

	int offset = 0;

	outputbitstream stream(dest);

	stream.writebyte(h.CMF);
	stream.writebyte(h.FLG);

	if (level == 0)	
	{		
		stream.writeUncompressedBlock(source, (uint16_t)sourceLen, 1);
		*destLen = (int)stream.byteswritten();
		return;
	}
	if (level != 1)
	{
		*destLen = -1;
		return;
	}

	WriteBlock(source, sourceLen, stream, 1);
	*destLen = (int)stream.byteswritten();
}

int testroundtrip(std::vector<unsigned char>& bufferUncompressed, int compression)
{
	auto testSize = bufferUncompressed.size();
	auto  bufferCompressed = std::vector<unsigned char>(testSize + 5000);

	uLongf comp_len = (uLongf)bufferCompressed.size();
 
	compressNew(&bufferCompressed[0], &comp_len, &bufferUncompressed[0], bufferUncompressed.size(), compression);
	 
	std::vector<unsigned char> decompressed(testSize);
	auto unc_len = decompressed.size();

	uncompress(&decompressed[0], &unc_len, &bufferCompressed[0], comp_len);

	for (auto i = 0; i < testSize; ++i)
	{
		EXPECT_EQ(bufferUncompressed[i], decompressed[i]) << "pos " << i;
	}

	return (int)comp_len;
}

int main(int ac, char* av[])
{
	buildLengthLookup();

	testing::InitGoogleTest(&ac, av);
	return RUN_ALL_TESTS();
}


TEST(Zlib, SimpleUncompressed)
{
	auto bufferUncompressed = std::vector<unsigned char>(20, 3);
	
	for(int i = 0; i < 333; ++i)
	{
		bufferUncompressed.push_back(i * 39034621);
	}

	testroundtrip(bufferUncompressed, 0);

}



TEST(Zlib, SimpleHuffman)
{
	auto bufferUncompressed = std::vector<unsigned char>(200, 3);
	
	for (int i = 0; i < 323; ++i)
	{
		bufferUncompressed.push_back(i );
	}

	testroundtrip(bufferUncompressed, 1);

}




TEST(Zlib, GenerateHuffman)
{ 
	auto lengths = huffman::defaultTableLengths();
	auto result = huffman::generate(lengths);

	EXPECT_EQ(result[0  ].bits, huffman::reverse(0b00110000, lengths[0]));
	EXPECT_EQ(result[143].bits, huffman::reverse(0b10111111, lengths[143]));
	EXPECT_EQ(result[144].bits, huffman::reverse(0b110010000, lengths[144]));
	EXPECT_EQ(result[255].bits, huffman::reverse(0b111111111, lengths[255]));
	EXPECT_EQ(result[256].bits, huffman::reverse(0, lengths[256]));
	EXPECT_EQ(result[279].bits, huffman::reverse(0b0010111, lengths[279]));
	EXPECT_EQ(result[280].bits, huffman::reverse(0b11000000, lengths[280]));
	EXPECT_EQ(result[287].bits, huffman::reverse(0b11000111, lengths[287]));
	//EXPECT_EQ(result[0], 0);
}