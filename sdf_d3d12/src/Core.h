#pragma once

#include <stdexcept>

#include "Framework/Log.h"
#include "Framework/Exception.h"


#ifdef _DEBUG

#define THROW_IF_FAIL(x) if (FAILED(x)) { LOG_FATAL(DXException(x).ToString().c_str()); }
#define ASSERT(x, msg) if (!(x)) { LOG_ERROR("Debug assertion failed in file ({0}) on line ({1}). Message: {2}", __FILE__, __LINE__, msg); DebugBreak(); }

#define NOT_IMPLEMENTED ASSERT(false, "Not Implemented")

#else

#define THROW_IF_FAIL(x) (void)(x);
#define ASSERT(x, msg) (x)

#define NOT_IMPLEMENTED

#endif


// Memory helper functions

#define SizeOfInUint32(obj) ((sizeof(obj) - 1) / sizeof(UINT32) + 1)

inline UINT Align(UINT size, UINT alignment)
{
	return (size + (alignment - 1)) & ~(alignment - 1);
}

