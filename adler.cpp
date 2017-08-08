#include <cstdint>
const int MOD_ADLER = 65521;


uint32_t combine(uint32_t first, uint32_t second, size_t lenSecond)
{
	uint64_t a = (first & 0xFFFF) + (second & 0xFFFF);
	uint64_t b = (first >> 16) + (second >> 16);

	b += lenSecond * (first & 0xFFFF);

	a = a % MOD_ADLER;	b = b % MOD_ADLER;

	return uint32_t((b << 16) | a);
}

uint32_t adler32x(uint32_t startValue, const uint8_t *data, size_t len)
{
	uint64_t a = (startValue & 0xFFFF), b = startValue >> 16;
	size_t index = 0;

	/* Process each byte of the data in order */
	for (; index + 4 <= len; index +=4)
	{
		a = (a + data[index]); b = (b + a);
		a = (a + data[index + 1]); b = (b + a);
		a = (a + data[index + 2]); b = (b + a);
		a = (a + data[index + 3]); b = (b + a);

		//a = a % MOD_ADLER;	b = b % MOD_ADLER;
	}


	/* Process each byte of the data in order */
	for (;index < len; ++index)
	{
		a = (a + data[index]) ;b = (b + a) ;
		a = a % MOD_ADLER;	b = b % MOD_ADLER;
	}
	a = a % MOD_ADLER;	b = b % MOD_ADLER;

	return uint32_t((b << 16) | a);
}
