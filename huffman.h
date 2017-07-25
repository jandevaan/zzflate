#pragma once
#include <vector>
#include "config.h"
#include "outputbitstream.h"


struct record
{
	int frequency;
	int id;
};



struct treeItem
{
	int frequency;
	int left;
	int right;
	int bits;
};



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


std::vector<lenghtRecord> FromLengths(const std::vector<int> lengths, std::vector<int>& freqs);
void CalcLengths(const std::vector<int>& symbolFreqs, std::vector<int>& lengths, int maxLength);


namespace  huffman
{
	const int MAX_BITS = 16;

	unsigned reverse(unsigned value, int len); 
	std::vector<int> defaultTableLengths();

	template <class T>
	void generate(const std::vector<int>& lengths, T* codes)
	{
		int bl_count[MAX_BITS] = {};
		for (int i = 0; i < lengths.size(); ++i)
		{
			bl_count[lengths[i]]++;
		}

		unsigned int next_code[MAX_BITS] = {};

		int bits = 0;
		bl_count[0] = 0;
		for (int n = 1; n < MAX_BITS; ++n)
		{
			bits = (bits + bl_count[n - 1]) << 1;
			next_code[n] = bits;
		}
		 
		for (int n = 0; n < lengths.size(); n++)
		{
			int len = lengths[n];
			if (len <= 0)
				continue;

			unsigned reversed = reverse(next_code[len], len);
			if (reverse(reversed, len) != next_code[len])
				break;

			codes[n] = { safecast(len), safecast(reversed) };
			next_code[len]++;
		}		 
	}
};

