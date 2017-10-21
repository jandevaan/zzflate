#include <cassert> 
#include <memory>

#include "encoder.h"
 
#include <iostream>
#include "zzflate.h"

lengthRecord  Encoder::lengthTable[259] = {
	{ 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 },
	
	{ 257, 0, 0 }, { 258, 0, 0 }, { 259, 0, 0 }, { 260, 0, 0 }, { 261, 0, 0 }, { 262, 0, 0 }, { 263, 0, 0 }, { 264, 0, 0 },
	
	{ 265, 0, 1 }, { 265, 1, 1 }, 	
	{ 266, 0, 1 }, { 266, 1, 1 }, 	
	{ 267, 0, 1 }, { 267, 1, 1 }, 	
	{ 268, 0, 1 }, { 268, 1, 1 },

	{ 269, 0, 2 }, { 269, 1, 2 }, { 269, 2, 2 }, { 269, 3, 2 }, 
	{ 270, 0, 2 }, { 270, 1, 2 }, { 270, 2, 2 }, { 270, 3, 2 },
	{ 271, 0, 2 }, { 271, 1, 2 }, { 271, 2, 2 }, { 271, 3, 2 }, 
	{ 272, 0, 2 }, { 272, 1, 2 }, { 272, 2, 2 }, { 272, 3, 2 },

	{ 273, 0, 3 }, { 273, 1, 3 }, { 273, 2, 3 }, { 273, 3, 3 }, { 273, 4, 3 }, { 273, 5, 3 }, { 273, 6, 3 }, { 273, 7, 3 },	
	{ 274, 0, 3 }, { 274, 1, 3 }, { 274, 2, 3 }, { 274, 3, 3 }, { 274, 4, 3 }, { 274, 5, 3 }, { 274, 6, 3 }, { 274, 7, 3 },	
	{ 275, 0, 3 }, { 275, 1, 3 }, { 275, 2, 3 }, { 275, 3, 3 }, { 275, 4, 3 }, { 275, 5, 3 }, { 275, 6, 3 }, { 275, 7, 3 },	
	{ 276, 0, 3 }, { 276, 1, 3 }, { 276, 2, 3 }, { 276, 3, 3 }, { 276, 4, 3 }, { 276, 5, 3 }, { 276, 6, 3 }, { 276, 7, 3 },

	{ 277, 0, 4 }, { 277, 1, 4 }, { 277, 2, 4 }, { 277, 3, 4 }, { 277, 4, 4 }, { 277, 5, 4 }, { 277, 6, 4 }, { 277, 7, 4 },
	{ 277, 8, 4 }, { 277, 9, 4 }, { 277, 10, 4 }, { 277, 11, 4 }, { 277, 12, 4 }, { 277, 13, 4 }, { 277, 14, 4 }, { 277, 15, 4 },	

	{ 278, 0, 4 }, { 278, 1, 4 }, { 278, 2, 4 }, { 278, 3, 4 }, { 278, 4, 4 }, { 278, 5, 4 }, { 278, 6, 4 }, { 278, 7, 4 },
	{ 278, 8, 4 }, { 278, 9, 4 }, { 278, 10, 4 }, { 278, 11, 4 }, { 278, 12, 4 }, { 278, 13, 4 }, { 278, 14, 4 }, { 278, 15, 4 }, 
	
	{ 279, 0, 4 }, { 279, 1, 4 }, { 279, 2, 4 }, { 279, 3, 4 }, { 279, 4, 4 }, { 279, 5, 4 }, { 279, 6, 4 }, { 279, 7, 4 }, 
	{ 279, 8, 4 }, { 279, 9, 4 }, { 279, 10, 4 }, { 279, 11, 4 }, { 279, 12, 4 }, { 279, 13, 4 }, { 279, 14, 4 }, { 279, 15, 4 }, 

	{ 280, 0, 4 }, { 280, 1, 4 }, { 280, 2, 4 }, { 280, 3, 4 },	{ 280, 4, 4 }, { 280, 5, 4 }, { 280, 6, 4 }, { 280, 7, 4 }, 
	{ 280, 8, 4 }, { 280, 9, 4 }, { 280, 10, 4 }, { 280, 11, 4 }, { 280, 12, 4 }, { 280, 13, 4 }, { 280, 14, 4 }, { 280, 15, 4 },

	{ 281, 0, 5 }, { 281, 1, 5 }, { 281, 2, 5 }, { 281, 3, 5 }, { 281, 4, 5 }, { 281, 5, 5 }, { 281, 6, 5 }, { 281, 7, 5 }, 
	{ 281, 8, 5 }, { 281, 9, 5 }, { 281, 10, 5 }, { 281, 11, 5 }, { 281, 12, 5 }, { 281, 13, 5 }, { 281, 14, 5 }, { 281, 15, 5 }, 
	{ 281, 16, 5 }, { 281, 17, 5 }, { 281, 18, 5 }, { 281, 19, 5 }, { 281, 20, 5 }, { 281, 21, 5 }, { 281, 22, 5 }, { 281, 23, 5 }, 
	{ 281, 24, 5 }, { 281, 25, 5 }, { 281, 26, 5 }, { 281, 27, 5 }, { 281, 28, 5 }, { 281, 29, 5 }, { 281, 30, 5 }, { 281, 31, 5 }, 
	
	{ 282, 0, 5 }, { 282, 1, 5 }, { 282, 2, 5 }, { 282, 3, 5 }, { 282, 4, 5 }, { 282, 5, 5 }, { 282, 6, 5 }, { 282, 7, 5 }, 
	{ 282, 8, 5 }, { 282, 9, 5 }, { 282, 10, 5 }, { 282, 11, 5 }, { 282, 12, 5 }, { 282, 13, 5 }, { 282, 14, 5 }, { 282, 15, 5 }, 
	{ 282, 16, 5 }, { 282, 17, 5 }, { 282, 18, 5 }, { 282, 19, 5 }, { 282, 20, 5 }, { 282, 21, 5 }, { 282, 22, 5 }, { 282, 23, 5 }, 
	{ 282, 24, 5 }, { 282, 25, 5 }, { 282, 26, 5 }, { 282, 27, 5 }, { 282, 28, 5 }, { 282, 29, 5 }, { 282, 30, 5 }, { 282, 31, 5 }, 
	
	{ 283, 0, 5 }, { 283, 1, 5 }, { 283, 2, 5 }, { 283, 3, 5 }, { 283, 4, 5 }, { 283, 5, 5 }, { 283, 6, 5 }, { 283, 7, 5 }, 
	{ 283, 8, 5 },  { 283, 9, 5 }, { 283, 10, 5 }, { 283, 11, 5 }, { 283, 12, 5 }, { 283, 13, 5 }, { 283, 14, 5 }, { 283, 15, 5 },
	{ 283, 16, 5 }, { 283, 17, 5 }, { 283, 18, 5 }, { 283, 19, 5 }, { 283, 20, 5 }, { 283, 21, 5 }, { 283, 22, 5 }, { 283, 23, 5 }, 
	{ 283, 24, 5 }, { 283, 25, 5 }, { 283, 26, 5 }, { 283, 27, 5 }, { 283, 28, 5 }, { 283, 29, 5 }, { 283, 30, 5 }, { 283, 31, 5 }, 
	
	{ 284, 0, 5 }, { 284, 1, 5 }, { 284, 2, 5 }, { 284, 3, 5 }, { 284, 4, 5 }, { 284, 5, 5 }, { 284, 6, 5 }, { 284, 7, 5 }, 
	{ 284, 8, 5 }, { 284, 9, 5 }, { 284, 10, 5 }, { 284, 11, 5 }, { 284, 12, 5 }, { 284, 13, 5 }, { 284, 14, 5 }, { 284, 15, 5 }, 
	{ 284, 16, 5 }, { 284, 17, 5 }, { 284, 18, 5 }, { 284, 19, 5 }, { 284, 20, 5 }, { 284, 21, 5 }, { 284, 22, 5 }, { 284, 23, 5 }, 
	{ 284, 24, 5 }, { 284, 25, 5 }, { 284, 26, 5 }, { 284, 27, 5 }, { 284, 28, 5 }, { 284, 29, 5 }, { 284, 30, 5 },
	
	{ 285, 0, 0 },
};

code Encoder::codes_f[286]; // literals
code Encoder::lcodes_f[259]; // table to send lengths (symbol + extra bits for all 258)
code Encoder::dcodes_f[30];
const int Encoder::hashBufferLen;
const uint8_t Encoder::order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

const uint8_t Encoder::extraDistanceBits[30] = { 0,0,0,0, 1,1,2,2, 3,3,4,4, 5,5,6,6, 7,7,8,8, 9,9,10,10, 11,11,12,12, 13,13 };

const uint8_t Encoder::extraLengthBits[286] =
{
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	0, 0,0,0,0, 0,0,0,0, 1,1,1,1, 2,2,2,2, 3,3,3,3, 4,4,4,4, 5,5,5,5, 0
};

const uint16_t Encoder::distanceTable[30]{
	1,
	2,
	3,
	4,
	5,
	7,
	9,
	13,
	17,
	25,
	33,
	49,
	65,
	97,
	129,
	193,
	257,
	385,
	513,
	769,
	1025,
	1537,
	2049,
	3073,
	4097,
	6145,
	8193,
	12289,
	16385,
	24577 
};



inline unsigned int Encoder::CalcHash(const uint8_t * ptr)
{
	const uint32_t* ptr32 = reinterpret_cast<const uint32_t*>(ptr);
	auto val = ((*ptr32 << 8) >> 8) * 0x00d68664u;

	return val >> (32 - hashBits);
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


struct LengthCounter
{
	void AppendToBitStream(int, int length)
	{
		totalLength += length;
	}

	int totalLength;
};



uint8_t Encoder::distanceLut[32769];

int Encoder::FindDistance(int offset)
{
	for (int n = 1; n < 30; ++n)
	{
		if (offset < distanceTable[n])
		{
			return n - 1;
		}
	}
	return offset <= 32768 ? 29 : -1;
}

 
void Encoder::buildLengthLookup()
{
	for (int i = 1; i < 32769; ++i)
	{
		distanceLut[i] = safecast(FindDistance(i));
	}

	InitFixedHuffman();
}


 void Encoder::InitFixedHuffman()
{
	huffman::generate<code>(huffman::defaultTableLengths(), codes_f);

	for (int i = 0; i < 30; ++i)
	{
		dcodes_f[i] = code(5, huffman::reverse(i, 5));
	}

	CreateMergedLengthCodes(lcodes_f, codes_f);

}


 inline int  remain(const uint8_t * a, const uint8_t * b, int matchLength, int maxLength)
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


int  remain(const uint8_t * a, const uint8_t * b, int matchLength)
 { 
	 for (; matchLength < 258; ++matchLength)
	 {
		 if (a[matchLength] != b[matchLength])
			 return matchLength;
	 }

	 return 258;
 }

 inline int  countMatchBackward(const uint8_t * a, const uint8_t * b, int maxLength)
 {
	 for (int matchLength = 0; matchLength < maxLength; ++matchLength)
	 {
 		 if (a[-1 - matchLength] != b[-1 - matchLength])
			 return matchLength;
		  
	 }

	 return maxLength;
 }


   ZZINLINE int countMatches(const uint8_t* a, const uint8_t* b, int maxLength)
 {

	 int matchLength = 0;

	 
		auto delta = *(compareType*)a ^ *(compareType*)b;

		if (delta != 0)
			return ZeroCount(delta);

		matchLength = sizeof(compareType);
	 
	 return remain(a, b, matchLength, int(maxLength));
 }


   ZZINLINE int countMatches2(const uint8_t* a, const uint8_t* b)
   { 
	   auto delta = *(compareType*)a ^ *(compareType*)b;

	   if (delta != 0)
		   return ZeroCount(delta);
	    
	   return remain(a, b, sizeof(compareType));
   }

 inline code  Merge(code first, code second)
 {
	 return code(first.length + second.length, second.bits << first.length | first.bits);
 }

 void Encoder::CreateMergedLengthCodes(code * lCodes, code * symbolCodes)
 {
	 for (int i = 0; i < 259; ++i)
	 {
		 auto lengthRecord = lengthTable[i];

		 lCodes[i] = Merge(symbolCodes[lengthRecord.code],
			 code(lengthRecord.extraBitLength, lengthRecord.extraBits));
	 }
 }

 inline void Encoder::WriteDistance(const code * distCodes, int offset)
 {
	 auto bucketId = distanceLut[offset];
	 auto start = distanceTable[bucketId];
	 stream.AppendToBitStream(distCodes[bucketId]);
	 stream.AppendToBitStream(offset - start, extraDistanceBits[bucketId]);
 }

 void Encoder::StartBlock(CurrentBlockType t, int final)
 {
	 stream.AppendToBitStream(final, 1); // final
	 stream.AppendToBitStream(t, 2); // fixed huffman table	
 }

 void Encoder::WriteRecords(const uint8_t * src)
 {
	 int offset = 0;
	 for (int i = 0; i < validRecords; ++i)
	 {
		 auto r = comprecords[i];
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

 std::vector<lenghtRecord> Encoder::ComputeCodes(const std::vector<int>& frequencies, std::vector<int>& codeLengthFreqs, code * outputCodes)
 {
	 CalcLengths(frequencies, lengths, 15);
	 huffman::generate<code>(lengths, outputCodes);
	 return FromLengths(lengths, codeLengthFreqs);
 }

 int64_t Encoder::CountBits(const std::vector<int>& frequencies, const uint8_t * extraBits)
 {
	 int64_t total = 0;
	 for (int i = 0; i < frequencies.size(); ++i)
	 {
		 int64_t length = lengths[i] + extraBits[i];
		 total += frequencies[i] * length;
	 }
	 return total;
 }

#ifdef _DEBUG

 void Encoder::AuditRecords(const uint8_t * source, int byteCount)
 {
	 int index = 0;
	 for (int i = 0; i < validRecords; ++i)
	 {
		 auto r = comprecords[i];

		 index += r.literals;
		 if (r.length == 0)
			 continue;

		 assert(3 <= r.length && r.length <= 258);
		 assert(1 <= r.backoffset && r.backoffset <= 32768);
		 assert(memcmp(source + index, source + index - r.backoffset, r.length) == 0);
		 index += r.length;
	 }
	 assert(index == byteCount);
 }
#else

 void Encoder::AuditRecords(const uint8_t *, int)
 {
 }
#endif


 int Encoder::WriteBlock2Pass(const uint8_t * source, int byteCount, bool final)
 {  
	 validRecords = 0;
	 comprecords.resize(maxRecords);
	  
	 int targetLength = std::max(byteCount - maxLength, 0);
	 int length = 0;

	 while (targetLength > 0 && validRecords < maxRecords)
	 {
		 int batchLength = std::min(16384, targetLength);
		 int bytesAdded = FirstPass(source, length, batchLength);
		 length += bytesAdded;
		 targetLength -= bytesAdded;
	
		 AuditRecords(source, length);

 	 }
  
	if (targetLength <= 0 && validRecords < maxRecords)
	{
		comprecords[validRecords++] = { safecast(byteCount - length), 0,0 };
		//memcpy(tempBuffer, source + bytesAdded - 32768, 32768 + maxLength);	 
		//int restLength = FirstPass(tempBuffer + 32768, maxLength);
		length += byteCount - length;

		AuditRecords(source, length);

	}
 	   

	 FixHashTable(length);

	 auto symbolFreqs = std::vector<int>(286, 0);
	 auto distanceFrequencies = std::vector<int>(30, 0);

	 GetFrequencies(source, symbolFreqs, distanceFrequencies);

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
	 if (available < requiredLength)
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

	 WriteRecords(source);

	 stream.AppendToBitStream(codes[256]);

	 return length;
 }

int Encoder::UncompressedFallback(int length, const uint8_t * source, bool final)
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


 inline void Encoder::FixHashTable(int offset)
 {
	 for (int i = 0; i < hashSize; ++i)
	 {
		 hashtable[i] = hashtable[i] - offset;
	 }

 }

 inline int Encoder::WriteBlockFixedHuff(const uint8_t * source, int byteCount, int final)
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

		 if (unsigned(distance) <= maxDistance)
		 {
			 auto matchLength = countMatches(sourcePtr, sourcePtr - distance, safecast(bytesToEncode - i));

			 if (matchLength > 3)
			 {
				 stream.AppendToBitStream(lcodes_f[matchLength]);
				 WriteDistance(dcodes_f, distance);

				 i += matchLength - 1;
				 continue;
			 }
		 }

		 stream.AppendToBitStream(codes_f[*sourcePtr]);
	 }

	 FixHashTable(bytesToEncode);
	 stream.AppendToBitStream(codes_f[256]);
	 return bytesToEncode;
 }

 inline int Encoder::FirstPass(const uint8_t * source, int startPos, int byteCount)
 {
	 if (byteCount == 0)
		 return 0;

	 int end = byteCount + startPos;

	 int backRefEnd = startPos + 1;

	 auto validRecordsStart = validRecords;
	 int j = startPos + 1;

	 while (j < end)
	 {
		 auto s = source + j;
		 auto newHash = CalcHash(s);
		 auto distance = j - hashtable[newHash];
		 hashtable[newHash] = j;

		 if (distance >= maxDistance)
		 {
			 j++;
			 continue;
		 }
 
		 int matchStart = j;
      
		 auto delta = *(compareType*)s ^ *(compareType*)(s-distance);
		  
		 auto matchLength = (delta != 0)
			 ? ZeroCount(delta)
			 : remain(s, s - distance, sizeof(compareType));

		 int lengthBackward = countMatchBackward(s, s - distance, matchStart - backRefEnd);

	     matchLength = matchLength + lengthBackward;
		 
		 if (matchLength < 4)
		 {
			 j++;
			 continue;
		 }


		 matchStart -= lengthBackward;

		 if (matchLength > 258)
		 {
			 matchLength = 258;
		 }
 
		 AddHashEntries(source, matchStart + 1, matchLength);

		 comprecords[validRecords++] = { safecast(matchStart - backRefEnd), safecast(distance), safecast(matchLength) };

		 backRefEnd = matchStart + matchLength;

		 j = backRefEnd + 1;

		 if (validRecords == maxRecords)
		 { 
			 end = 0;
			 break; 
		 }	 
	 }

	 comprecords[validRecordsStart].literals += 1;

	 if (backRefEnd > end)
 		 return backRefEnd - startPos;
 
	 comprecords[validRecords++] = { safecast(end - backRefEnd), 0,0 };
 	 return end - startPos;
 }

 void Encoder::GetFrequencies(const uint8_t * source,  std::vector<int> & symbolFreqs, std::vector<int> & distanceFrequencies)
 {

	 int index = 0;
	 for (int n = 0; n < validRecords; ++n)
	 {
		 auto r = comprecords[n];

		 for (unsigned i = 0; i < r.literals; i++)
		 {
			 symbolFreqs[source[index + i]]++;
		 }
		 index += r.literals;

		 if (r.length == 0)
			 continue;

		 assert(1 <= r.backoffset && r.backoffset <= 32768);
		 assert(3 <= r.length && r.length <= 258);

		 for (int i = index; i < index + r.length; i++)
		 {
			 assert(source[i] == source[i - r.backoffset]);
		 }
		 symbolFreqs[lengthTable[r.length].code]++;
		 distanceFrequencies[distanceLut[r.backoffset]]++;
		 index += r.length;
	 }

	 symbolFreqs[256]++;
}
 

 void Encoder::AddHashEntries(const uint8_t * source, int i, int extra)
 {
	 for (int n = i ; n < i + extra; ++n)
	 {
		 hashtable[CalcHash(source + n )] = n;
	 }
 }

 int Encoder::WriteUncompressedBlock(const uint8_t* source, int byteCount, int final)
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



 int Encoder::WriteDeflateBlock(const uint8_t * source, int inputLength, bool final)
 {
	 if (level == 0)
	 {
		 return WriteUncompressedBlock(source, inputLength, final);
	 }
	 else if (level == 1)
	 {
		 return WriteBlockFixedHuff(source, inputLength, final);
	 }
	 else if (level >= 2)
	 {
		 if (inputLength > 500000)
		 {
			 inputLength = 500000;
			 final = false;
		 }
		 return WriteBlock2Pass(source, inputLength, final);
	 }

	 return -1;
 }

 Encoder::Encoder(int level, uint8_t * outputBuffer, int64_t bytes) :
	 stream(outputBuffer, bytes),
	 level(level)
 {
	 for (auto& h : hashtable)
	 {
		 h = -100000;
	 }
 }

 bool Encoder::AddData(const uint8_t* start, const uint8_t* end, bool final)
 {
	 int64_t total = 0;
	 while (start != end)
	 {
		 auto bytesRead = WriteDeflateBlock(start, safecast(end - start), final);
		 if (bytesRead <= 0)
			 return false;

		 total += bytesRead;
		 start += bytesRead;
	 }
	 return true;
 }


 bool Encoder::AddDataGzip(const uint8_t* start, const uint8_t* end, uint32_t& adler, bool final)
 {
	 int64_t total = 0;
	 while (start != end)
	 {
		 auto bytesRead = WriteDeflateBlock(start, safecast(end - start), final);
		 if (bytesRead <= 0)
			 return false;

		 total += bytesRead;
		 adler = adler32x(adler, start, bytesRead);
		 start += bytesRead;
	 }
	 return true;
 }
