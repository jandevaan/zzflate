
#pragma once
#include <functional>
#include "huffman.h"

#include <intrin.h>

namespace
{
	const char order[] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

	const int hashBits = 16;
	const unsigned hashSize = 1 << hashBits;
	const unsigned hashMask = hashSize -1;
} 

struct record
{
	int frequency;
	int id;
};


struct lengthRecord
{
	short code;
	char extraBits;
	char extraBitLength;
};


struct distanceRecord
{
	unsigned char code;
	unsigned char bits;
	unsigned short distanceStart;
};



extern lengthRecord  lengthTable[259];

extern code  lengthCodes[259];

extern const distanceRecord distanceTable[32];
extern const int distanceTable2[32];
extern unsigned char distanceLut[32769];

class greater
{
public:
	bool operator()(const record _Left, const record _Right) const
	{
		return _Left.frequency > _Right.frequency;
	}
};
 
enum CurrentBlockType
{
	Uncompressed = 0b00,
	FixedHuffman = 0b01,
	UserDefinedHuffman
};
 
struct compressionRecord
{
	int literals;
	unsigned short backoffset;
	unsigned short length;
};


struct EncoderState
{
	outputbitstream stream;

	const unsigned char* source; 
	size_t bufferLength;

	CurrentBlockType _type;
	int _level;

	
	std::vector<compressionRecord> comprecords;

	
	scode codes[288]; // literals
	code lcodes[259]; // codes to send lengths (symbol + extra bits for all 258)
	code dcodes[32];
	int hashtable[hashSize];

	EncoderState(int level, unsigned char* outputBuffer)
		: stream(outputBuffer),
		_level(level)
	{

		for(auto& h : hashtable)
		{
			h = -100000;
		}
		if (level == 0)
		{
			_type = Uncompressed;
		}
		if (level == 1)
		{
			_type = FixedHuffman;
			InitFixedHuffman();
			comprecords.reserve(10000);
		}
		else  
		{
			_type = UserDefinedHuffman;
			comprecords.reserve(10000);
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

	 

	void InitFixedHuffman()
	{
		auto buffer = huffman::generate(huffman::defaultTableLengths());

	 	for (int i = 0; i < 288; ++i)
		{
			codes[i] = { (char)buffer[i].length, (unsigned short)buffer[i].bits };
		}
		

		for (int i = 0; i < 32; ++i)
		{
			dcodes[i] = { 5, (unsigned short)huffman::reverse(i, 5) };
		}

		for (int i = 0; i < 259; ++i)
		{
			auto lengthRecord = lengthTable[i];

			auto code = codes[lengthRecord.code];
			lcodes[i] = {
				code.length + lengthRecord.extraBitLength,
				(lengthRecord.extraBits << code.length) | (unsigned int)code.bits };
		}
 
	}

	void WriteDistance(int offset)
	{
		auto bucketId = distanceLut[offset];
		stream.AppendToBitStream(dcodes[bucketId]);

		auto bucket = distanceTable[bucketId];
		stream.AppendToBitStream(offset - bucket.distanceStart, bucket.bits);
	}
  

	void StartBlock(CurrentBlockType type, int final)
	{
		stream.AppendToBitStream(final, 1); // final
		stream.AppendToBitStream(type, 2); // fixed huffman table		

		_type = type;		
	}
	 
	

	struct treeItem
	{
		int frequency;
		int left;
		int right;
		int bits;
	};


	static std::vector<int> Lengths(const std::vector<treeItem>& tree )
	{

		std::vector<int> lengths = std::vector<int>();

		for (auto t : tree)
		{
			if (t.right != ~0)
				break;

			if (lengths.size() <= t.left)
			{
				lengths.resize(t.left + 1);
			}
			
			lengths[t.left] = t.bits;
		}

		return lengths;
	}

	static int CalculateTree(const std::vector<int>& symbolFreqs, int minFreq, std::vector<treeItem>& tree)
	{
		tree.clear();
		std::vector<record> records;
		for (int i = 0; i < symbolFreqs.size(); ++i)
		{
			if (symbolFreqs[i] == 0)
			{
				tree.push_back({ 0, i, -1 });
				continue;
			}

			int freq =  std::max(minFreq, symbolFreqs[i]);
			tree.push_back({ freq, i, -1 });			 
			record rec = { freq, i };
			records.push_back(rec);
			
		}

		greater pred = greater();

		std::make_heap(records.begin(), records.end(), pred);

		while (records.size() >= 2)
		{
			pop_heap(records.begin(), records.end(), pred);
			record a = records.back();
			records.pop_back();
			
			pop_heap(records.begin(), records.end(), pred);
			record b = records.back();
			records.pop_back();

			auto sumfreq = a.frequency + b.frequency;
			tree.push_back({ sumfreq, a.id, b.id });

			records.push_back({ sumfreq, (int)(tree.size() - 1) });
			push_heap(records.begin(), records.end(), pred);
		}

		int maxLength = 0;
		for (auto i = tree.size() - 1; i != 0; --i)
		{
			auto item = tree[i];
			if (item.right == -1)
			{
				maxLength = std::max(maxLength, item.bits);
				continue;
			}
			tree[item.left].bits = item.bits + 1;
			tree[item.right].bits = item.bits + 1;
		}
		return maxLength;
	}

	static std::vector<int> calcLengths(const std::vector<int>& symbolFreqs, int maxLength)
	{
		int minFreq = 0;
		std::vector<treeItem> tree;
		while (true)
		{
			int max = CalculateTree(symbolFreqs, minFreq, tree);

			if (max <= maxLength)
				return Lengths(tree);
		    
			int total = 0;
			for(auto n : symbolFreqs)
			{
				total += n;
			}
			
			minFreq += total / (1 << maxLength);
		}
	}





	int FindBackRef(int index, int end, int* offset)
	{
		if (index == 0)
			return 0;

		int max = std::min(end, index + 255);

		auto val = source[index - 1];
		int i = index;
		for (; i < max; ++i)
		{
			if (source[i] != val)
				break;
		}
		i -= index;
		*offset = 1;

		return i;
	}


	void CheckLength(std::vector<compressionRecord> comprecords)
	{
		int count = 0;
		for (auto rec : comprecords)
		{
			count += rec.literals + rec.length;
		}
	}

	void WriteRecords(const unsigned char* source, const std::vector<compressionRecord>& vector, CurrentBlockType type, bool final)
	{

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
				stream.AppendToBitStream(lcodes[r.length].bits, lcodes[r.length].length);
				WriteDistance(r.backoffset);
			}

			offset += r.literals + r.length;
		}
	}


	static std::vector<int> calcMetaLengths(std::vector<int> lengths, std::vector<int> distances)
	{
		std::vector<int> lengthFreqs = std::vector<int>(19, 0);

		for(auto l : lengths)
		{
			lengthFreqs[l] += 1;
		}

		for (auto l : distances)
		{
			lengthFreqs[l] += 1;
		}

		auto metacodesLengths = calcLengths(lengthFreqs, 7);
		metacodesLengths.resize(19);

		return metacodesLengths;
	}

	struct lenghtRecord
	{
		lenghtRecord(int val, int load)
		{
			value = uint8_t(val);
			payLoad = uint8_t(load);
		}
		uint8_t value;
		uint8_t payLoad;
	};

	void AddRecords( std::vector<lenghtRecord>& vector, int value, int count)
	{
		if (count == 0)
			return;

		if (value == 0)
		{
			while (count >= 3)
			{
				int writeCount = std::min(count, 138);
				count -= writeCount;
				vector.push_back(lenghtRecord( writeCount < 11 ? 17 : 18,  writeCount));
			}			
		}
		else
		{
			vector.push_back({ value, 0 });
			count--;
			while (count >= 3)
			{
				int writeCount = std::min(count, 6);
				count -= writeCount;
				vector.push_back({ 16, writeCount });
			}
		}

		for (int i = 0; i < count; ++i)
		{
			vector.push_back({ value, 0 });
		}

	}

	std::vector<lenghtRecord> FromLengths(const std::vector<int> lengths, std::vector<int>& freqs)
	{
		std::vector<lenghtRecord> records;
		int currentValue = -1;
		int count = 0;
		for (auto n : lengths)
		{
			if (n == currentValue)
			{
				count++;
				continue;
			}

			AddRecords(records, currentValue, count);
			currentValue = n;
			count = 1;
		}

		AddRecords(records, currentValue, count);

		for (lenghtRecord element : records)
		{
			freqs[element.value]++;
		}
		return records;
	}

	void writelengths(const std::vector<lenghtRecord>& vector, const std::vector<code>& codes)
	{
		for(auto c: vector)
		{
			stream.AppendToBitStream(codes[c.value]);
			switch (c.value)
			{
			case 16: stream.AppendToBitStream(c.payLoad - 3, 2); break;
			case 17: stream.AppendToBitStream(c.payLoad - 3, 3); break;
			case 18: stream.AppendToBitStream(c.payLoad - 11, 7); break;
			default: break;
			}
		}
	}

	void BuildLengthCodesCache()
	{
		for (int i = 0; i < 259; ++i)
		{
			auto lengthRecord = lengthTable[i];

			auto code = codes[lengthRecord.code];
			lcodes[i] = {
				code.length + lengthRecord.extraBitLength,
				(lengthRecord.extraBits << code.length) | (unsigned int)code.bits };
		}
	}

    void DetermineAndWriteCodes(const std::vector<int>& symbolFreqs, const std::vector<int>& distanceFrequencies)
	{ 
		std::vector<int> metaCodeFrequencies(19, 0);

		auto symbolLengths = calcLengths(symbolFreqs, 15);
		auto symbolMetaCodes = FromLengths(symbolLengths, metaCodeFrequencies);
		
		auto distLengths = calcLengths(distanceFrequencies, 15);
		auto distMetaCodes = FromLengths(distLengths, metaCodeFrequencies);
		
		std::vector<int> metacodesLengths = calcLengths(metaCodeFrequencies, 7);
		auto metaCodes = huffman::generate(metacodesLengths);

		// write the codes

		stream.AppendToBitStream(int32_t(symbolLengths.size() - 257), 5);
		stream.AppendToBitStream(int32_t(distLengths.size() - 1), 5); // distance code count
		stream.AppendToBitStream(int32_t(metacodesLengths.size() - 4), 4);
		
		for (int i = 0; i < 19; ++i)
		{
			stream.AppendToBitStream(metacodesLengths[order[i]], 3);
		}

		writelengths(symbolMetaCodes, metaCodes);
		writelengths(distMetaCodes, metaCodes);
 
		// update code tables 

		auto symbolCodes = huffman::generate(symbolLengths);
		 
		for (int i = 0; i < 286; ++i)
		{
			codes[i] = { (char)symbolCodes[i].length, (unsigned short)symbolCodes[i].bits };
		}

		auto distcodes = huffman::generate(distLengths);
		std::copy(distcodes.begin(), distcodes.end(), dcodes);

		BuildLengthCodesCache();
	}

	 static unsigned int CalcHash(const unsigned char * source )
	{
		uint32_t* source32 = (uint32_t*)source;
		auto val = (*source32 >> 8) * 0x00d68664u;
  
		return val >> (32 - hashBits);;
	}
	 
	int remain(const unsigned char* a, const unsigned char* b, int matchLength, unsigned maxLength)
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

		return (int)maxLength;
	}

    __forceinline int countMatches(int i, int offset)
	{
		auto a = source + i;
		auto b = source + offset;

		int matchLength = 0;

		auto maxLength = bufferLength - i;

		typedef uint64_t compareType;

		if (maxLength >= (sizeof(compareType)))
		{
			matchLength = sizeof(compareType);

			auto delta = *(compareType*)a ^ *(compareType*)b;

			unsigned long index;
			if (_BitScanForward64(&index, delta)!= 0)
				return index >> 3;
		}
		
		return remain(a, b, matchLength, maxLength);
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
		bufferLength = byteCount;

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
				auto matchLength = countMatches(i, offset);
				if (matchLength >= 3)
				{
					stream.AppendToBitStream(lcodes[matchLength].bits, lcodes[matchLength].length);
					WriteDistance(delta);

					i += matchLength - 1;					 
					continue;
				}
			}

			auto code = codes[*sourcePtr];
			stream.AppendToBitStream(code.bits, code.length);
		}

		FixHashTable(byteCount);

		stream.AppendToBitStream(codes[256]);

		return (int)byteCount;
	}

 
	int WriteBlock2Pass(int byteCount, bool final)
	{
		bufferLength = byteCount;
		auto length = (byteCount < 65536 && final) ? byteCount : std::min(256000, byteCount - 258);
		comprecords.clear();

		auto symbolFreqs = std::vector<int>(286,0);
		
		auto distanceFrequencies = std::vector<int>(30, 1);		
		int backRefEnd = 0;	
		unsigned int hashSeed = 0;
		 
		for (int i = 0; i < length; ++i)
		{
			auto newHash = CalcHash(source + i);
			int offset = hashtable[newHash];
			hashtable[newHash] = i;
			 
			if (i - 32768 < offset)
			{
				auto matchLength = countMatches(i, offset);

				if (matchLength >= 3)
				{
					symbolFreqs[lengthTable[matchLength].code]++;
					comprecords.push_back({ (int)(i - backRefEnd), (unsigned short)(i - offset), (unsigned short)matchLength });
					int nextI = i + matchLength - 1;
					while (i < nextI)
					{
						hashtable[CalcHash(source + i)] = i;
						i++;
					}

				
					backRefEnd = i + 1;

					continue;
				}
			}
			
			symbolFreqs[source[i]]++;
		}

		length = std::max(length, backRefEnd);
		
		symbolFreqs[256]++;
		
		if (length< byteCount)
		{
			final = 0;
		}

		comprecords.push_back({(int)(length - backRefEnd), 0,0});

		FixHashTable(length);
		 
		StartBlock(_type, final);

		if (_type == UserDefinedHuffman)
		{
			DetermineAndWriteCodes(symbolFreqs, distanceFrequencies);
		}

		WriteRecords(source, comprecords, _type, final);

		stream.AppendToBitStream(codes[256]);

		return (int)length;
	}
 



	int writeUncompressedBlock(int byteCount, int final)	
	{
		uint16_t length = std::min(byteCount, 0xFFFF);
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
			return writeUncompressedBlock(length, final);
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
			auto bytesWritten = WriteDeflateBlock((int)(end - source), true);
			adler = adler32x(adler, source, bytesWritten);
			source += bytesWritten;
		}
	}
	
};
