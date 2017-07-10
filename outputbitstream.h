#ifndef outputbitstream_
#define outputbitstream_

#include <cstdint>
#include <cassert>


template <class TDest, class TSource>
TDest int_cast(TSource value)
{
	TDest result = TDest(value);
	assert(result == value);// && sign(value) == sign(result));
	return result;
}


template <class TValue>
class safeint
{
public:
	safeint(TValue v) { value = v; }

	template <class TDest>
	TDest convert()
	{
		return int_cast<TDest, TValue>(value);
	}
	 
	template <class TDest> operator TDest() { return int_cast<TDest, TValue>(value); }

	TValue value;
};



template <class TValue>
safeint<TValue> safecast(TValue v) { return safeint<TValue>(v); }

uint32_t adler32x(uint32_t startValue, const unsigned char *data, size_t len);

struct code
{  
	code() = default;
	code(int length, uint32_t bits)
	{
		this->length = safecast(length);
		this->bits = bits;
	}
	int32_t length;
	uint32_t bits;
};

struct scode
{
	scode() = default;

	scode(int length, int bits)
	{
		this->length = safecast(length);
		this->bits = safecast(bits);
	}
	int16_t length;
	uint16_t bits;
};


class outputbitstream
{ 
	outputbitstream(const outputbitstream& strm) = delete;


public:

	outputbitstream(unsigned char* stream)		  
	{		
		_start = stream;
		_stream = stream;
	}

	__forceinline void AppendToBitStream(code code)
	{
		if(code.length == 0)
		{
			assert(false);
		}
		AppendToBitStream(code.bits, code.length);
	}

	__forceinline void AppendToBitStream(scode code)
	{
		if (code.length == 0)
		{
			assert(false);
		}
		AppendToBitStream(code.bits, code.length);
	}


	__forceinline void AppendToBitStream(uint64_t bits, int32_t bitCount)
	{
		//if(((~0 << bitCount) & bits) != 0)
		//{
		//	assert(false);
		//}
		_bitBuffer |= bits << _usedBitCount;

		_usedBitCount += bitCount;

		if (_usedBitCount < 32)
			return;

		_usedBitCount -= 32;
		write(uint32_t(_bitBuffer));
		_bitBuffer = _bitBuffer >> 32;
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
private:

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


	
	unsigned char* _start = nullptr;
	
	uint64_t _bitBuffer = 0;
	int _usedBitCount  = 0;
	
};

#endif