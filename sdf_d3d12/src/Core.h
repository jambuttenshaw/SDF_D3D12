#pragma once

#include "Framework/Log.h"
#include "Framework/Exception.h"


#ifdef _DEBUG

#define THROW_IF_FAIL(x) { const HRESULT hr = x; if (FAILED(hr)) { LOG_FATAL(DXException(hr).ToString().c_str()); throw; }}
#define ASSERT(x, msg) if (!(x)) { LOG_ERROR("Debug assertion failed in file ({0}) on line ({1}). Message: {2}", __FILE__, __LINE__, msg); DebugBreak(); }

#define NOT_IMPLEMENTED ASSERT(false, "Not Implemented")
#define DEPRECATED ASSERT(false, "Deprecated")

#else

#define THROW_IF_FAIL(x) (void)(x);
#define ASSERT(x, msg)

#define NOT_IMPLEMENTED
#define DEPRECATED

#endif


// Class helpers

#define DEFAULT_COPY(T) T(T&) = default; \
						T& operator=(const T&) = default;
#define DEFAULT_MOVE(T) T(T&&) = default; \
						T& operator=(T&&) = default;

#define DISALLOW_COPY(T) T(T&) = delete; \
						 T& operator=(const T&) = delete;
#define DISALLOW_MOVE(T) T(T&&) = delete; \
						 T& operator=(T&&) = delete;


// Memory helper functions

#define SizeOfInUint32(obj) ((sizeof(obj) - 1) / sizeof(UINT32) + 1)

inline UINT64 Align(UINT64 size, UINT64 alignment)
{
	return (size + (alignment - 1)) & ~(alignment - 1);
}


// DX Helpers
#ifdef _DEBUG
#define D3D_NAME(T) THROW_IF_FAIL(T->SetName(L#T))
#else
#define D3D_NAME(T) 
#endif
