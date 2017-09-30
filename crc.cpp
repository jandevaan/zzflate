#include "crc.h"
#include <array>
 

std::array<uint32_t, 256> PrepareTable(uint32_t polynominal)
{
	std::array<uint32_t, 256> table;
	  
	for (auto i = 0; i < 0x100; ++i)
	{
		uint32_t crc = i;
		for (auto j = 0; j < 8; j++)
		{
			crc = (crc >> 1) ^ ( (crc & 1) * polynominal);
		}
		table[i] = crc;
	}

	return table;
}
 
std::array<uint32_t, 256> Crc32Lookup  = PrepareTable(0xEDB88320);
 
uint32_t crc32(const uint8_t* buffer, size_t length, uint32_t startValue)
{
	auto crc = ~startValue;

	for (int i = 0; i < length; ++i)
	{
		crc = (crc >> 8) ^ Crc32Lookup[(crc & 0xFF) ^ buffer[i]];
	}
	return ~crc; 
}