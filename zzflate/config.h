#ifndef _ZZCONIG
#define _ZZCONIG

#include "string"

#if _MSC_VER && !__INTEL_COMPILER
#include "visualc.h"
#else
#if __GNUC__
#include "gcc.h"
#else
#include "generic.h"
#endif
#endif

#endif
