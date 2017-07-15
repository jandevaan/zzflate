
#include "huffman.h"


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

