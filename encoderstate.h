#ifndef _ZZENCODERSTATE
#define _ZZENCODERSTATE
 
#include "config.h" 
#include <functional>
#include <algorithm>
#include "huffman.h" 
#include "outputbitstream.h"

  
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
extern uint8_t distanceLut[32769];

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
	static const int hashBits = 13;
	static const unsigned hashSize = 1 << hashBits;
	static const unsigned hashMask = hashSize - 1;
	static const int maxRecords = 20000;

private:
	CurrentBlockType type;
	int level;
	std::vector<compressionRecord> comprecords;
	std::vector<int> lengths = std::vector<int>(286);

	// for fixed huffman encoding
	static code codes_f[286]; // literals
	static code lcodes_f[259]; // table to send lengths (symbol + extra bits for all 258)
	static code dcodes_f[32];

	static const uint8_t order[19];

public:
	static const uint8_t extraLengthBits[286];
	static const uint8_t extraDistanceBits[32];


	code codes[286]; // literals
	code lcodes[259]; // table to send lengths (symbol + extra bits for all 258)
	code dcodes[32];
	int hashtable[hashSize];

public:
	outputbitstream stream;


	void Init()
	{
		for (auto& h : hashtable)
		{
			h = -100000;
		}

		if (level == 0)
		{
			type = Uncompressed;
		}
		else if (level == 1)
		{
			type = FixedHuffman;
		}
		else
		{
			type = UserDefinedHuffman;
			comprecords.resize(maxRecords);
		}
	}

public:

	EncoderState(int level, uint8_t* outputBuffer, int64_t bytes) :
		stream(outputBuffer, bytes),
		level(level)
 	{ 
		Init();
	}


	EncoderState(int level) :
		stream(nullptr, 0),
		level(level)
 	{
 		Init();
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
			dcodes_f[i] = code(5, huffman::reverse(i, 5));
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

	void StartBlock(CurrentBlockType t, int final)
	{
		stream.AppendToBitStream(final, 1); // final
		stream.AppendToBitStream(t, 2); // fixed huffman table		

		type = t;
	}



	void WriteRecords(const uint8_t* src, const std::vector<compressionRecord>& vector)
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



	template<class STREAMTYPE>
	static void WriteLengths(STREAMTYPE& stream, const std::vector<lenghtRecord>& records, const std::vector<code>& table)
	{
		for (auto r : records)
		{
			const code& entry = table[r.value];
			stream.AppendToBitStream(entry.bits, entry.length);
			switch (r.value)
			{
			case 16: stream.AppendToBitStream(r.payLoad - 3, 2); break;
			case 17: stream.AppendToBitStream(r.payLoad - 3, 3); break;
			case 18: stream.AppendToBitStream(r.payLoad - 11, 7); break;
			default: break;
			}
		}
	}
	int64_t userBlockBitLength = 0;

	std::vector<lenghtRecord> ComputeCodes(const std::vector<int>& frequencies, std::vector<int>& codeLengthFreqs, const uint8_t* extraBits, code* outputCodes)
	{
		 
		CalcLengths(frequencies, lengths, 15);
		for (int i = 0; i < frequencies.size(); ++i)
		{
			int64_t length = lengths[i] + extraBits[i]; 
			userBlockBitLength += frequencies[i] * length;
		}
		huffman::generate<code>(lengths, outputCodes);
		return FromLengths(lengths, codeLengthFreqs);
	}

	struct LengthCounter
	{
		void AppendToBitStream(int, int length)
		{
			totalLength += length;
		}
		
		int totalLength;
	};

	void DetermineAndWriteCodes(const std::vector<int>& symbolFreqs, const std::vector<int>& distanceFrequencies)
	{
		userBlockBitLength = 0;
		std::vector<int> lengthfrequencies(19, 0);
	 	auto symbolMetaCodes = ComputeCodes(symbolFreqs, lengthfrequencies, EncoderState::extraLengthBits, codes);
		auto distMetaCodes = ComputeCodes(distanceFrequencies, lengthfrequencies, EncoderState::extraDistanceBits, dcodes);

		auto metaCodes = std::vector<code>(19);
		CalcLengths(lengthfrequencies, lengths, 7);
		huffman::generate<code>(lengths, &metaCodes[0]);

		LengthCounter lengthCounter = { safecast(5 + 5 + 4 + 3 * 19  + userBlockBitLength) };
		WriteLengths(lengthCounter, symbolMetaCodes, metaCodes);
		WriteLengths(lengthCounter, distMetaCodes, metaCodes); 
	
		//std::cout << "Total bits to write: " << lengthCounter.totalLength;
		 
		// write the table
		stream.AppendToBitStream(safecast(symbolFreqs.size() - 257), 5);
		stream.AppendToBitStream(safecast(distanceFrequencies.size() - 1), 5); // distance code count
		stream.AppendToBitStream(safecast(lengthfrequencies.size() - 4), 4);

		for (int i = 0; i < 19; ++i)
		{
			stream.AppendToBitStream(lengths[order[i]], 3);
		}
		 

		WriteLengths(stream, symbolMetaCodes, metaCodes);
		WriteLengths(stream, distMetaCodes, metaCodes);

		// update code tables 
		CreateMergedLengthCodes(lcodes, codes);
	}

	void LogDistances(const std::vector<int> & distanceFrequencies)
	{
		int distCount = 0;
		for (auto n : distanceFrequencies) { distCount += n; }
		std::cout << "\r\n Distances " << distCount << " ";
	}

	//*	
		// This hashfunction is broken: it skips the first byte.
		// Any attempt to fix it ruins compression ratios, strangely enough. 
	static unsigned int CalcHash(const uint8_t * ptr)
	{
		const uint32_t* ptr32 = reinterpret_cast<const uint32_t*>(ptr);
		auto val = (*ptr32 >> 8) * 0x00d68664u;

		return val >> (32 - hashBits);
	}

	/*/

		 static unsigned int CalcHash(const uint8_t * ptr)
		 {
			 const uint32_t* ptr32 = reinterpret_cast<const uint32_t*>(ptr);
			 auto n = (*ptr32);
			 return n % 4093;
		 }

	//*/
	static int remain(const uint8_t* a, const uint8_t* b, int matchLength, int maxLength)
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


	static __forceinline int countMatches(const uint8_t* a, const uint8_t* b, int maxLength)
	{

		int matchLength = 0;

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

	int WriteBlockFixedHuff(const uint8_t * source, int byteCount, int final)
	{
		auto outputBytesAvailable = stream.EnsureOutputLength(byteCount) - 1;
		int64_t bitsAvailable = outputBytesAvailable * 8;
		int bytesToEncode = std::min(int64_t(byteCount), bitsAvailable / 9 - 8);
		if (bytesToEncode != byteCount)
		{
			final = 0;
		}
		StartBlock(FixedHuffman, final);
			 
		for (int i = 0; i < bytesToEncode; ++i)
		{
			auto sourcePtr = source + i;
			auto newHash = CalcHash(sourcePtr);
			int offset = hashtable[newHash];
			hashtable[newHash] = i;

			auto delta = i - offset;
			if (unsigned(delta) < 0x8000)
			{
				auto matchLength = countMatches(source + i, source + offset, safecast(bytesToEncode - i));

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

 	 	FixHashTable(bytesToEncode);		 
		stream.AppendToBitStream(codes_f[256]);
		return bytesToEncode;
	}

	int WriteBlock2Pass(const uint8_t * source, int byteCount, bool final)
	{
		int length = (byteCount < 256000 && final) ? byteCount : std::min(256000, byteCount - 258);
		comprecords.resize(maxRecords);

		auto symbolFreqs = std::vector<int>(286, 0);

		auto distanceFrequencies = std::vector<int>(30, 0);
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
					distanceFrequencies[distanceLut[i - offset]]++;

					comprecords[recordCount++] = { (i - backRefEnd), safecast(i - offset), safecast(matchLength) };
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
		 
		comprecords[recordCount++] = { safecast(length - backRefEnd), 0,0 };
		comprecords.resize(recordCount);
		FixHashTable(length);
		StartBlock(type, length < byteCount ? 0 : final);
		int64_t currentPos = stream.BitsWritten();

		int available = stream.EnsureOutputLength(length);
		if (available < length)
			return 0;

		DetermineAndWriteCodes(symbolFreqs, distanceFrequencies);
		 
		WriteRecords(source, comprecords);

		stream.AppendToBitStream(codes[256]);

	//	std::cout << "Actual writtn" << (currentPos - stream.BitsWritten() ) ;
		 
		return length;
	}



	int WriteUncompressedBlock(const uint8_t* source, int byteCount, int final)
	{
		auto length = std::min(byteCount, 0xFFFF);

		int worstCaseLength = 6 + length;
		 
		int outputbytesAvailable = stream.EnsureOutputLength(worstCaseLength);

		length = std::min(length, outputbytesAvailable - 6);
		 
		if (length <= 40)
			return 0;
		 
		StartBlock(Uncompressed, final && length == byteCount);
		stream.PadToByte();
		stream.WriteU16(safecast(length));
		stream.WriteU16(uint16_t(~length));
		stream.WriteBytes(source, length);

		return length;
	}



	int WriteDeflateBlock(const uint8_t * source,int inputLength, bool final)
	{
		if (level == 0)
		{
			return WriteUncompressedBlock(source, inputLength, final);
		}
		else if (level == 1)
		{
			return WriteBlockFixedHuff(source, inputLength, final);
		}
		else if (level == 2)
		{
			return WriteBlock2Pass(source, inputLength, final);
		}

		return -1;
	}



	bool AddData(const uint8_t* start, const uint8_t* end, uint32_t& adler)
	{ 
		while (start != end)
		{
		 	
			auto bytesRead = WriteDeflateBlock(start, safecast(end - start), true);
			if (bytesRead <= 0)
				return false;

			adler = adler32x(adler, start, bytesRead);
			start += bytesRead;
		}
		return true;
	}

};

#endif
