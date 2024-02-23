#include "pch.h"
#include "Profiler.h"


#ifdef NV_PERF_ENABLE_INSTRUMENTATION
#include "NvProfiler.h"
#endif


std::unique_ptr<Profiler> Profiler::Create(ID3D12Device* device, const D3DQueue* queue)
{
#ifdef NV_PERF_ENABLE_INSTRUMENTATION
	return std::make_unique<NvProfiler>(device, queue);
#endif

	return nullptr;
}
