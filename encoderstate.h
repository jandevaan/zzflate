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

enum CurrentBlockType
{
	Uncompressed = 0b00,
	FixedHuffman = 0b01,
	UserDefinedHuffman
};
 
struct compressionRecord
{
	uint32_t literals;
	uint16_t backoffset;
	uint16_t length;
};


struct EncoderState
{

private:
	static const int hashBits = 13;
	static const unsigned hashSize = 1 << hashBits;
	static const unsigned hashMask = hashSize - 1;
	static const int maxRecords = 20000;


	int level;
	std::vector<compressionRecord> comprecords;
	std::vector<int> lengths = std::vector<int>(286);

	// for fixed huffman encoding
	static code codes_f[286]; // literals
	static code lcodes_f[259]; // table to send lengths (symbol + extra bits for all 258)
	static code dcodes_f[32];

	static lengthRecord  lengthTable[259]; 

	static const uint8_t order[19];
	static const uint16_t distanceTable[32];
public:
	static uint8_t distanceLut[32769];
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

	static void buildLengthLookup();
	static void InitFixedHuffman();


	static int FindDistance(int offset)
	{
		for (int n = 1; n < std::size(distanceTable); ++n)
		{
			if (offset < distanceTable[n])
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


	
	void WriteDistance(const code* distCodes, int offset)
	{
		auto bucketId = distanceLut[offset];
		auto start = distanceTable[bucketId];
		stream.AppendToBitStream(distCodes[bucketId]);
		stream.AppendToBitStream(offset - start, extraDistanceBits[bucketId]);
	}

	void StartBlock(CurrentBlockType t, int final)
	{
		stream.AppendToBitStream(final, 1); // final
		stream.AppendToBitStream(t, 2); // fixed huffman table	
	}



	void WriteRecords(const uint8_t* src, const std::vector<compressionRecord>& vector)
	{
		int offset = 0;
		for (auto r : vector)
		{
			for (unsigned n = 0; n < r.literals; ++n)
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
	  

	std::vector<lenghtRecord> ComputeCodes(const std::vector<int>& frequencies, std::vector<int>& codeLengthFreqs, code* outputCodes)
	{ 	 
		CalcLengths(frequencies, lengths, 15);
		huffman::generate<code>(lengths, outputCodes);
		return FromLengths(lengths, codeLengthFreqs);
	}

	int64_t CountBits(const std::vector<int> & frequencies, const uint8_t * extraBits)
	{
		int64_t total = 0;
		for (int i = 0; i < frequencies.size(); ++i)
		{
			int64_t length = lengths[i] + extraBits[i];
			total += frequencies[i] * length;
		}
		return total;
	}

	struct LengthCounter
	{
		void AppendToBitStream(int, int length)
		{
			totalLength += length;
		}
		
		int totalLength;
	};
 

	int WriteBlock2Pass(const uint8_t * source, int byteCount, bool final)
	{
		auto symbolFreqs = std::vector<int>(286, 0);
		auto distanceFrequencies = std::vector<int>(30, 0);
		int length = FirstPass(source, byteCount, final, symbolFreqs, distanceFrequencies);
		 
		int64_t userBlockBitLength = 0;
		std::vector<int> lengthfrequencies(19, 0);
		auto symbolMetaCodes = ComputeCodes(symbolFreqs, lengthfrequencies, codes);
		userBlockBitLength += CountBits(symbolFreqs, extraLengthBits);

		auto distMetaCodes = ComputeCodes(distanceFrequencies, lengthfrequencies, dcodes);
		userBlockBitLength += CountBits(distanceFrequencies, extraDistanceBits);

		auto metaCodes = std::vector<code>(19);
		CalcLengths(lengthfrequencies, lengths, 7);
		huffman::generate<code>(lengths, &metaCodes[0]);

		LengthCounter lengthCounter = { safecast(3 + 5 + 5 + 4 + 3 * 19 + userBlockBitLength) };
		WriteLengths(lengthCounter, symbolMetaCodes, metaCodes);
		WriteLengths(lengthCounter, distMetaCodes, metaCodes);

		auto requiredLength = (lengthCounter.totalLength + 8) / 8;

		if (requiredLength >= length)		 
			return UncompressedFallback(length, source, final);
		
		int available = stream.EnsureOutputLength(requiredLength);
		if (available < length)
			return 0;

		StartBlock(UserDefinedHuffman, length < byteCount ? 0 : final);

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

		WriteRecords(source, comprecords);

		stream.AppendToBitStream(codes[256]);

		return length;
	}

	int UncompressedFallback(int length, const uint8_t * source, bool final)
	{
		int bytesWritten = 0;
		while (bytesWritten < length)
		{
			auto count = WriteUncompressedBlock(source + bytesWritten, length - bytesWritten, final);
			if (count <= 0)
				return 0;

			bytesWritten += count;
		}
		return bytesWritten;
	}

 

	//*	
		// This hashfunction is broken: it skips the first byte.
		// Any attempt to fix it ruins compression ratios, strangely enough. 
	static unsigned int CalcHash(const uint8_t * ptr)
	{
		const uint32_t* ptr32 = reinterpret_cast<const uint32_t*>(ptr - 1);
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

	static int countMatchBackward(const uint8_t* a, const uint8_t* b, int maxLength)
	{  
		for (int matchLength = -1; -matchLength < maxLength; --matchLength)
		{
			if (a[matchLength] != b[matchLength])
				return matchLength;
		}

		return maxLength - 1;
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
		int bytesToEncode = safecast(std::min(int64_t(byteCount), bitsAvailable / 9 - 8));
		if (bytesToEncode != byteCount)
		{
			final = 0;
		}
		StartBlock(FixedHuffman, final);
			 
		for (int i = 0; i < bytesToEncode; ++i)
		{
			auto sourcePtr = source + i;
			auto newHash = CalcHash(sourcePtr + 1);			 
			auto distance = i - hashtable[newHash];
			hashtable[newHash] = i;

			if (unsigned(distance) <= 0x8000)
			{
				auto matchLength = countMatches(sourcePtr, sourcePtr - distance, safecast(bytesToEncode - i));

				if (matchLength >= 3)
				{
					stream.AppendToBitStream(lcodes_f[matchLength].bits, lcodes_f[matchLength].length);
					WriteDistance(dcodes_f, distance);

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
	 
	int FirstPass(const uint8_t * source, int byteCount, bool final, std::vector<int> &symbolFreqs, std::vector<int> &distanceFrequencies)
	{
		int length = (byteCount < 256000 && final) ? byteCount : std::min(256000, byteCount - 258);
		comprecords.resize(maxRecords);
	
		int backRefEnd = 0;
		int recordCount = 0; 
		for (int i = 0; i < length; ++i)
		{
			auto sourcePtr = source + i;
			auto newHash = CalcHash(sourcePtr + 1);		 
			auto distance = i - (hashtable[newHash]);
			hashtable[newHash] = i;

			if (distance > 32768)
				continue;

			auto matchLength = countMatches(sourcePtr + 1, sourcePtr + 1 - distance, safecast(byteCount - i - 1));

			if (sourcePtr[0] == sourcePtr[-distance])
			{
				if (matchLength < 3)
					continue;

				if (matchLength < 258)
				{
					matchLength++;
				}

				
				AddHashEntries(source, i + 1, matchLength - 1);
			}
			else						
			{ 
				if (matchLength < 4)
			  		continue;
			 
				i++;
				AddHashEntries(source, i , matchLength  );
			}

			comprecords[recordCount++] = { safecast(i - backRefEnd), safecast(distance), safecast(matchLength) };			  
		
			i += matchLength - 1;
			backRefEnd = i + 1;
			 
			if (recordCount == maxRecords - 1)
			{
				length = i;
				break;
			} 
		}

		length = std::max(length, backRefEnd);
		FixHashTable(length);

		comprecords[recordCount++] = { safecast(length - backRefEnd), 0,0 };
		comprecords.resize(recordCount);
		int index = 0;
		for (auto r : comprecords)
		{
			for (int i = 0; i < r.literals; i++)
			{
				symbolFreqs[source[index + i]]++;			 
			}
			
			symbolFreqs[lengthTable[r.length].code]++;
			distanceFrequencies[distanceLut[r.backoffset]]++;
			index += r.literals + r.length;
		}
		symbolFreqs[256]++;

		return length;

	}

	void AddHashEntries(const uint8_t * source, int i, int extra)
	{
		for (int n = i; n < i + extra; ++n)
		{
			hashtable[CalcHash(source + n + 1)] = n;

		}
	}



	int WriteUncompressedBlock(const uint8_t* source, int byteCount, int final)
	{
		auto length = std::min(byteCount, 0xFFFF);

		int worstCaseLength = 6 + length;
		 
		int outputbytesAvailable = stream.EnsureOutputLength(worstCaseLength);
 
		if (outputbytesAvailable <= 40)
			return 0;

		length = std::min(length, outputbytesAvailable - 6);

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
