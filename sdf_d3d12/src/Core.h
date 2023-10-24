#pragma once

#include <stdexcept>

#include "Framework/Log.h"


#ifdef _DEBUG

#define THROW_IF_FAIL(x) if (FAILED(x)) { LOG_FATAL("Fatal exception"); }
#define ASSERT(x, msg) if (!(x)) { LOG_ERROR("Debug assertion failed in file ({0}) on line ({1}). Message: {2}", __FILE__, __LINE__, msg); DebugBreak(); }

#define NOT_IMPLEMENTED ASSERT(false, "Not Implemented")

#else

#define THROW_IF_FAIL(x) (void)(x);
#define ASSERT(x, msg) (x)

#define NOT_IMPLEMENTED

#endif
