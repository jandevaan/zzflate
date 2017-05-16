#ifndef outputbitstream_
#define outputbitstream_

#include <cstdint>
#include <cassert>
#include <string>

uint32_t adler32x(const unsigned char *data, size_t len);

struct code
{
	int length;
	int bits;
};

class outputbitstream
{ 
	outputbitstream(const outputbitstream& strm) = delete;
public:

	void writebyte(unsigned char b)
	{
		*_stream = b;
		_stream++;
	}

	outputbitstream(unsigned char* stream)		  
	{		
		_start = stream;
		_stream = stream;
	}

	void AppendToBitStream(code code)
	{
		assert(code.length != 0);
		AppendToBitStream(code.bits, code.length);
	}


	void AppendToBitStream(int32_t bits, int32_t bitCount)
	{
		if(((~0 << bitCount) & bits) != 0)
		{
			assert(false);
		}
		_bitBuffer |= bits << _usedBitCount;

		_usedBitCount += bitCount;

		while (_usedBitCount >= 8)
		{
			_usedBitCount -= 8;
			writebyte(_bitBuffer & 0xFF);
			_bitBuffer = _bitBuffer >> 8;
		}
	}

	void Flush()
	{
		if (_usedBitCount == 0)
			return;

		AppendToBitStream(0, 8 - _usedBitCount);
	}

	bool WriteU16(uint16_t value)
	{
		assert(_usedBitCount == 0);
		*_stream++ = value & 0xFF;
		*_stream++ = value >> 8;
		return true;
	}

	void WriteBigEndianU32(uint32_t value)
	{
		assert(_usedBitCount == 0);
		*_stream++ = value >> 24;
		*_stream++ = value >> 16;
		*_stream++ = value >> 8;
		*_stream++ = value;

	}


	long long byteswritten() const
	{
		assert(_usedBitCount == 0);
		return _stream - _start;
	}

 

	unsigned char* _stream = nullptr;
	unsigned char* _start = nullptr;
	
	uint32_t _bitBuffer = 0;
	int _usedBitCount  = 0;
	
};

#endif