


enum CurrentBlockType
{
	Uncompressed = 0b00,
	FixedHuffman = 0b01,
	UserDefinedHuffman
};
 

void WriteDistance(outputbitstream& stream, int offset)
{
	for (int n = 1; n < std::size(distanceTable); ++n)
	{
		if (offset < distanceTable[n + 1].distanceStart)
		{
			auto bucket = distanceTable[n - 1];
			stream.AppendToBitStream(bucket.code, 5); // fixed 5 bit table
			stream.AppendToBitStream(bucket.distanceStart - offset, bucket.bits);
			return;
		}
	}

	throw std::exception("Distance too far away");
}

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


	std::vector<char> Lengths(std::vector<treeItem> tree)
	{
		std::vector<char> lengths = std::vector<char>();

		for (auto t : tree)
		{
			if (t.right != ~0)
				break;

			lengths.push_back(t.bits);
		}

		return lengths;
	}

	std::vector<char> calcLengths(const std::vector<int>& freqsx)
	{
		//long long symbolcount = 0;
		//
		//for (auto f : freqsx)
		//{
		//	symbolcount += f;
		//}

		//// cheap hack to prevent excessively long codewords.
		//int limit = symbolcount / (1 << 15);
		std::vector<record> records;
		std::vector<treeItem> tree;
		for (int i = 0; i < freqsx.size(); ++i)
		{
			if (freqsx[i] == 0)
				continue;

			int freq = freqsx[i];// != 0 ? std::max(limit, freqsx[i]) : 0;
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

		//
		unsigned int backRefEnd = 0;		
		//for (int i = 0; i < sourceLen; ++i)
		//{
		//	int offset;
		//	//auto matchLength = FindBackRef(source, i, sourceLen, &offset);
		//	if (true)
		//	{
		//		freqs[source[i]]++;				
		//		continue;
		//	}

		///*	freqs[lengthTable[matchLength].code]++;

		//	comprecords.push_back({ i - backRefEnd, (unsigned short)offset, (unsigned short)matchLength });
		//	i += matchLength - 1;
		//	backRefEnd = i + 1;*/
		//}
		// 
		comprecords.push_back({(unsigned int) (sourceLen - backRefEnd), 0,0});

		//freqs[256]++;
		//auto freqs = std::vector<int>(280,1);
		StartBlock(type, final);

		if (type == UserDefinedHuffman)
		{
			auto lengths = std::vector<char>(255, 8);
			lengths.push_back(9);
			lengths.push_back(9);

			auto distances = std::vector<char>() = {0};

			
			auto metacodesLengths = std::vector<char>() = { 
				2,0,0,0, 
				0,0,0,0, 
				1,2 , 0, 0, 
				0, 0, 0, 0, 
				0, 0, 0 };
	
			auto order = std::vector<char>() = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
			
			
			stream.AppendToBitStream(lengths.size() - 257, 5);
			stream.AppendToBitStream(distances.size() - 1, 5); // distance code count
			stream.AppendToBitStream(metacodesLengths.size() - 4, 4);

			for (int i = 0; i < 19; ++i)
			{
				stream.AppendToBitStream(metacodesLengths[order[i]], 3);
			}

			auto metaCodes = huffman::generate(metacodesLengths);

			for (auto len : lengths)
			{
				stream.AppendToBitStream(metaCodes[len]);
			}
			
			for (auto d : distances)
			{
				stream.AppendToBitStream(metaCodes[d]);
			}

			codes = huffman::generate(lengths);
		}
		else
		{
			codes = huffman::generate(huffman::defaultTableLengths());
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
	int _level;
};
