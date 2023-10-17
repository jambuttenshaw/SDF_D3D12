#pragma once

#include <stdexcept>


#ifdef _DEBUG

#define THROW_IF_FAIL(x) if (FAILED(x)) { throw std::exception(); }
#define ASSERT(x) if (!(x)) { DebugBreak(); }

#else

#define THROW_IF_FAIL(x) (void)(x);
#define ASSERT(x) (x)

#endif
