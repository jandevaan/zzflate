#include "huffman.h"


namespace
{
	
	unsigned int reverse(register unsigned int x)
	{
		x = (((x & 0xaaaaaaaa) >> 1) | ((x & 0x55555555) << 1));
		x = (((x & 0xcccccccc) >> 2) | ((x & 0x33333333) << 2));
		x = (((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4));
		x = (((x & 0xff00ff00) >> 8) | ((x & 0x00ff00ff) << 8));
		return((x >> 16) | (x << 16));

	}
}


struct intRange
{
	int lowerBound; 
	int upperbound;

	bool isEmpty()
	{
		return lowerBound < upperbound;
	}

	bool inRange(int i)
	{
		return lowerBound <= i && i < upperbound;
	}

	void  add(int i)
	{
		if (inRange(i))
			return;

		if (isEmpty())
		{
			lowerBound = i;
			upperbound = i + 1;
			return;
		}

		if (i < lowerBound)
		{
			lowerBound = i;			
		}
		else
		{
			upperbound = i + 1;
		}
	}

	unsigned int count()
	{
		return upperbound - lowerBound;
	}
};
 
const int MAX_BITS = 30;


unsigned huffman::reverse(unsigned value, int len)
{
	return ::reverse(value) >> (32 - len);
}


std::vector<code> huffman::generate(std::vector<char>& lengths)
{
	
	int bl_count[MAX_BITS] = {};
	for (int i = 0; i < lengths.size(); ++i)
	{
		bl_count[lengths[i]]++;
	}
	
	unsigned int next_code[MAX_BITS] = { };
	
	int bits = 0;
	bl_count[0] = 0;
	for (int n = 1; n < MAX_BITS; ++n)
	{
		bits = (bits + bl_count[n - 1]) << 1;
		next_code[n] = bits;
	}

	std::vector<code> codes(lengths.size(), {});
	for (int n = 0; n < lengths.size(); n++)
	{
		int len = lengths[n];
		if (len <= 0)
			continue;

		codes[n] = { len, (int)reverse(next_code[len], len) };
		next_code[len]++;
	}

	return codes;
}


std::vector<char> huffman::defaultTableLengths()
{
	std::vector<char> lengths;

	for (int i = 0; i < 288; ++i)
	{
		if (i <= 143 || i >= 280)
		{
			lengths.push_back(8);
			continue;
		}

		lengths.push_back(i <= 255 ? 9 : 7);
	}

	return lengths;
}

