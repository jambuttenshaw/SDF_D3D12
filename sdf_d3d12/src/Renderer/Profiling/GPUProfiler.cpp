#include "pch.h"
#include "GPUProfiler.h"

#include "Renderer/D3DGraphicsContext.h"


#ifdef NV_PERF_ENABLE_INSTRUMENTATION
#include "NvGPUProfiler.h"
#endif


std::unique_ptr<GPUProfiler> GPUProfiler::s_Profiler;


void GPUProfiler::Create(const GPUProfilerArgs& args)
{
	ASSERT(!s_Profiler, "Cannot create multiple profilers!");
	LOG_INFO("Creating profiler...");

#ifdef NV_PERF_ENABLE_INSTRUMENTATION
	s_Profiler = std::make_unique<NvGPUProfiler>(args);
#endif

	ASSERT(s_Profiler, "No profiler was created!");

	ID3D12CommandQueue* pQueue = nullptr;
	switch (args.Queue)
	{
	case GPUProfilerQueue::Direct: pQueue = g_D3DGraphicsContext->GetDirectCommandQueue()->GetCommandQueue(); break;
	case GPUProfilerQueue::Compute: pQueue = g_D3DGraphicsContext->GetComputeCommandQueue()->GetCommandQueue(); break;
	default: break;
	}

	ASSERT(pQueue, "Failed to get queue!");

	s_Profiler->Init(g_D3DGraphicsContext->GetDevice(), pQueue, args);
}


void GPUProfiler::Destroy()
{
	ASSERT(s_Profiler, "No profiler has been created.");
	LOG_INFO("Destroying profiler...");
	s_Profiler.reset();
}


void GPUProfiler::GetAvailableMetrics(const std::string& outfilePath)
{
	bool destroy = false;
	if (!s_Profiler)
	{
		destroy = true;
		// args are not relevant in this case
		const GPUProfilerArgs args = { GPUProfilerQueue::Direct };
		Create(args);
	}

	s_Profiler->LogAllMetrics(outfilePath);

	if (destroy)
	{
		Destroy();
	}
}



GPUProfiler& GPUProfiler::Get()
{
	ASSERT(s_Profiler, "No profiler has been created.");
	return *s_Profiler;
}


GPUProfiler::GPUProfiler(const GPUProfilerArgs& args)
	: m_Queue(args.Queue)
{
	QueryPerformanceFrequency(&m_ClockFreq);
	QueryPerformanceCounter(&m_StartTimestamp);
}


void GPUProfiler::CaptureNextFrame()
{
	if (m_InCollection)
		return;

	LARGE_INTEGER currentFrameTimeStamp;
	LARGE_INTEGER elapsedTime;
	QueryPerformanceCounter(&currentFrameTimeStamp);
	elapsedTime.QuadPart = currentFrameTimeStamp.QuadPart - m_StartTimestamp.QuadPart;
	m_CurrentRunTime = static_cast<double>(elapsedTime.QuadPart) / static_cast<double>(m_ClockFreq.QuadPart);

	if (s_WarmupTime < m_CurrentRunTime)
	{
		m_InCollection = true;
		CaptureNextFrameImpl();
	}
	else
	{
		LOG_WARN("Capture not initiated: GPU is warming up...");
	}
}

void GPUProfiler::BeginPass(GPUProfilerQueue queue, const char* name)
{
	if (queue != m_Queue)
		return;

	if (m_InCollection)
	{
		BeginPassImpl(name);
	}

}
void GPUProfiler::EndPass(GPUProfilerQueue queue)
{
	if (queue != m_Queue)
		return;

	if (m_InCollection)
	{
		EndPassImpl();
	}
}

void GPUProfiler::PushRange(GPUProfilerQueue queue, const char* name)
{
	if (queue != m_Queue)
		return;

	PushRangeImpl(name);
}
void GPUProfiler::PushRange(GPUProfilerQueue queue, const char* name, ID3D12GraphicsCommandList* commandList)
{
	if (queue != m_Queue)
		return;

	PushRangeImpl(name, commandList);
}
void GPUProfiler::PopRange(GPUProfilerQueue queue)
{
	if (queue != m_Queue)
		return;

	PopRangeImpl();
}
void GPUProfiler::PopRange(GPUProfilerQueue queue, ID3D12GraphicsCommandList* commandList)
{
	if (queue != m_Queue)
		return;

	PopRangeImpl(commandList);
}
