
#include <functional>


namespace
{
	const char order[] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

	const unsigned hashSize = 0x2000;
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
	int code;
	int bits;
	int distanceStart;
};



extern lengthRecord  lengthTable[259];

extern code  lengthCodes[259];

extern const distanceRecord distanceTable[32];

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
 

inline void WriteLength(outputbitstream& stream, std::vector<code>& codes, int runLength)
{
	auto lengthRecord = lengthTable[runLength];
	auto code = lengthRecord.code;

	stream.AppendToBitStream(codes[code]);
	stream.AppendToBitStream(lengthRecord.extraBits, lengthRecord.extraBitLength);
}

struct EncoderState
{
	EncoderState(int level, unsigned char* outputBuffer)
		: stream(outputBuffer),
		_level(level)
	{
		
		if (level == 1)
		{
			InitFixedHuffman();
		}
		else if (level == 2)
		{
			comprecords.reserve(11000);
		}
	}


	int FindDistance(int offset)
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


	__forceinline int FindDistance2(int offset)
	{
		int n = 0;
		n += (distanceTable[n + 16].distanceStart <= offset) << 4;
		n += (distanceTable[n + 8].distanceStart <= offset) << 3;
		n += (distanceTable[n + 4].distanceStart <= offset) << 2;
		n += (distanceTable[n + 2].distanceStart <= offset) << 1;
		n += (distanceTable[n + 1].distanceStart <= offset);

		if (n < 0 || n > 29)
		{
			assert(false);
		}
		return n ;
	}

	void InitFixedHuffman()
	{
		codes = huffman::generate(huffman::defaultTableLengths());
		dcodes = std::vector<code>(32);
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

	void WriteDistance(outputbitstream& stream, int offset)
	{
		auto bucket = distanceTable[FindDistance2(offset)];
		stream.AppendToBitStream(dcodes[bucket.code]);
		stream.AppendToBitStream(offset - bucket.distanceStart, bucket.bits);
	}

	void StartBlock(CurrentBlockType type, int final)
	{
		stream.AppendToBitStream(final, 1); // final
		stream.AppendToBitStream(type, 2); // fixed huffman table		

		_type = type;		
	}
	 
	struct compressionRecord
	{
		int literals;
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


	static std::vector<int> Lengths(const std::vector<treeItem>& tree, int maxLength)
	{

		for (auto item : tree)
		{
			if (item.bits > maxLength)
			{
				assert(false);
			}
		}


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

	std::vector<int> calcLengths(const std::vector<int>& freqsx, int maxLength)
	{
		long long symbolcount = 0;
		 
		std::vector<record> records;
		std::vector<treeItem> tree;
		for (int i = 0; i < freqsx.size(); ++i)
		{
			int freq = freqsx[i];// != 0 ? std::max(limit, freqsx[i]) : 0;
			tree.push_back({ freq, i, -1 });
			
			if (freqsx[i] == 0)
				continue;

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

		std::vector<int> lengthCounts(maxLength +2, 0);
		
		 
		for (auto i = tree.size() - 1; i != 0; --i)
		{
			auto item = tree[i];
			if (item.right == -1)
			{
				if (item.bits > maxLength)
				{ 
					tree[i].bits = maxLength;
					lengthCounts[maxLength]++;
				}
				else
				{
					lengthCounts[item.bits]++;
				}

				continue;
			}
				
			tree[item.left].bits = item.bits + 1;
			tree[item.right].bits = item.bits + 1;
		}
	    
		int codeSpaceSize = 1 << maxLength;

		for (int i = 1; i <= maxLength; ++i)
		{
			codeSpaceSize -= lengthCounts[i] << (maxLength - i);
		}

		if (codeSpaceSize < 0)
		{
			std::sort(tree.begin(), tree.end(), [](treeItem a, treeItem b) { return a.frequency < b.frequency; });

			for (int n = 0; n < tree.size(); ++n)
			{
				if (tree[n].bits == maxLength)
					continue;

				codeSpaceSize += 1 << (maxLength - tree[n].bits);
				codeSpaceSize -= 1 << maxLength;
				tree[n].bits = maxLength;

				if (codeSpaceSize >= 0)
					break;
			}

		}

		 
		return Lengths(tree, maxLength);
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
				WriteLength(stream, codes, r.length);
				WriteDistance(stream, r.backoffset);
			}

			offset += r.literals + r.length;
		}
	}


	std::vector<int> calcMetaLengths(std::vector<int> lengths, std::vector<int> distances)
	{
		std::vector<int> lengthFreqs = std::vector<int>(19, 0);

		for(auto l : lengths)
		{
			lengthFreqs[l] = 1;
		}

		for (auto l : distances)
		{
			lengthFreqs[l] = 1;
		}

		auto metacodesLengths = calcLengths(lengthFreqs, 7);
		metacodesLengths.resize(19);

		return metacodesLengths;
	}

	void prepareCodes(const std::vector<int>& symbolFreqs, const std::vector<int>& distanceFrequencies)
	{
		auto symLengths = calcLengths(symbolFreqs, 15);
		auto distLengths = calcLengths(distanceFrequencies, 15);

		auto metacodesLengths = calcMetaLengths(symLengths, distLengths); 
				 
		stream.AppendToBitStream(int32_t(symLengths.size() - 257), 5);
		stream.AppendToBitStream(int32_t(distLengths.size() - 1), 5); // distance code count
		stream.AppendToBitStream(int32_t(metacodesLengths.size() - 4), 4);

		for (int i = 0; i < 19; ++i)
		{
			int length = metacodesLengths[order[i]];
			stream.AppendToBitStream(length, 3);
		}

		auto metaCodes = huffman::generate(metacodesLengths);

		for (auto len : symLengths)
		{
			auto meta_code = metaCodes[len];
			if (meta_code.length == 0)
			{
				assert(false);
			}
			stream.AppendToBitStream(meta_code);
		}
			
		for (auto d : distLengths)
		{
			stream.AppendToBitStream(metaCodes[d]);
		}

		codes = huffman::generate(symLengths);
		dcodes = huffman::generate(distLengths);
	}

	std::vector<int> hashtable = std::vector<int>(hashSize,-100000);

	const unsigned char* source;
	size_t length;

	static unsigned int CalcHash(const unsigned char * source )
	{
		return (source[0] * 0x102u) ^ (source[1] * 0xF004u) ^ (source[2] * 0xFFu);
	}

	//int matchOffset(unsigned iu, unsigned short oldVal)
	//{ 
	//	int i = iu;
	//	auto offset =  (i & ~0x7FFF) + oldVal;
	//	
	//	unsigned val = offset >= i;

	//	return offset - (val << 15);
	//	
	//}

    __forceinline  int countMatches(int i, int offset)
	{
		auto a = source + i;
		auto b = source + offset;

		int matchLength = 0;

		int maxLength = length - i;

		if (maxLength >= 4)
		{
			auto delta = *(uint32_t*)a ^ *(uint32_t*)b;

			if (delta != 0)
				return (delta << 8 == 0) ? 3 : 0;

			matchLength = 4;
		}
		
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


	//	

	//	int matchLength = 0;      // +(diff == 0);

	//	while (matchLength <= limit - 4)
	//	{
	//		auto diff = (*wpa) ^ (*wpb);

	//		if (diff != 0)
	//		{
	//			int offset2 = 2 * ((diff & 0xFFFF) == 0);
	//			diff = diff | (diff >> 16);

	//			int offset1 = ((diff & 0xFF) ==0 );

	//			return matchLength + offset2 + offset1;
	//		}
	//		
	//		matchLength += 4;
	//		wpa++;
	//		wpb++;
	//	}
	//	 
	//	return limit;
	//}


	void FixHashTable()
	{
		for (int i = 0; i < hashtable.size(); ++i)
		{
			hashtable[i] = hashtable[i] - (int)length;
		}
	}

	void WriteBlockFixedHuff(int final)
	{  
		StartBlock(FixedHuffman, final);
		
		for (int i = 0; i < length; ++i)
		{
			auto sourcePtr = source + i;
			auto newHash = CalcHash(sourcePtr) & hashMask;
			int offset = hashtable[newHash];
			hashtable[newHash] = i;
			if (i - offset < 32768)
			{
				int matchLength = countMatches(i, offset);

				if (matchLength >= 3)
				{
					stream.AppendToBitStream(lcodes[matchLength].bits, lcodes[matchLength].length);
					WriteDistance(stream, i - offset);

					i += matchLength - 1;
					continue;
				}
			}

			auto code = codes[*sourcePtr];
			stream.AppendToBitStream(code.bits, code.length);
		}

		FixHashTable();

	 

	}
	 
	std::vector<compressionRecord> comprecords;

	void WriteBlockUserHuffman(CurrentBlockType type, int final)
	{
		comprecords.clear();

		auto symbolFreqs = std::vector<int>(286,0);
		
		auto distanceFrequencies = std::vector<int>(30, 1);		
		int backRefEnd = 0;	

		for (int i = 0; i < length; ++i)
		{
			auto newHash = CalcHash(source + i) & hashMask;
			int offset = hashtable[newHash];
			hashtable[newHash] = i;
			 
			if (i - 32768 < offset)
			{
				int matchLength = countMatches(i, offset);

				if (matchLength >= 3)
				{
					symbolFreqs[lengthTable[matchLength].code]++;
					comprecords.push_back({ (int)(i - backRefEnd), (unsigned short)(i - offset), (unsigned short)matchLength });
					i += matchLength - 1;
					backRefEnd = i + 1;
					continue;
				}
			}
			
			symbolFreqs[source[i]]++;
		}

		FixHashTable();
		 
		comprecords.push_back({(int) (length - backRefEnd), 0,0});

		symbolFreqs[256]++;
		
		StartBlock(type, final);

		prepareCodes(symbolFreqs, distanceFrequencies);
		
		WriteRecords(source, comprecords, type, final != 0);
	}




	void writeUncompressedBlock(int final)
	{
		StartBlock(Uncompressed, final);
		stream.WriteU16(int16_t(this->length));
		stream.WriteU16(int16_t(~this->length));
		stream.Flush();

		memcpy(stream._stream, source, this->length);
		stream._stream += this->length;

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
	std::vector<code> dcodes;
	code lcodes[259];
	int _level;
};
