#include "pch.h"
#include "Profiler.h"

#include "Renderer/D3DGraphicsContext.h"


#ifdef NV_PERF_ENABLE_INSTRUMENTATION
#include "NvProfiler.h"
#endif


std::unique_ptr<Profiler> Profiler::s_Profiler;


void Profiler::Create(const ProfilerArgs& args)
{
	ASSERT(!s_Profiler, "Cannot create multiple profilers!");
	LOG_INFO("Creating profiler...");

#ifdef NV_PERF_ENABLE_INSTRUMENTATION
	s_Profiler = std::make_unique<NvProfiler>(args);
#endif

	ID3D12CommandQueue* pQueue = nullptr;
	switch (args.Queue)
	{
	case ProfilerQueue::Direct: pQueue = g_D3DGraphicsContext->GetDirectCommandQueue()->GetCommandQueue(); break;
	case ProfilerQueue::Compute: pQueue = g_D3DGraphicsContext->GetComputeCommandQueue()->GetCommandQueue(); break;
	default: break;
	}

	ASSERT(pQueue, "Failed to get queue!");

	s_Profiler->Init(g_D3DGraphicsContext->GetDevice(), pQueue, args);
}


void Profiler::Destroy()
{
	ASSERT(s_Profiler, "No profiler has been created.");
	LOG_INFO("Destroying profiler...");
	s_Profiler.reset();
}

Profiler& Profiler::Get()
{
	ASSERT(s_Profiler, "No profiler has been created.");
	return *s_Profiler;
}


Profiler::Profiler(const ProfilerArgs& args)
	: m_Queue(args.Queue)
{
	QueryPerformanceFrequency(&m_ClockFreq);
	QueryPerformanceCounter(&m_StartTimestamp);
}


void Profiler::CaptureNextFrame()
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

void Profiler::BeginPass(ProfilerQueue queue, const char* name)
{
	if (queue != m_Queue)
		return;

	if (m_InCollection)
	{
		BeginPassImpl(name);
	}

}
void Profiler::EndPass(ProfilerQueue queue)
{
	if (queue != m_Queue)
		return;

	if (m_InCollection)
	{
		EndPassImpl();
	}
}

void Profiler::PushRange(ProfilerQueue queue, const char* name)
{
	if (queue != m_Queue)
		return;

	PushRangeImpl(name);
}
void Profiler::PushRange(ProfilerQueue queue, const char* name, ID3D12GraphicsCommandList* commandList)
{
	if (queue != m_Queue)
		return;

	PushRangeImpl(name, commandList);
}
void Profiler::PopRange(ProfilerQueue queue)
{
	if (queue != m_Queue)
		return;

	PopRangeImpl();
}
void Profiler::PopRange(ProfilerQueue queue, ID3D12GraphicsCommandList* commandList)
{
	if (queue != m_Queue)
		return;

	PopRangeImpl(commandList);
}
