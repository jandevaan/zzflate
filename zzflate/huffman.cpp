
#include "huffman.h"

#include <vector> 
#include <algorithm>
#include "outputbitstream.h"

namespace
{
	
	unsigned int reverse(unsigned int x)
	{
		x = (((x & 0xaaaaaaaa) >> 1) | ((x & 0x55555555) << 1));
		x = (((x & 0xcccccccc) >> 2) | ((x & 0x33333333) << 2));
		x = (((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4));
		x = (((x & 0xff00ff00) >> 8) | ((x & 0x00ff00ff) << 8));
		return((x >> 16) | (x << 16));

	}
}
 
unsigned huffman::reverse(unsigned value, int len)
{
	auto mask = ~((1 << len) - 1);

	if ((value & mask) != 0)
	{
		assert(false);
	}
	

	return ::reverse(value) >> (32 - len);
}
 
std::vector<int> huffman::defaultTableLengths()
{
	std::vector<int> lengths(288);

	for (int i = 0; i < 288; ++i)
	{
		if (i <= 143 || i >= 280)
		{
			lengths[i] = 8;
			continue;
		}

		lengths[i] = i <= 255 ? 9 : 7;		
	}

	return lengths;
}



class greater
{
public:
	bool operator()(const record a, const record b) const
	{
		return a.frequency > b.frequency;
	}
};




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

		int freq = std::max(minFreq, symbolFreqs[i]);
		tree.push_back({ freq, i, -1 });
		record rec = { freq, i };
		records.push_back(rec);

	}

	greater pred = greater();

	make_heap(records.begin(), records.end(), pred);

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

		records.push_back({ sumfreq, safecast(tree.size() - 1) });
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

void CalcLengths(const std::vector<int>& symbolFreqs, std::vector<int>& lengths, int maxLength)
{
	int minFreq = 0;
	std::vector<treeItem> tree;
	tree.reserve(2 * symbolFreqs.size());

	while (true)
	{
		int max = CalculateTree(symbolFreqs, minFreq, tree);

		if (max <= maxLength)
		{
			lengths.resize(symbolFreqs.size());

			for (auto t : tree)
			{
				if (t.right != ~0)
					break;
				 
				lengths[t.left] = t.frequency == 0 ? 0 : std::max(1,t.bits);
			}
			return;
		}

		int total = 0;
		for (auto n : symbolFreqs)
		{
			total += n;
		}

		minFreq += std::max(1,total / (1 << maxLength));
	}
}



void AddRecords(std::vector<lenghtRecord>& vector, int value, int count)
{
	if (count == 0)
		return;

	if (value == 0)
	{
		while (count >= 3)
		{
			int writeCount = std::min(count, 138);
			count -= writeCount;
			vector.push_back(lenghtRecord(writeCount < 11 ? 17 : 18, writeCount));
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
