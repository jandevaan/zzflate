// ConsoleApplication1.cpp : Defines the entry point for the console application.
//
#include <vector>
#include <cassert> 
#include <gtest/gtest.h>

#include <zlib.h>
#include <fstream>
#include "../zzflate/zzflate.h"
#include <chrono>


  
#include "../zzflate/encoder.h"

#ifdef NDEBUG
bool debugging = false;
#else
bool debugging = true;
#endif

namespace ch = std::chrono;

#if _MSC_VER && !__INTEL_COMPILER

#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
std::vector<std::string> enumfiles(std::string folder)
{ 
	std::vector<std::string> files = {};
	  
	for (fs::directory_entry p : fs::directory_iterator(folder))
	{ 
		files.push_back(p.path().string());
 	}
	return files;
}
#else

#include <dirent.h>
std::vector<std::string> enumfiles(std::string folder)
{
    std::vector<std::string> files_ = {};
 
    DIR* dp = opendir (folder.c_str());
    
    if (dp != NULL)
    {
        struct dirent *ep = nullptr;
        
        while ((ep = readdir (dp)) != nullptr)
        {
            if (ep->d_name[0] == '.')
                continue;
            
            files_.push_back(folder + "/" + ep->d_name);
            std::cerr << files_[files_.size() -1];
        }
         
        closedir (dp);
    }
    else {
        std::cerr << ("Couldn't open the directory");
       
    }

        return files_;
}

#endif
std::vector<uint8_t> ReadFile(std::string name)
{
	std::ifstream file;
	file.exceptions(std::ifstream::badbit | std::ifstream::failbit | std::ifstream::eofbit);
	file.open(name.c_str(), std::ifstream::in | std::ifstream::binary);
	file.seekg(0, std::ios::end);
	size_t length = file.tellg();
	file.seekg(0, std::ios::beg);

	auto vec = std::vector<uint8_t>(length);

	file.read((char*)&vec.front(), length);
	return vec;
}


int ZlibUncompress(uint8_t *dest, size_t *destLen, const uint8_t *source, size_t sourceLen, bool gzip) 
{
	z_stream stream = {};
	int err;
	const unsigned int max = unsigned(~0);
	size_t left;
	uint8_t buf[1];    /* for detection of incomplete stream when *destLen == 0 */

	if (*destLen) {
		left = *destLen;
		*destLen = 0;
	}
	else {
		left = 1;
		dest = buf;
	}

	stream.next_in = (uint8_t *)source;
	stream.avail_in = 0;

	err = inflateInit2(&stream, gzip? 15 | 16 : 15);
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
std::vector<uint8_t> bufferCompressed = std::vector<uint8_t>(1500000);

int testroundtripperf(const std::vector<uint8_t>& bufferUncompressed, bool multithread, int compression, int repeatcount = 10)
{ 
	auto testSize = bufferUncompressed.size();

	bufferCompressed.resize(int(testSize * 1.01));
	
	size_t comp_len = 0;

	ch::system_clock::time_point times[11];

	times[0] = ch::system_clock::now();
 

	for (int i = 0; i < repeatcount; ++i)
	{
		comp_len = safecast(bufferCompressed.size());
		Config config = { Zlib, uint8_t(compression), multithread };

 		ZzFlateEncode(&bufferCompressed[0], &comp_len, &bufferUncompressed[0], bufferUncompressed.size(), &config);
 
		if (debugging)
			return 0;

		times[i + 1] = ch::system_clock::now();
	}

	std::cout << "Reduced "  << testSize << " to " << ((comp_len) * 100.0 / testSize) << "%\r\n";

	for(int i = 0; i < repeatcount; ++i)
	{
		auto microseconds = ch::duration_cast<ch::microseconds>(times[i + 1] - times[i]);
		std::cout << microseconds.count()/1000.0 << " ms\n";
	}

	return 0;
}


int testroundtripperfzlib(const std::vector<uint8_t>& bufferUncompressed, int compression, std::string name = {}, int repeatcount = 10)
{
	uLong source_len = safecast(bufferUncompressed.size());
	auto testSize = source_len; 
	uLongf comp_len = 0;
	bufferCompressed.resize(bufferUncompressed.size());
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


int testroundtrip(const std::vector<uint8_t>& bufferUncompressed, Config config, std::string name = "")
{ 
	auto testSize = bufferUncompressed.size();
	auto  compressed = std::vector<uint8_t>();
 
	if (config.level == 1)
	{
		compressed.resize(std::max((size_t)200,bufferUncompressed.size()));
		size_t comp_len = safecast(compressed.size());

		ZzFlateEncode(&compressed[0], &comp_len, &bufferUncompressed[0], bufferUncompressed.size(), &config);
		compressed.resize(comp_len);
	}
	else
	{
	 	ZzFlateEncodeToCallback(&bufferUncompressed[0], bufferUncompressed.size(), &config, [&compressed](auto buffer, auto count)->bool
		{
			compressed.insert(compressed.end(), buffer, buffer + count);
			return false;
		}); 
	}
	
	auto comp_len = compressed.size();
	std::cout << std::fixed;
	std::cout << std::setprecision(2);

	std::cout << "Reduced " << name << " " << testSize << " to " << ((comp_len) * 100.0 / testSize) << "%\r\n" ;

	std::vector<uint8_t> decompressed(testSize);
	auto unc_len = decompressed.size();

	auto error = ZlibUncompress(&decompressed[0], &unc_len, &compressed[0], comp_len, false);

	EXPECT_EQ(error, Z_OK);

	if (error != Z_OK)
		return -1;
 
	for (auto i = 0; i < testSize; ++i)
	{ 
		EXPECT_EQ(bufferUncompressed[i], decompressed[i]) << "pos " << i;
	}

	return safecast(comp_len);
}


int testroundtripgzip(const std::vector<uint8_t>& bufferUncompressed, int compression, std::string name = "")
{
	auto testSize = bufferUncompressed.size();
	auto  compressed = std::vector<uint8_t>();
 
	compressed.resize(bufferUncompressed.size());
	size_t comp_len = safecast(compressed.size());
	Config config = { Gzip, uint8_t(compression), false };

	ZzFlateEncode(&compressed[0], &comp_len, &bufferUncompressed[0], bufferUncompressed.size(), &config);

  
	std::cout << std::fixed;
	std::cout << std::setprecision(2);

	std::cout << "Reduced " << name << " " << testSize << " to " << ((comp_len) * 100.0 / testSize) << "%\r\n";

	std::vector<uint8_t> decompressed(testSize);
	auto unc_len = decompressed.size();

	auto error = ZlibUncompress(&decompressed[0], &unc_len, &compressed[0], comp_len, true);

	EXPECT_EQ(error, Z_OK);

	if (error != Z_OK)
		return -1;

	for (auto i = 0; i < testSize; ++i)
	{
		EXPECT_EQ(bufferUncompressed[i], decompressed[i]) << "pos " << i;
	}

	return safecast(comp_len);
}


namespace
{
	std::string prefix = "";
	std::vector<uint8_t> bufferUncompressed = ReadFile(prefix + "ADInsight.exe");
}

int main(int ac, char* av[])
{ 
	testing::InitGoogleTest(&ac, av);
	return RUN_ALL_TESTS();
}





TEST(Adler, Combine)
{
	std::vector<unsigned char> asdf = { 0, 1, 23, 30, 4,69, 145, 32,216 };

	auto adlerTotal = adler32x(1, &asdf[0], 9);

	int split = 5;
	auto adlerFirst = adler32x(1, &asdf[0], split);
	auto adlerSecond = adler32x(0, &asdf[split], 9 - split);
	auto combined = combine(adlerFirst, adlerSecond, 9 - split);

	ASSERT_EQ(adlerTotal, combined);
}
TEST(ZzGzip, Simple)
{
	testroundtripgzip(bufferUncompressed, 1);
}

TEST(ZzFlate, UserHuffman)
{ 
	testroundtrip(bufferUncompressed, {Zlib, 2});
}

TEST(ZzFlate, FixedHuffman)
{  
	testroundtrip(bufferUncompressed, { Zlib, 1 });
}
 

TEST(ZzFlate, SmallZerBouffer)
{
	std::vector<uint8_t> zeroBuffer;
	for (int i = 1; i <= 512; i = i * 2)
	{
		zeroBuffer.resize(i);
		testroundtrip(zeroBuffer, { Zlib, 2 });
	}
}
 

TEST(ZzFlate, CanterburyZzflate2)
{
	for (auto x : enumfiles(prefix + "corpus"))
	{
		testroundtrip(ReadFile(x), {Zlib, 2}, x);
	}	
}

TEST(ZzFlate, CanterburyZzflate3)
{
	for (auto x : enumfiles(prefix + "corpus"))
	{
		testroundtrip(ReadFile(x), { Zlib, 3 }, x);
	}
}

//
//TEST(ZzFlate, CanterburyNoCompression)
//{
//	for (auto x : enumfiles("c://dev//corpus"))
//	{
//		testroundtrip(ReadFile(x), 0, x);
//	}
//
//}




TEST(ZzFlate, CanterburyZlib)
{
	for (auto x : enumfiles(prefix + "corpus"))
	{
		testroundtripperfzlib(ReadFile(x), 1, x, 1);
	}
}


TEST(ZzFlate, CanterburyZlib3)
{
	for (auto x : enumfiles(prefix + "corpus"))
	{
		testroundtripperfzlib(ReadFile(x), 3, x, 1);
	}
}

TEST(ZzFlate, CanterburyZlib6)
{
	for (auto x : enumfiles(prefix + "corpus"))
	{
		testroundtripperfzlib(ReadFile(x), 6, x, 1);
	}
}

 

TEST(ZzFlate, LargeZlib)
{
	for (auto x : enumfiles(prefix + "large"))
	{
		testroundtripperfzlib(ReadFile(x), 1, x, 1);
	}
}

TEST(ZzFlate, LargeFiles)
{
	for (auto x : enumfiles(prefix + "large"))
	{
	//	testroundtripperf(ReadFile(x),false, 1, 1);
	}
}


TEST(ZzxFlatePerf, UncompressedPerf)
{
	testroundtripperf(bufferUncompressed, false, 0);
}
 
TEST(ZzxFlatePerf, UserHuffmanPerf)
{
	testroundtripperf(bufferUncompressed, false, 2);
}
TEST(ZzxFlatePerf, UserHuffmanPerfM)
{
	testroundtripperf(bufferUncompressed, true, 2);
}

TEST(ZzxFlatePerf, FixedHuffmanPerf)
{
	testroundtripperf(bufferUncompressed, false, 1);
}

TEST(ZzxFlatePerf, FixedHuffmanPerfM)
{
	testroundtripperf(bufferUncompressed, true, 1);
}



TEST(ZzxFlatePerf, FixedHuffmanPerf2)
{
	testroundtripperf(bufferUncompressed, false, 1);
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
