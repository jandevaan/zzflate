
#pragma once

#include <functional>
#include "huffman.h"
#include "visualc.h" 
namespace
{
	const char order[] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

	const int hashBits = 12;
	const unsigned hashSize = 1 << hashBits;
	const unsigned hashMask = hashSize -1;
	const int maxRecords = 20000;
} 

#include "userhuffman.h"


struct lengthRecord
{
	short code;
	char extraBits;
	char extraBitLength;
};


struct distanceRecord
{ 
	unsigned short bits;
	unsigned short distanceStart;
};



extern lengthRecord  lengthTable[259];

extern code  lengthCodes[259];

extern const distanceRecord distanceTable[32]; 
extern unsigned char distanceLut[32769];

enum CurrentBlockType
{
	Uncompressed = 0b00,
	FixedHuffman = 0b01,
	UserDefinedHuffman
};
 
struct compressionRecord
{
	int32_t literals;
	uint16_t backoffset;
	uint16_t length;
};


struct EncoderState
{
	outputbitstream stream;

	const unsigned char* source; 
 
	CurrentBlockType _type;
	int _level;

	
	std::vector<compressionRecord> comprecords;
	std::vector<int> lengths = std::vector<int>(288);

	static code codes_f[288]; // literals
	static code lcodes_f[259]; // table to send lengths (symbol + extra bits for all 258)
	static code dcodes_f[32];

	
	code codes[288]; // literals
	code lcodes[259]; // table to send lengths (symbol + extra bits for all 258)
	code dcodes[32];
	int hashtable[hashSize];

 
	EncoderState(int level, unsigned char* outputBuffer) :
		stream(outputBuffer),
		source(), 
		_level(level)
	{
		for (auto& h : hashtable)
		{
			h = -100000;
		}

		if (level == 0)
		{
			_type = Uncompressed;
		}
		else if (level == 1)
		{
			_type = FixedHuffman;
			InitFixedHuffman();
			comprecords.resize(maxRecords);
		}
		else
		{
			_type = UserDefinedHuffman;
			comprecords.resize(maxRecords);
		}
	}


	static int FindDistance(int offset)
	{
		for (int n = 1; n < std::size(distanceTable); ++n)
		{
			if (offset < distanceTable[n].distanceStart)
			{
				return n - 1;
			}
		}
		return -1;
	}


	static code Merge(code first, code second)
	{
		return code(first.length + second.length, second.bits << first.length | first.bits);
	}


	static void CreateMergedLengthCodes(code* lCodes, code* symbolCodes)
	{
		for (int i = 0; i < 259; ++i)
		{
			auto lengthRecord = lengthTable[i];

			lCodes[i] = Merge(symbolCodes[lengthRecord.code], 
			                  code(lengthRecord.extraBitLength, lengthRecord.extraBits)); 
		}
	}
	 

	static void InitFixedHuffman()
	{
		huffman::generate<code>(huffman::defaultTableLengths(), codes_f);
 
		for (int i = 0; i < 32; ++i)
		{
			dcodes_f[i] = code( 5, huffman::reverse(i, 5) );
		}

		CreateMergedLengthCodes(lcodes_f, codes_f);
 
	}

	void WriteDistance(const code* distCodes, int offset)
	{
		auto bucketId = distanceLut[offset];		 
		auto bucket = distanceTable[bucketId];
		stream.AppendToBitStream(distCodes[bucketId]);
		stream.AppendToBitStream(offset - bucket.distanceStart, bucket.bits);
	}
   
	void StartBlock(CurrentBlockType type, int final)
	{
		stream.AppendToBitStream(final, 1); // final
		stream.AppendToBitStream(type, 2); // fixed huffman table		

		_type = type;		
	}
	 
	 

	void WriteRecords(const unsigned char* src, const std::vector<compressionRecord>& vector)
	{
		int offset = 0;
		for (auto r : vector)
		{
			for (int n = 0; n < r.literals; ++n)
			{
				auto value = src[offset + n];
				stream.AppendToBitStream(codes[value]);
			}

			if (r.length != 0)
			{
				stream.AppendToBitStream(lcodes[r.length]);
				WriteDistance(dcodes, r.backoffset);
			}

			offset += r.literals + r.length;
		}
	}




	void writelengths(const std::vector<lenghtRecord>& records, const std::vector<code>& table)
	{
		for (auto r : records)
		{
			stream.AppendToBitStream(table[r.value]);
			switch (r.value)
			{
			case 16: stream.AppendToBitStream(r.payLoad - 3, 2); break;
			case 17: stream.AppendToBitStream(r.payLoad - 3, 3); break;
			case 18: stream.AppendToBitStream(r.payLoad - 11, 7); break;
			default: break;
			}
		}
	}

	std::vector<lenghtRecord> ComputeCodes(const std::vector<int>& frequencies, std::vector<int>& codeLengthFreqs, code* outputCodes)
	{
		CalcLengths(frequencies, lengths, 15);
		huffman::generate<code>(lengths, outputCodes);
		return FromLengths(lengths, codeLengthFreqs);
	}

    void DetermineAndWriteCodes(const std::vector<int>& symbolFreqs, const std::vector<int>& distanceFrequencies)
	{ 
		std::vector<int> lengthfrequencies(19, 0);
		
		auto symbolMetaCodes = ComputeCodes(symbolFreqs, lengthfrequencies, codes);
		auto distMetaCodes = ComputeCodes(distanceFrequencies, lengthfrequencies, dcodes);

		auto metaCodes = std::vector<code>(19);
		CalcLengths(lengthfrequencies, lengths, 7);
		huffman::generate<code>(lengths, &metaCodes[0]);

		// write the table
		stream.AppendToBitStream(safecast(symbolFreqs.size() - 257), 5);
		stream.AppendToBitStream(safecast(distanceFrequencies.size() - 1), 5); // distance code count
		stream.AppendToBitStream(safecast(lengthfrequencies.size() - 4), 4);
		
		for (int i = 0; i < 19; ++i)
		{
			stream.AppendToBitStream(lengths[order[i]], 3);
		}

		writelengths(symbolMetaCodes, metaCodes);
		writelengths(distMetaCodes, metaCodes);
 
		// update code tables 


		CreateMergedLengthCodes(lcodes, codes);
	}

	 static unsigned int CalcHash(const unsigned char * ptr )
	{
		const uint32_t* ptr32 = reinterpret_cast<const uint32_t*>(ptr);
		auto val = (*ptr32 >> 8) * 0x00d68664u;
  
		return val >> (32 - hashBits);
	}
	 
	static int remain(const unsigned char* a, const unsigned char* b, int matchLength, int maxLength)
	{
		if (maxLength > 258)
		{
			maxLength = 258;
		}
		 
		for (; matchLength < maxLength; ++matchLength)
		{
			if (a[matchLength] != b[matchLength])
				return matchLength;
		}

		return maxLength;
	}


	static __forceinline int countMatches(const unsigned char* a, const unsigned char* b, int maxLength)
	{

		int matchLength = 0;
		 
		typedef uint64_t compareType;

		if (maxLength >= (sizeof(compareType)))
		{
			matchLength = sizeof(compareType);

			auto delta = *(compareType*)a ^ *(compareType*)b;

			if (delta != 0)
				return ZeroCount(delta);
		}
		
		return remain(a, b, matchLength, int(maxLength));
	}

 

	void FixHashTable(int offset)
	{
		for (int i = 0; i < hashSize; ++i)
		{
			hashtable[i] = hashtable[i] - offset;
		}
	}

	int WriteBlockFixedHuff(int byteCount, int final)
	{
		StartBlock(FixedHuffman, final);
		
		for (int i = 0; i < byteCount; ++i)
		{
			auto sourcePtr = source + i;
			auto newHash = CalcHash(sourcePtr);
			int offset = hashtable[newHash];
			hashtable[newHash] = i;

			auto delta = i - offset;
			if (unsigned(delta) < 0x8000)
			{
				auto matchLength = countMatches(source + i, source + offset, safecast(byteCount - i));
				if (matchLength >= 3)
				{
					stream.AppendToBitStream(lcodes_f[matchLength].bits, lcodes_f[matchLength].length);
					WriteDistance(dcodes_f, delta);

					i += matchLength - 1;					 
					continue;
				}
			}

			auto code = codes_f[*sourcePtr];
			stream.AppendToBitStream(code.bits, code.length);
		}

		FixHashTable(byteCount);

		stream.AppendToBitStream(codes_f[256]);

		return byteCount;
	}

	int WriteBlock2Pass(int byteCount, bool final)
	{
		int length = (byteCount < 256000 && final) ? byteCount : std::min(256000, byteCount - 258);
		comprecords.resize(maxRecords);

		auto symbolFreqs = std::vector<int>(286,0);
		
		auto distanceFrequencies = std::vector<int>(30, 1);		
		int backRefEnd = 0;

		int recordCount = 0;

		for (int i = 0; i < length; ++i)
		{
			auto newHash = CalcHash(source + i);
			int offset = hashtable[newHash];
			hashtable[newHash] = i;
			 
			if (i - 32768 < offset)
			{
				auto matchLength = countMatches(source + i, source + offset, safecast(byteCount - i));
				 		
				if (matchLength >= 3)
				{				 
					symbolFreqs[lengthTable[matchLength].code]++;
					comprecords[recordCount++] = {  (i - backRefEnd), safecast(i - offset), safecast(matchLength) };
					int nextI = i + matchLength - 1;
					while (i < nextI)
					{
						hashtable[CalcHash(source + i)] = i;
						i++;
					}

					backRefEnd = nextI + 1;

					if (recordCount == maxRecords - 1)
					{
						length = i;
						break;
					}

					continue;
				}
			}
			
			symbolFreqs[source[i]]++;
		}

		length = std::max(length, backRefEnd);
		
		symbolFreqs[256]++;
		
		if (length< byteCount)
		{
			final = false;
		}

		comprecords[recordCount++] = { safecast(length - backRefEnd), 0,0};
		comprecords.resize(recordCount);
		FixHashTable(length);
		 
		StartBlock(_type, final);

		if (_type == UserDefinedHuffman)
		{
			DetermineAndWriteCodes(symbolFreqs, distanceFrequencies);
		}

		WriteRecords(source, comprecords);

		stream.AppendToBitStream(codes[256]);

		return length;
	}
 



	int WriteUncompressedBlock(int byteCount, int final)	
	{
		uint16_t length = safecast(std::min(byteCount, 0xFFFF));
		StartBlock(Uncompressed, final && length == byteCount);
		stream.WriteU16(length);
		stream.WriteU16(uint16_t(~length));
		stream.Flush();

		memcpy(stream._stream, source, length);
		stream._stream += length;

		return length;
	}
 


	int WriteDeflateBlock(int length, bool final)
	{
		if (_level == 0)
		{
			return WriteUncompressedBlock(length, final);
		}
		else if (_level == 1)
		{
			return WriteBlockFixedHuff(length, final);
		}
		else if (_level == 2)
		{
			return WriteBlock2Pass(length, final);
		}
	 
		return -1;
	}


	void AddData(const unsigned char* start, const unsigned char* end, uint32_t& adler)
	{
		source = start;
		while (source != end)
		{  
			auto bytesWritten = WriteDeflateBlock(safecast(end - source), true);
			adler = adler32x(adler, source, bytesWritten);
			source += bytesWritten;
		}
	}
	
};
