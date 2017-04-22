#pragma once
#include <vector>

namespace huffman
{
	unsigned reverse(unsigned value, int len);
	std::vector<unsigned> generate(std::vector<char>& bytes);  
	std::vector<char> defaultTableLengths();
};

