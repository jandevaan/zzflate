#ifndef _ZZENCODERSTATE
#define _ZZENCODERSTATE
 
#include "config.h" 
#include <functional>
#include <algorithm>
#include "huffman.h" 
#include "outputbitstream.h"

uint32_t adler32x(uint32_t startValue, const uint8_t *data, size_t len);

uint32_t combine(uint32_t first, uint32_t second, size_t lenSecond);

  
struct lengthRecord
{
	short code;
	char extraBits;
	char extraBitLength;
};

enum CurrentBlockType
{
	Uncompressed = 0b00,
	FixedHuffman = 0b01,
	UserDefinedHuffman
};
 
struct compressionRecord
{
	uint32_t literals;
	uint16_t backoffset;
	uint16_t length;
};


struct Encoder
{

private:
	static const int hashBits = 13;
	static const unsigned hashSize = 1 << hashBits;
	static const unsigned hashMask = hashSize - 1;
	static const int maxRecords = 20000;
	static const int maxDistance = 0x8000;
	 
	static const uint8_t extraLengthBits[286];
	static const uint8_t extraDistanceBits[30];
	static const uint8_t order[19];
	static const uint16_t distanceTable[30];
	static uint8_t distanceLut[32769];
	static lengthRecord  lengthTable[259];

	// for fixed huffman encoding
	static code codes_f[286]; // literals
	static code lcodes_f[259]; // table to send lengths (symbol + extra bits for all 258)
	static code dcodes_f[30];


	
	// user huffman 
	std::vector<compressionRecord> comprecords;
	std::vector<int> lengths = std::vector<int>(286); // temp
	code codes[286]; // literals
	code lcodes[259]; // table to send lengths (symbol + extra bits for all 258)
	code dcodes[30];
	
	
	// general
	int level;
	int hashtable[hashSize];

public:
	outputbitstream stream;
	 
public:

	Encoder(int level, uint8_t* outputBuffer = nullptr, int64_t bytes = 0);

	bool AddData(const uint8_t * start, const uint8_t * end, bool final);
	 
	void SetLevel(int newlevel)
	{
		level = newlevel;
	}

	bool AddDataGzip(const uint8_t* start, const uint8_t* end, uint32_t& adler, bool final);

	static void buildLengthLookup();
	static void InitFixedHuffman();

	static int FindDistance(int offset);
	static int ReadLut(int offset) { return distanceLut[offset]; }
	 
	static void CreateMergedLengthCodes(code* lCodes, code* symbolCodes);

private:
	void Init();	
	void WriteDistance(const code* distCodes, int offset);
	void StartBlock(CurrentBlockType t, int final);
	void WriteRecords(const uint8_t* src, const std::vector<compressionRecord>& vector);
	std::vector<lenghtRecord> ComputeCodes(const std::vector<int>& frequencies, std::vector<int>& codeLengthFreqs, code* outputCodes);
	int64_t CountBits(const std::vector<int> & frequencies, const uint8_t * extraBits);
	int WriteBlock2Pass(const uint8_t * source, int byteCount, bool final);
	int UncompressedFallback(int length, const uint8_t * source, bool final);
	static unsigned int CalcHash(const uint8_t * ptr);	  
	void FixHashTable(int offset);
	int WriteBlockFixedHuff(const uint8_t * source, int byteCount, int final);	 
	int FirstPass(const uint8_t * source, int byteCount);
	int FirstPass2(const uint8_t * source, int byteCount);
	void GetFrequencies(const uint8_t * source, int length, std::vector<int>& symbolFreqs, std::vector<int>& distanceFrequencies);
 	void AddHashEntries(const uint8_t * source, int i, int extra);
	int WriteUncompressedBlock(const uint8_t* source, int byteCount, int final);
	int WriteDeflateBlock(const uint8_t * source, int inputLength, bool final);
};

#endif
