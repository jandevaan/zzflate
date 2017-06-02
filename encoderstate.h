
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
		comprecords.reserve(3000);
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

	void WriteDistance(outputbitstream& stream, int offset)
	{
		auto i = FindDistance(offset);

		auto bucket = distanceTable[i];
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


	static std::vector<int> Lengths(const std::vector<treeItem>& tree, int maxLength)
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

	std::vector<int> calcLengths(const std::vector<int>& freqsx, int maxLength)
	{
		long long symbolcount = 0;
		
		for (auto f : freqsx)
		{
			symbolcount += f;
		}

		// cheap hack to prevent excessively long codewords.
		int limit = symbolcount / (1 << 15);
		std::vector<record> records;
		std::vector<treeItem> tree;
		for (int i = 0; i < freqsx.size(); ++i)
		{
			int freq = freqsx[i] != 0 ? std::max(limit, freqsx[i]) : 0;
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

		std::vector<int> lengthCounts(30, 0);
		
		for (int i = tree.size() - 1; i > 0; --i)
		{
			auto item = tree[i];
			if (item.right == -1)
			{
				lengthCounts[item.bits]++;
				continue;
			}
				
			tree[item.left].bits = item.bits + 1;
			tree[item.right].bits = item.bits + 1;
		}

		return Lengths(tree, 15);
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
				 
		stream.AppendToBitStream(symLengths.size() - 257, 5);
		stream.AppendToBitStream(distLengths.size() - 1, 5); // distance code count
		stream.AppendToBitStream(metacodesLengths.size() - 4, 4);

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

	std::vector<unsigned short> hashtable = std::vector<unsigned short>(hashSize, ~0);

	const unsigned char* source;
	size_t length;

	unsigned static int CalcHash(const unsigned char * source )
	{
		return (source[0] * 0x102u) ^ (source[1] * 0xF00Fu) ^ (source[2] * 0xFFu);
	}

	int matchOffset(unsigned iu, unsigned short oldVal)
	{ 
		int i = iu;
		auto offset =  (i & ~0x7FFF) + oldVal;
		
		unsigned val = offset >= i;

		return offset - val * 0x8000;

	}

    int countMatches(int i, int offset)
	{
		auto a = source + i;
		auto b = source + offset;

		int limit = std::min(258, (int)(length - i));
		int matchLength = 0;
		
		for (; matchLength < limit; ++matchLength)
		{
			if (a[matchLength] != b[matchLength])
				return matchLength;
		}

		return limit;
	}


	void WriteBlockFixedHuff(CurrentBlockType type, int final)
	{ 
		auto pHash = &hashtable[0]; 
		
		StartBlock(type, final);

		for (auto i = 0u; i < length; ++i)
		{
			auto newHash = CalcHash(source + i) & hashMask;
			unsigned oldVal = pHash[newHash];
			pHash[newHash] = i & 0x7FFF;
			if (oldVal > 0x7FFF)
			{
				stream.AppendToBitStream(codes[source[i]]);
				continue;
			} 
			auto offset = matchOffset(i, oldVal);

			int matchLength = countMatches(i, offset);

			if (matchLength < 3)
			{
				stream.AppendToBitStream(codes[source[i]]);
				continue;
			}
			stream.AppendToBitStream(lcodes[matchLength]);
			WriteDistance(stream, i - offset);

			i += matchLength - 1; 
		}
		  
	}
	 
	std::vector<compressionRecord> comprecords;

	void WriteBlockV(CurrentBlockType type, int final)
	{

		comprecords.clear();

		auto symbolFreqs = std::vector<int>(286,1);
		auto symbolFrequencies = &symbolFreqs[0];
		auto distanceFrequencies = std::vector<int>(30, 0);
		auto pHash = &hashtable[0];
		unsigned int backRefEnd = 0;	

		for (auto i = 0u; i < length; ++i)
		{
			auto newHash = CalcHash(source + i) & hashMask;
			unsigned oldVal = pHash[newHash];
			pHash[newHash] = i & 0x7FFF; 
			if (oldVal > 0x7FFF)
			{
				symbolFrequencies[source[i]]++;
				continue;;
			}
  
			auto offset = matchOffset(i, oldVal);
			
			int matchLength = countMatches(i, offset);
		 
			if (matchLength <3)
			{
				symbolFrequencies[source[i]]++;
				continue;
			}
			 
		/*	auto newHash2 = CalcHash(i + 1) % hashMask;
			unsigned short old_val2 = pHash[newHash2];
			if (old_val2 >= 0)
			{
				auto offset2 = matchOffset(i + 1, old_val2);
				int matchLength2 = countMatches(i + 1, offset2);

				if (matchLength2 > matchLength)
				{
					symbolFrequencies[source[i]]++;
					continue;
				}
			}*/

			symbolFrequencies[lengthTable[matchLength].code]++;
			distanceFrequencies[FindDistance(i - offset)]++;
			comprecords.push_back({ i - backRefEnd, (unsigned short)(i - offset), (unsigned short)matchLength });
			i += matchLength - 1;
			backRefEnd = i + 1;
		}
		 
		comprecords.push_back({(unsigned int) (length - backRefEnd), 0,0});

		symbolFrequencies[256]++;
		
		StartBlock(type, final);

		if (type == UserDefinedHuffman)
		{
			prepareCodes(symbolFreqs, distanceFrequencies);
		}
		else
		{
			codes = huffman::generate(huffman::defaultTableLengths());
			dcodes = std::vector<code>(32);
			for (int i = 0; i < 32; ++i)
			{
				dcodes[i] = {5, (int)huffman::reverse(i, 5)};
			}
		
		}

		WriteRecords(source, comprecords, type, final);
	}




	void writeUncompressedBlock(int final)
	{
		StartBlock(Uncompressed, final);
		stream.WriteU16(this->length);
		stream.WriteU16(~this->length);
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
