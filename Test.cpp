// ConsoleApplication1.cpp : Defines the entry point for the console application.
//
#include <vector>
#include <cassert> 
#include <gtest/gtest.h>

#include <zlib.h>
#include <fstream>
#include "zzflate.h"
#include <chrono>


std::vector<unsigned char> readFile(std::string name)
{
	std::ifstream file;
	file.exceptions(std::ifstream::badbit | std::ifstream::failbit | std::ifstream::eofbit);
	file.open(name.c_str(), std::ifstream::in | std::ifstream::binary);
	file.seekg(0, std::ios::end);
	size_t length = file.tellg();
	file.seekg(0, std::ios::beg);

	auto vec = std::vector<unsigned char>(length);

	file.read((char*)&vec.front(), length);
//	vec.resize(vec.size() / 5);
	return vec;
}


int uncompress(unsigned char *dest, size_t *destLen, const unsigned char *source, size_t sourceLen) 
{
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

	if (err < Z_OK)
	{
		std::cout << stream.msg << "\r\n";
	}

	inflateEnd(&stream);
	return err == Z_STREAM_END ? Z_OK :
		err == Z_NEED_DICT ? Z_DATA_ERROR :
		err == Z_BUF_ERROR && left + stream.avail_out ? Z_DATA_ERROR :
		err;
}
std::vector<unsigned char> bufferCompressed = std::vector<unsigned char>(1500000);

int testroundtripperf(std::vector<unsigned char>& bufferUncompressed, int compression)
{ 
	auto testSize = bufferUncompressed.size();
	
	uLongf comp_len;

	std::chrono::steady_clock::time_point times[11];

	times[0] = std::chrono::high_resolution_clock::now();
	
	for (int i = 0; i < 10; ++i)
	{
		 
		comp_len = (uLongf)bufferCompressed.size();
		ZzFlateEncode(&bufferCompressed[0], &comp_len, &bufferUncompressed[0], bufferUncompressed.size(), compression);
		times[i + 1] = std::chrono::high_resolution_clock::now();
	}

	

	for(int i = 0; i < 10; ++i)
	{
		auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(times[i + 1] - times[i]);
		std::cout << microseconds.count() << " µs\n";
	}

	return 0;
}


int testroundtripperfzlib(std::vector<unsigned char>& bufferUncompressed, int compression)
{
	uLong source_len = bufferUncompressed.size();
	auto testSize = source_len; 
	uLongf comp_len;

	for (int i = 0; i < 10; ++i)
	{
		comp_len = (uLongf)bufferCompressed.size();
		compress2(&bufferCompressed[0], &comp_len, &bufferUncompressed[0], source_len, compression);
	}

	std::cout << "Reduced " << testSize << " to " << ((testSize) * 100 / testSize) << "%";

	return 0;
}

int testroundtrip(std::vector<unsigned char>& bufferUncompressed, int compression)
{ 
	auto testSize = bufferUncompressed.size();
	auto  bufferCompressed = std::vector<unsigned char>(testSize * 3 / 2 + 5000);
	uLongf comp_len;

	comp_len = (uLongf)bufferCompressed.size();
	ZzFlateEncode(&bufferCompressed[0], &comp_len, &bufferUncompressed[0], bufferUncompressed.size(), compression);

	std::cout << "Reduced " << testSize << " to " << ((testSize) * 100 / testSize) << "%";

	std::vector<unsigned char> decompressed(testSize);
	auto unc_len = decompressed.size();

	auto error = uncompress(&decompressed[0], &unc_len, &bufferCompressed[0], comp_len);

	EXPECT_EQ(error, Z_OK);

	if (error != Z_OK)
		return -1;

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

namespace
{
	std::vector<unsigned char> bufferUncompressed = readFile("e:\\tools\\adinsight.exe");
}


TEST(ZzFlate, FixedHuffman)
{ 
	testroundtrip(bufferUncompressed, 1);
}

TEST(ZzFlate, UserHuffman)
{ 
	testroundtrip(bufferUncompressed, 2);
}


TEST(ZzFlate, SimpleUncompressed)
{
	auto bufferUncompressed = std::vector<unsigned char>(20, 3);

	for (int i = 0; i < 3333; ++i)
	{
		bufferUncompressed.push_back(i * 39034621);
	}

	testroundtrip(bufferUncompressed, 0);

}





TEST(ZzxFlatePerf, UserHuffmanPerf)
{
	testroundtripperf(bufferUncompressed, 2);
}





TEST(ZzxFlatePerf, FixedHuffmanPerf)
{
	testroundtripperf(bufferUncompressed, 1);
}


TEST(ZzxFlatePerf, FixedHuffmanPerf2)
{
	testroundtripperf(bufferUncompressed, 1);
}




TEST(ZlibPerf, ZlibCompress1)
{

	testroundtripperfzlib(bufferUncompressed, 1);
}

TEST(ZlibPerf, ZlibCompress3)
{
	testroundtripperfzlib(bufferUncompressed, 3);
}

TEST(ZlibPerf, ZlibCompress6)
{
	testroundtripperfzlib(bufferUncompressed, 6);
}

TEST(ZlibPerf, ZlibCompress9)
{
	testroundtripperfzlib(bufferUncompressed, 9);
}