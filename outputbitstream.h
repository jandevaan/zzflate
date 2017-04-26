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
		AppendToBitStream(0, 8 - _usedBitCount % 8);		
	}

	bool writeushort(uint16_t value)
	{
		*_stream++ = value & 0xFF;
		*_stream++ = value >> 8;
		return true;
	}

	void writeuint32_bigendian(uint32_t value)
	{
		*_stream++ = value >> 24;
		*_stream++ = value >> 16;
		*_stream++ = value >> 8;
		*_stream++ = value;

	}


	long long byteswritten() const
	{
		return _stream - _start;
	}

	void writeUncompressedBlock(const unsigned char* source, uint16_t sourceLen, int final)
	{
		AppendToBitStream(final, 1);  // final
		AppendToBitStream(0, 2); // uncompressed
		Flush();
		
		writeushort(sourceLen);
		writeushort(~sourceLen);

		memcpy(_stream, source, sourceLen);
		_stream += sourceLen;

		uint32_t adler = adler32x(source, sourceLen);

		writeuint32_bigendian(adler); 
	}

 

	unsigned char* _stream = nullptr;
	unsigned char* _start = nullptr;
	
	uint32_t _bitBuffer = 0;
	int _usedBitCount  = 0;
};

#endif