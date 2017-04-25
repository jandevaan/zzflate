#pragma once
#include <vector>
#include "outputbitstream.h"

namespace huffman
{

	unsigned reverse(unsigned value, int len);
	std::vector<code> generate(std::vector<char>& bytes);  
	std::vector<char> defaultTableLengths();
};

