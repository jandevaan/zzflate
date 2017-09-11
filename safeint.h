#ifndef _ZZSAFEINT
#define _ZZSAFEINT


template <class TValue>
struct safeint
{
	safeint(TValue v) { value = v; }

	template <class TDest> ZZINLINE operator TDest()
	{
		TDest result = TDest(value);
#ifdef _DEBUG
		assert(TValue(result) == value && (value > 0) ==  (result > 0));
#endif
		return result;
	}

	TValue value;
};



template <class TValue>
ZZINLINE safeint<TValue> safecast(TValue v) { return safeint<TValue>(v); }

#endif
