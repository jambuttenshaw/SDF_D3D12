#pragma once

#include <stdexcept>

#include "Log.h"


#ifdef _DEBUG

#define THROW_IF_FAIL(x) if (FAILED(x)) { LOG_FATAL("Fatal exception"); }
#define ASSERT(x, msg) if (!(x)) { LOG_ERROR(msg); DebugBreak(); }

#define NOT_IMPLEMENTED ASSERT(false, "Not Implemented")

#else

#define THROW_IF_FAIL(x) (void)(x);
#define ASSERT(x) (x)

#define NOT_IMPLEMENTED

#endif
