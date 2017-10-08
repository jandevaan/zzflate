#include <cassert> 
#include <memory>

#include "encoder.h"

#include "zzflate.h"

lengthRecord  Encoder::lengthTable[259];

code Encoder::codes_f[286]; // literals
code Encoder::lcodes_f[259]; // table to send lengths (symbol + extra bits for all 258)
code Encoder::dcodes_f[30];
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
	int offset = 3;

	lengthTable[0] = {};
	lengthTable[1] = {};
	lengthTable[2] = {};

	for (int i = 0; i < 28; ++i)
	{
		int calccode = 257 + i;
		int bits = extraLengthBits[calccode];

		for (int j = 0; j < (1 << bits); ++j)
		{
			lengthTable[offset++] = { safecast(calccode), safecast(j), safecast(bits) };
		}
	}

	lengthTable[258] = { 285, 0,0 };

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

	 if (maxLength >= (sizeof(compareType)))
	 {
		 matchLength = sizeof(compareType);

		 auto delta = *(compareType*)a ^ *(compareType*)b;

		 if (delta != 0)
			 return ZeroCount(delta);
	 }

	 return remain(a, b, matchLength, int(maxLength));
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

 void Encoder::WriteRecords(const uint8_t * src, const std::vector<compressionRecord>& vector)
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

 int Encoder::WriteBlock2Pass(const uint8_t * source, int byteCount, bool final)
 { 
	 int length = (level == 2)
		 ? FirstPass(source, byteCount)
		 : FirstPass2(source, byteCount);

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

	 WriteRecords(source, comprecords);

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

			 if (matchLength >= 3)
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

 inline int Encoder::FirstPass(const uint8_t * source, int byteCount)
 {
	 int length = std::min(byteCount, 256000);
	 comprecords.resize(maxRecords);

	 int backRefEnd = 0;
	 int recordCount = 0;

	 for (int j = 1; j < length; ++j)
	 { 
		 auto newHash = CalcHash(source + j);
		 auto distance = j - (hashtable[newHash]);
		
		 if (distance >= maxDistance)
		 {
			 hashtable[newHash] = j;
			 continue;
		 } 

		 int matchStart = j;

		 auto matchLength = countMatches(source + matchStart, source + matchStart - distance, safecast(byteCount - matchStart));

		 int lengthBackward = countMatchBackward(source + matchStart, source + matchStart - distance, matchStart - backRefEnd);

		 if (lengthBackward > 0)
		 {
			 if (distance == 1)
			 {
				 lengthBackward = 1;
			 }

			 matchLength = std::min(matchLength + lengthBackward, 258);

			 matchStart -= lengthBackward;

		 }

		 hashtable[newHash] = j;

		 if (matchLength < 4)
			 continue;

		 AddHashEntries(source, matchStart + 1, matchLength);

		 comprecords[recordCount++] = { safecast(matchStart - backRefEnd), safecast(distance), safecast(matchLength) };

		 backRefEnd = matchStart + matchLength;

		 if (recordCount == maxRecords - 1)
		 {
			 length = backRefEnd;
			 break;
		 }

		 j = matchStart + matchLength;
	 }

	 length = std::max(length, backRefEnd);
	  
	 comprecords[recordCount++] = { safecast(length - backRefEnd), 0,0 };
	 comprecords.resize(recordCount);
	 
	 return length;
 }

 void Encoder::GetFrequencies(const uint8_t * source,  std::vector<int> & symbolFreqs, std::vector<int> & distanceFrequencies)
 {

	 int index = 0;
	 for (auto r : comprecords)
	 {
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


 inline int Encoder::FirstPass2(const uint8_t * source, int byteCount)
 {
	 int length = std::min(byteCount, 256000);
	 comprecords.resize(maxRecords);

	 int backRefEnd = 0;
	 int recordCount = 0;

	 for (int j = 1; j < length; ++j)
	 {
		 auto newHash = CalcHash(source + j);
		 auto distance = j - (hashtable[newHash]);
		 hashtable[newHash] = j;

		 if (distance >= maxDistance)
			 continue;

		 int matchStart = j;

		 auto matchLength = countMatches(source + matchStart, source + matchStart - distance, safecast(byteCount - matchStart));
 		 while (true)
		 {
 			 int altJ = j + matchLength -1;
			 auto altDistance = altJ - (hashtable[CalcHash(source + altJ)]);
			 if (altDistance <=0  || altDistance >= maxDistance)
			 {
				 altJ = j + matchLength - 2;
				 altDistance = altJ - (hashtable[CalcHash(source + altJ)]);
			 }

			 if (0 < altDistance && altDistance < maxDistance)
			 {
				 auto matchLength2 = countMatches(source + matchStart, source + matchStart - altDistance, safecast(byteCount - matchStart));
				 if (matchLength2 > matchLength)
				 {
					 distance = altDistance;
					 matchLength = matchLength2;
					 continue;
				 }
			 }
			 break;
		 } 
		
		 int lengthBackward = countMatchBackward(source + matchStart, source + matchStart - distance, matchStart - backRefEnd);

		 if (lengthBackward > 0)
		 {
			 if (distance == 1)
			 {
				 lengthBackward = 1;
			 }

			 matchLength = std::min(matchLength + lengthBackward, 258);

			 matchStart -= lengthBackward;

		 }

		 if (matchLength < 4)
			 continue;

		 AddHashEntries(source, matchStart + 1, matchLength);

		 comprecords[recordCount++] = { safecast(matchStart - backRefEnd), safecast(distance), safecast(matchLength) };

		 backRefEnd = matchStart + matchLength;

		 if (recordCount == maxRecords - 1)
		 {
			 length = backRefEnd;
			 break;
		 }

		 j = matchStart + matchLength;
	 }

	 length = std::max(length, backRefEnd);
	 comprecords[recordCount++] = { safecast(length - backRefEnd), 0,0 };
	 comprecords.resize(recordCount);

	 return length;
 }


 inline void Encoder::AddHashEntries(const uint8_t * source, int i, int extra)
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
