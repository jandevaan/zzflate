#include <cassert> 
#include <memory>

#include "encoder.h"
 
#include <iostream>
#include "zzflate.h"

const int Encoder::hashBufferLen;

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
		 auto rec = lengthTable[i];
		 lCodes[i] = Merge(symbolCodes[rec.code], code(rec.extraBitLength, rec.extraBits));
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
			 auto delta = *(compareType*)sourcePtr ^ *(compareType*)(sourcePtr - distance);

			 auto matchLength = (delta != 0)
				 ? ZeroCount(delta)
				 : remain(sourcePtr, sourcePtr - distance, sizeof(compareType), safecast(bytesToEncode - i));

			  
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
 
		
		 int lengthBackward = countMatchBackward(s, s - distance, j - backRefEnd);
		 
		 auto delta = *(compareType*)s ^ *(compareType*)(s - distance);
		 int matchLength = lengthBackward;

		 int matchStart = j - lengthBackward;

		 if (delta != 0)
		 {
			 matchLength += ZeroCount(delta);
			 if (matchLength < 4)
			 {
				 j++; 
				 continue;
			 }
		 }
		 else
		 {
			 matchLength += remain(s, s - distance, sizeof(compareType));
			 if (matchLength > 258)
			 {
				 matchLength = 258;
			 }
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
