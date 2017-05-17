namespace
{
	const char order[] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
}


enum CurrentBlockType
{
	Uncompressed = 0b00,
	FixedHuffman = 0b01,
	UserDefinedHuffman
};
 

void WriteLength(outputbitstream& stream, std::vector<code>& codes, int runLength)
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

	}



	void WriteDistance(outputbitstream& stream, int offset)
	{
		for (int n = 1; n < std::size(distanceTable); ++n)
		{
			if (offset < distanceTable[n + 1].distanceStart)
			{
				auto bucket = distanceTable[n - 1];
				stream.AppendToBitStream(dcodes[bucket.code]);  
				stream.AppendToBitStream(bucket.distanceStart - offset, bucket.bits);
				return;
			}
		}

		throw std::exception("Distance too far away");
	}

	void StartBlock(CurrentBlockType type, int final)
	{
		stream.AppendToBitStream(final, 1); // final
		stream.AppendToBitStream(type, 2); // fixed huffman table		

		_type = type;
		if (_type == FixedHuffman)
		{
			//		codes = huffman::generate(huffman::defaultTableLengths());
		}
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


	std::vector<int> Lengths(std::vector<treeItem> tree)
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

	std::vector<int> calcLengths(const std::vector<int>& freqsx)
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
			record rec = { freq, i };
			records.push_back(rec);
			tree.push_back({ rec.frequency, rec.id, -1 });
		}

		std::sort(records.begin(), records.end(),
			[](auto a, auto b) -> bool { return a.frequency > b.frequency; });

		auto nonzero = std::count_if(records.begin(), records.end(), [](record r) -> bool { return r.frequency != 0; });

		records.resize(nonzero);

		while (records.size() >= 2)
		{
			auto a = records[records.size() - 2];
			auto b = records[records.size() - 1];
			records.resize(records.size() - 2);

			auto sumfreq = a.frequency + b.frequency;
			tree.push_back({ sumfreq, a.id, b.id });

			record val = { sumfreq, (int)(tree.size() - 1) };
 
			records.push_back(val);

			std::sort(records.begin(), records.end(),
				[](auto a, auto b) -> bool { return a.frequency > b.frequency; });

		}

		for (int i = tree.size() - 1; i > 0; --i)
		{
			auto item = tree[i];
			if (item.right == -1)
				continue;

			tree[item.left].bits = item.bits + 1;
			tree[item.right].bits = item.bits + 1;
		}

		return Lengths(tree);
	}


	int FindBackRef(const unsigned char* source, int index, int end, int* offset)
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

	

	void WriteBlockV(const unsigned char* source, size_t sourceLen, CurrentBlockType type, int final)
	{

		auto comprecords = std::vector<compressionRecord>();
		auto freqs = std::vector<int>(286,1);
		
		unsigned int backRefEnd = 0;		
		for (int i = 0; i < sourceLen; ++i)
		{
			int offset;
			auto matchLength = FindBackRef(source, i, sourceLen, &offset);
			if (matchLength < 3)
			{
				freqs[source[i]]++;				
				continue;
			}

			freqs[lengthTable[matchLength].code]++;

			comprecords.push_back({ i - backRefEnd, (unsigned short)offset, (unsigned short)matchLength });
			i += matchLength - 1;
			backRefEnd = i + 1;
		}
		 
		comprecords.push_back({(unsigned int) (sourceLen - backRefEnd), 0,0});

		freqs[256]++;
		
		StartBlock(type, final);

		if (type == UserDefinedHuffman)
		{
			auto lengths = calcLengths(freqs);
			auto distances = std::vector<int> {1};

			std::vector<int> lengthFreqs = std::vector<int>(19,0);

			for(auto l : lengths)
			{
				lengthFreqs[l] = 1;
			}

			for (auto l : distances)
			{
				lengthFreqs[l] = 1;
			}
			auto metacodesLengths = calcLengths(lengthFreqs);
			metacodesLengths.resize(19);
				 
			stream.AppendToBitStream(lengths.size() - 257, 5);
			stream.AppendToBitStream(distances.size() - 1, 5); // distance code count
			stream.AppendToBitStream(metacodesLengths.size() - 4, 4);

			for (int i = 0; i < 19; ++i)
			{
				int length = metacodesLengths[order[i]];
				stream.AppendToBitStream(length, 3);
			}

			auto metaCodes = huffman::generate(metacodesLengths);

			for (auto len : lengths)
			{
				auto meta_code = metaCodes[len];
				if (meta_code.length == 0)
				{
					assert(false);
				}
				stream.AppendToBitStream(meta_code);
			}
			
			for (auto d : distances)
			{
				stream.AppendToBitStream(metaCodes[d]);
			}

			codes = huffman::generate(lengths);
			dcodes = huffman::generate(distances);
		}
		else
		{
			codes = huffman::generate(huffman::defaultTableLengths());
			dcodes = std::vector<code>(32);
			for (int i = 0; i < 32; ++i)
			{
				dcodes[i] = {5, i};
			}
		
		}

		WriteRecords(source, comprecords, type, final);
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
	std::vector<code> dcodes;
	int _level;
};
