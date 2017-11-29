#pragma once
#include <stdint.h> 

class Decoder
{


	Decoder()
	{

	}

	struct code { uint8_t length; ; };

	code lengthCodes[65536];
	code symbolCodes[65536];

	class BitStream
	{
	public:
		uint16_t Peek16Bits() { return 0; };
	};

	code 
		ReadCode()
	{
		uint16_t value = inputStream.Peek16Bits();
		auto code = symbolCodes[value];
	 
	}
	
	BitStream inputStream;
};