// ConsoleApplication1.cpp : Defines the entry point for the console application.
//
 
#include <zlib.h>

 
#include <vector>
#include <cassert> 
#include <gtest/gtest.h>
#include "outputbitstream.h"
#include "huffman.h"
#include <fstream>


std::vector<unsigned char> readFile(std::string name)
{ 
	std::ifstream file;
	file.exceptions( std::ifstream::badbit | std::ifstream::failbit | std::ifstream::eofbit);	
	file.open(name.c_str(), std::ifstream::in | std::ifstream::binary);
	file.seekg(0, std::ios::end);
	size_t length = file.tellg();
	file.seekg(0, std::ios::beg);

	auto vec = std::vector<unsigned char>(length);
	
	file.read((char*)&vec.front(), length);

	return vec;
}


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

distanceRecord distanceTable[31]{
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
	29,13, 24577,
	30,0, 32768,
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


void WriteDistance(outputbitstream& stream, int offset)
{
	for(int n = 1; n < std::size(distanceTable); ++n)
	{
		if (offset < distanceTable[n + 1].distanceStart )
		{
			auto bucket = distanceTable[n - 1];
			stream.AppendToBitStream(bucket.code, 5); // fixed 5 bit table
			stream.AppendToBitStream(bucket.distanceStart - offset, bucket.bits);
			return;
		}
	}	

	throw std::exception("Distance too far away");
}

void WriteLength(outputbitstream& stream, std::vector<code>& codes, int runLength)
{
	auto lengthRecord = lengthTable[runLength];
	auto code = lengthRecord.code;

	stream.AppendToBitStream(codes[code]);
	stream.AppendToBitStream(lengthRecord.extraBits, lengthRecord.extraBitLength);
}

const int ChooseRunCount(int repeat_count)
{
	if (repeat_count <= 258)
		return repeat_count;

	return std::min(repeat_count - 3, 258);
}

enum CurrentBlockType
{
	Uncompressed = 0b00,
	FixedHuffman = 0b01,
	UserDefinedHuffman	
};

struct EncoderState
{
	EncoderState(int level, unsigned char* outputBuffer)
		: stream(outputBuffer),
		_level(level)
	{
		
	}

	
	void StartBlock(CurrentBlockType type, int final)
	{
		stream.AppendToBitStream(final, 1); // final
		stream.AppendToBitStream(type, 2); // fixed huffman table		

		_type = type;
		if (_type == FixedHuffman)
		{
			codes = huffman::generate(huffman::defaultTableLengths());
		}		
	}

	void WriteBlock(const unsigned char* source, size_t sourceLen, int final)
	{
		StartBlock(FixedHuffman, final);

		int lastValue = -1;
		int repeatCount = 0;
		for (int i = 0; i < sourceLen; ++i)
		{
			auto value = source[i];
			if (lastValue == value)
			{
				repeatCount++;
				continue;
			}
			if (repeatCount != 0)
			{
				writeRun(lastValue, repeatCount);
			}
			stream.AppendToBitStream(codes[value]);
			lastValue = value;
		}
		writeRun(lastValue, repeatCount);
		
	}
	struct record
	{
		int frequency;
		int id;
	};
	struct compressionRecord
	{
		unsigned int literals;
		unsigned short backoffset;
		unsigned short length;
	};
	 

	struct treeItem
	{
		int frequency;
		int left;
		int right;
		int bits;
	};


	std::vector<char> calcLengths(std::vector<record>& freqs)
	{
		std::vector<char> lengths;
		long long symbolcount = 0;
		for (int i = 0; i <= 285; ++i)
		{
			symbolcount += freqs[i].frequency;
			freqs[i].id = i;
		}
		freqs[256].frequency++;
		
		// cheap hack to prevent excessively long codewords.
		int limit = symbolcount / (1 << 15);

		std::vector<treeItem> tree;
		for (auto& record : freqs)
		{ 
			record.frequency = std::max(limit, record.frequency);
			tree.push_back({ record.frequency, record.id, -1 });
		}

		std::sort(freqs.begin(), freqs.end(),
		          [](auto a, auto b) -> bool { return a.frequency > b.frequency; });

		auto nonzero = std::count_if(freqs.begin(), freqs.end(), [](record r) -> bool { return r.frequency != 0; });
		
		freqs.resize(nonzero);

		while (freqs.size() >= 2)
		{
			auto a = freqs[freqs.size() - 2];
			auto b = freqs[freqs.size() - 1];
			freqs.resize(freqs.size() - 2);

			auto sumfreq = a.frequency + b.frequency;
			tree.push_back({ sumfreq, a.id, b.id });

			record val = { sumfreq, (int)(tree.size() - 1) };

			auto index = std::upper_bound(freqs.begin(), freqs.end(), val, [](auto a, auto b) -> bool { return a.frequency > b.frequency; });

			freqs.insert(index, val);			
		}

		lengths = std::vector<char>(286);

		for(int i = tree.size() - 1; i > 285; --i)
		{
			auto item = tree[i];
			tree[item.left].bits = item.bits + 1;
			tree[item.right].bits = item.bits + 1;
		}

		for (int i = 0; i < lengths.size(); ++i)
		{
			lengths[i] = tree[i].bits;
		}

		return lengths;
	}
	int FindBackRef(const unsigned char* source, int index, int end, int* offset)
	{
		if (index == 0)
			return 0;

		int max = std::min(end, index + 255);

		auto val = source[index -1];
		int i = index;
		for (; i < max; ++i)
		{
			if (source[i] != val)
				break;
		}
		i -= index;
		*offset = 1;
	  
		if (i >= 4) 
			return i;
		else return 0;
	}


	void CheckLength(std::vector<compressionRecord> comprecords)
	{
		int count = 0;
		for (auto rec : comprecords)
		{
			count += rec.literals + rec.length;
		}
	}

	void WriteRecords(const unsigned char* source, const std::vector<compressionRecord>& vector, bool final)
	{
		StartBlock(FixedHuffman, final);
		 
		int offset = 0;
		for (auto r : vector)
		{
			for (int n = 0; n < r.literals; ++n)
			{
				auto value = source[offset + n];
				stream.AppendToBitStream(codes[value]);
			}
			
			if (r.length != 0)
			{
				WriteLength(stream, codes, r.length);
				WriteDistance(stream, r.backoffset);
			}
			
			offset += r.length + r.literals;
		}
	}


	void WriteBlockV(const unsigned char* source, size_t sourceLen, int final)
	{
		auto comprecords = std::vector<compressionRecord>();
		
		unsigned int literals = 0;
		for (int i = 0; i < sourceLen; ++i)
		{
			int offset;
			auto matchLength = FindBackRef(source, i, sourceLen, &offset);
			if (matchLength == 0)
			{
				literals++;
				continue;
			}
			  
			comprecords.push_back({ literals, (unsigned short)offset, (unsigned short)matchLength });
			i += matchLength - 1;
			literals = 0;
		}

		comprecords.push_back({ literals, 0,0});		
		 	

		WriteRecords(source, comprecords, final);
	}

	void writeRun(int last_value, int& repeat_count)
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
			int runLength = ChooseRunCount(repeat_count);
			WriteLength(stream, codes, runLength);
			WriteDistance(stream, 1);
			repeat_count -= runLength;
		}

		repeat_count = 0;
	}


	void writeUncompressedBlock(const unsigned char* source, uint16_t sourceLen, int final)
	{
		StartBlock(Uncompressed, final);
		stream.Flush();

		stream.WriteU16(sourceLen);
		stream.WriteU16(~sourceLen);

		memcpy(stream._stream, source, sourceLen);
		stream._stream += sourceLen;

	}

	void EndBlock()
	{
		if (_type != Uncompressed)
		{
			stream.AppendToBitStream(codes[256]);
		}
	}

	CurrentBlockType _type;
	outputbitstream stream;
	std::vector<code> codes;
	int _level;
};


void WriteDeflateBlock(EncoderState& state, const unsigned char* source, size_t sourceLen)
{
	if (state._level == 0)
	{		
		state.writeUncompressedBlock(source, (uint16_t)sourceLen, 1);
	}
	else if (state._level == 1)
	{
		state.WriteBlockV(source, sourceLen, 1); 
	}
	
	state.EndBlock();
}

void EncodeZlib(unsigned char *dest, unsigned long *destLen, const unsigned char *source, size_t sourceLen, int level)
{
	if (level < 0 || level > 1)
	{
		*destLen = -1;
		return;
	}

	EncoderState state(level, dest);
	
	auto header = getHeader();	
	state.stream.writebyte(header.CMF);
	state.stream.writebyte(header.FLG);
	  
	WriteDeflateBlock(state, source, sourceLen);

	// end of zlib stream (not block!)
	auto adler = adler32x(source, sourceLen);
	state.stream.Flush();
	state.stream.WriteBigEndianU32(adler);

	*destLen = (int)state.stream.byteswritten();
}

int testroundtrip(std::vector<unsigned char>& bufferUncompressed, int compression)
{
	auto testSize = bufferUncompressed.size();
	auto  bufferCompressed = std::vector<unsigned char>(testSize + 5000);

	uLongf comp_len = (uLongf)bufferCompressed.size();
 
	EncodeZlib(&bufferCompressed[0], &comp_len, &bufferUncompressed[0], bufferUncompressed.size(), compression);
	 
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
	
	for(int i = 0; i < 3333; ++i)
	{
		bufferUncompressed.push_back(i * 39034621);
	}

	testroundtrip(bufferUncompressed, 0);

}



TEST(Zlib, SimpleHuffman)
{
	auto bufferUncompressed = readFile("e:\\tools\\ADInsight.exe");
	
	 
	testroundtrip(bufferUncompressed, 1);
}




TEST(Zlib, GenerateHuffman)
{
	auto result = huffman::generate(huffman::defaultTableLengths());

	EXPECT_EQ(result[0  ].bits, huffman::reverse(0b00110000, huffman::defaultTableLengths()[0]));
	EXPECT_EQ(result[143].bits, huffman::reverse(0b10111111, huffman::defaultTableLengths()[143]));
	EXPECT_EQ(result[144].bits, huffman::reverse(0b110010000, huffman::defaultTableLengths()[144]));
	EXPECT_EQ(result[255].bits, huffman::reverse(0b111111111, huffman::defaultTableLengths()[255]));
	EXPECT_EQ(result[256].bits, huffman::reverse(0, huffman::defaultTableLengths()[256]));
	EXPECT_EQ(result[279].bits, huffman::reverse(0b0010111, huffman::defaultTableLengths()[279]));
	EXPECT_EQ(result[280].bits, huffman::reverse(0b11000000, huffman::defaultTableLengths()[280]));
	EXPECT_EQ(result[287].bits, huffman::reverse(0b11000111, huffman::defaultTableLengths()[287]));
	//EXPECT_EQ(result[0], 0);
}