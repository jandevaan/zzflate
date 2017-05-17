#pragma once
#include <vector>
#include "outputbitstream.h"

namespace huffman
{

	unsigned reverse(unsigned value, int len);
	std::vector<code> generate(const std::vector<int>& bytes);  
	std::vector<int> defaultTableLengths();
};

