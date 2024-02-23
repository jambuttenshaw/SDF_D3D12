#include "pch.h"
#include "Profiler.h"


#ifdef NV_PERF_ENABLE_INSTRUMENTATION
#include "NvProfiler.h"
#endif


std::unique_ptr<Profiler> Profiler::s_Profiler;


void Profiler::Create(ID3D12Device* device, const D3DQueue* queue)
{
	ASSERT(!s_Profiler, "Cannot create multiple profilers!");
	LOG_INFO("Creating profiler...");

#ifdef NV_PERF_ENABLE_INSTRUMENTATION
	s_Profiler = std::make_unique<NvProfiler>(device, queue);
#endif

	if (!s_Profiler)
	{
		LOG_ERROR("No profiler was created.");
	}
}


void Profiler::Destroy()
{
	LOG_INFO("Destroying profiler...");
	s_Profiler.reset();
}

Profiler& Profiler::Get()
{
	ASSERT(s_Profiler, "Instrumentation is enabled but Profiler::Create has not been called.");
	return *s_Profiler;
}
