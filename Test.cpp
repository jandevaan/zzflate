// ConsoleApplication1.cpp : Defines the entry point for the console application.
//
#include <vector>
#include <cassert> 
#include <gtest/gtest.h>

#include <zlib.h>
#include <fstream>
#include "zzflate.h"
#include <chrono>


#include <filesystem>
 
#include "safeint.h"

#ifdef NDEBUG
bool debugging = false;
#else
bool debugging = true;
#endif


namespace fs = std::experimental::filesystem;

std::vector<std::string> directory(std::string folder)
{

	std::vector<std::string> files = {};
	 
	for (fs::directory_entry p : fs::directory_iterator(folder))
	{ 
		files.push_back(p.path().string());
 	}
	return files;
}

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
	return vec;
}


int uncompress(unsigned char *dest, size_t *destLen, const unsigned char *source, size_t sourceLen) 
{
	z_stream stream = {};
	int err;
	const unsigned int max = unsigned(~0);
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
	bufferCompressed.resize(int(bufferUncompressed.size() * 1.01)); 
	
	uLongf comp_len = 0;

	std::chrono::steady_clock::time_point times[11];

	times[0] = std::chrono::high_resolution_clock::now();
 

	for (int i = 0; i < 10; ++i)
	{
		 
		comp_len = safecast(bufferCompressed.size());
		ZzFlateEncode(&bufferCompressed[0], &comp_len, &bufferUncompressed[0], bufferUncompressed.size(), compression);
		if (debugging)
			return 0;

		times[i + 1] = std::chrono::high_resolution_clock::now();
	}

	
	for(int i = 0; i < 10; ++i)
	{
		auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(times[i + 1] - times[i]);
		std::cout << microseconds.count() << " µs\n";
	}

	return 0;
}


int testroundtripperfzlib(const std::vector<unsigned char>& bufferUncompressed, int compression, std::string name = {}, int repeatcount = 10)
{
	uLong source_len = safecast(bufferUncompressed.size());
	auto testSize = source_len; 
	uLongf comp_len = 0;

	for (int i = 0; i < repeatcount; ++i)
	{
		comp_len = safecast(bufferCompressed.size());
		compress2(&bufferCompressed[0], &comp_len, &bufferUncompressed[0], source_len, compression);
		
	}
	std::cout << std::fixed;
	std::cout << std::setprecision(2);

	std::cout << name << "Reduced " << testSize << " to " << ((comp_len) * 100.0 / testSize) << "%" << "\r\n";

	return 0;
}

int testroundtrip(const std::vector<unsigned char>& bufferUncompressed, int compression, std::string name = "")
{ 
	auto testSize = bufferUncompressed.size();
	auto  compressed = std::vector<unsigned char>();

	if (compression != 0)
	{
		unsigned long compressedLength = testSize + (testSize >> 7);
		compressed.resize(compressedLength);
		ZzFlateEncode(&compressed[0], &compressedLength, &bufferUncompressed[0], bufferUncompressed.size(), compression);
		compressed.resize(compressedLength);
	}
	else
	{ 
		std::function<bool(const bufferHelper )> callback = [&compressed](auto h)->bool
		{
			for (int i = 0; i < h.bytesStored; ++i)
			{
				compressed.push_back(h.buffer[i]);
			}
 			return false;
		};
 		ZzFlateEncode2(&bufferUncompressed[0], bufferUncompressed.size(), compression, callback);
	}
	auto comp_len = compressed.size();
	std::cout << std::fixed;
	std::cout << std::setprecision(2);

	std::cout << "Reduced " << name << " " << testSize << " to " << ((comp_len) * 100.0 / testSize) << "%\r\n" ;

	std::vector<unsigned char> decompressed(testSize);
	auto unc_len = decompressed.size();

	auto error = uncompress(&decompressed[0], &unc_len, &compressed[0], comp_len);

	EXPECT_EQ(error, Z_OK);

	if (error != Z_OK)
		return -1;

	for (auto i = 0; i < testSize; ++i)
	{
		EXPECT_EQ(bufferUncompressed[i], decompressed[i]) << "pos " << i;
	}

	return safecast(comp_len);
}




int main(int ac, char* av[])
{
	buildLengthLookup();

	testing::InitGoogleTest(&ac, av);
	return RUN_ALL_TESTS();
}

namespace
{
	std::vector<unsigned char> bufferUncompressed = readFile("c:\\tools\\sysinternals\\adinsight.exe");
}


TEST(ZzFlate, FixedHuffman)
{ 
	testroundtrip(bufferUncompressed, 1);
}

TEST(ZzFlate, UserHuffman)
{ 
	testroundtrip(bufferUncompressed, 2);
}
 

TEST(ZzFlate, CanterburyZzflate)
{
	for(auto x : directory("c://dev//corpus"))
	{
		testroundtrip(readFile(x), 2, x);
	}
	
}

TEST(ZzFlate, CanterburyNoCompression)
{
	for (auto x : directory("c://dev//corpus"))
	{
		testroundtrip(readFile(x), 0, x);
	}

}



TEST(ZzFlate, CanterburyZlib)
{
	for (auto x : directory("c://dev//corpus"))
	{
		testroundtripperfzlib(readFile(x), 1, x, 1);
	}
}
   

TEST(ZzxFlatePerf, UncompressedPerf)
{
	testroundtripperf(bufferUncompressed, 0);
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
 