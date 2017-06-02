#ifndef outputbitstream_
#define outputbitstream_

#include <cstdint>
#include <cassert>

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

	void write(uint32_t val)
	{
		*(uint32_t*)_stream = val;
		_stream += 4;
	}

	outputbitstream(unsigned char* stream)		  
	{		
		_start = stream;
		_stream = stream;
	}

	void AppendToBitStream(code code)
	{
		if(code.length == 0)
		{
			assert(false);
		}
		AppendToBitStream(code.bits, code.length);
	}


	void AppendToBitStream(int32_t bits, int32_t bitCount)
	{
		if(((~0 << bitCount) & bits) != 0)
		{
			assert(false);
		}
		_bitBuffer |= ((uint64_t)bits) << _usedBitCount;

		_usedBitCount += bitCount;

		while (_usedBitCount >= 32)
		{
			_usedBitCount -= 32;
			write((uint32_t)_bitBuffer);
			_bitBuffer = _bitBuffer >> 32;
		}
	}

	void Flush()
	{
		if (_usedBitCount == 0)
			return;

		AppendToBitStream(0, -_usedBitCount & 0x7);

		while (_usedBitCount >= 8)
		{
			_usedBitCount -= 8;
			writebyte(_bitBuffer & 0xFF);
			_bitBuffer = _bitBuffer >> 8;
		}
		
	}

	bool WriteU16(uint16_t value)
	{
		AppendToBitStream(0, -_usedBitCount & 0x7);
		AppendToBitStream(value, 16);
		return true;
	}


	void WriteU8(uint8_t value)
	{
		AppendToBitStream(0, -_usedBitCount & 0x7);
		AppendToBitStream(value, 8);		
	}

	void WriteBigEndianU32(uint32_t value)
	{
		AppendToBitStream(0, -_usedBitCount & 0x7);

		AppendToBitStream(value >> 24 & 0xFF, 8);
		AppendToBitStream(value >> 16 & 0xFF, 8);		
		AppendToBitStream(value >> 8 & 0xFF, 8);
		AppendToBitStream(value & 0xFF, 8);
	}


	long long byteswritten() const
	{
		assert(_usedBitCount == 0);
		return _stream - _start;
	}

	unsigned char* _stream = nullptr;
	unsigned char* _start = nullptr;
	
	uint64_t _bitBuffer = 0;
	int _usedBitCount  = 0;
	
};

#endif