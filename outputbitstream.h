#ifndef _ZZOUTPUTBITSTREAM
#define _ZZOUTPUTBITSTREAM

#include <cstdint>
#include <cassert>
#include <functional>
#include <algorithm>
#include <memory>

#include "safeint.h" 
#include "huffman.h"
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
 


struct bufferHelper
{
	bufferHelper(const bufferHelper& a) = default;

	bufferHelper(int capacity) :
		buffer(new unsigned char[capacity]),
		capacity(capacity),
		bytesStored(0)
	{
	}

	unsigned char* buffer;
	int bytesStored;
	int capacity;
};




struct outputbitstream
{ 

private:
	unsigned char* start = nullptr;

	uint64_t _bitBuffer = 0;
	int _usedBitCount = 0;
 	 
	unsigned char* streamEnd = nullptr;
	unsigned char* stream = nullptr;
	bool fixedOutputBuffer;
public:
	std::vector<std::unique_ptr<bufferHelper>> buffers;
 
	outputbitstream(const outputbitstream& strm) = delete;
 	
	outputbitstream(unsigned char* stream, int64_t byteCount) :
		fixedOutputBuffer(stream != nullptr),
		start(stream),
		stream(stream),
		streamEnd(stream + byteCount)
	{				 	
	}

	__forceinline void AppendToBitStream(code code)
	{
		if(code.length == 0)
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

	void PadToByte()
	{
		AppendToBitStream(0, -_usedBitCount & 0x7);
	}

	void Flush()
	{
		if (_usedBitCount != 0)
		{
			PadToByte();

			while (_usedBitCount >= 8)
			{
				_usedBitCount -= 8;
				writebyte(_bitBuffer & 0xFF);
				_bitBuffer = _bitBuffer >> 8;
			}
		}

		if (!fixedOutputBuffer)
		{
			auto bytes = safecast(byteswritten());
			(buffers[buffers.size() - 1])->bytesStored = bytes;
		}
	}

	bool WriteU16(uint16_t value)
	{
		PadToByte(); 
		AppendToBitStream(value, 16);
		return true;
	}


	void WriteU8(uint8_t value)
	{
		PadToByte(); 
		AppendToBitStream(value, 8);
	}

	void WriteBigEndianU32(uint32_t value)
	{
		PadToByte();
		AppendToBitStream(value >> 24 & 0xFF, 8);
		AppendToBitStream(value >> 16 & 0xFF, 8);		
		AppendToBitStream(value >> 8 & 0xFF, 8);
		AppendToBitStream(value & 0xFF, 8);
	}


	void WriteBytes(const unsigned char* source, uint16_t length)
	{ 
		Flush();
		memcpy(stream, source, length);
		stream += length;
	}

	int64_t byteswritten() const
	{
		assert(_usedBitCount == 0);
		return stream - start;
	}

	int64_t AvailableBytes() const 	{ return streamEnd - stream; }


	int64_t CheckOutputBuffer(int64_t length)
	{
		auto available = AvailableBytes();

		if (IsEnough(available, length))
			 return available;

		if (buffers.size() != 0)
		{
			 buffers[buffers.size()- 1]->bytesStored = safecast(stream - start);
		}

		auto buffer = std::make_unique<bufferHelper>(1000000);
		stream = start = buffer->buffer;
		streamEnd = buffer->buffer + buffer->capacity;

		buffers.push_back(std::move(buffer));
		return AvailableBytes();

	}

	bool IsEnough(int64_t available, int64_t length)
	{
		if (fixedOutputBuffer)
			return true;

		if (available > 2 * length)
			return true;

		return available > 1 << 18; 
	}

private:

	void writebyte(unsigned char b)
	{
		*stream = b;
		stream++;
	}

	void write(uint32_t val)
	{
		*(uint32_t*)stream = val;
		stream += 4;
	}
	 
	
};

#endif
