#ifndef _ZZSAFEINT
#define _ZZSAFEINT


template <class TValue>
struct safeint
{
	safeint(TValue v) { value = v; }

	template <class TDest> __forceinline operator TDest()
	{
		TDest result = TDest(value);
#ifdef DEBUG
		assert(result == value);// && sign(value) == sign(result));
#endif
		return result;
	}

	TValue value;
};



template <class TValue>
__forceinline safeint<TValue> safecast(TValue v) { return safeint<TValue>(v); }

#endif
