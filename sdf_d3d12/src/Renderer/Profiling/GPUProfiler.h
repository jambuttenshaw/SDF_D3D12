#pragma once

#include "Core.h"


enum class GPUProfilerQueue
{
	Direct,
	Compute,
	Count
};

struct GPUProfilerArgs
{
	// Which queue to profile
	GPUProfilerQueue Queue;

	// Which metrics to record
	std::vector<std::string> Metrics;
};


class GPUProfiler
{
protected:
	GPUProfiler(const GPUProfilerArgs& args);

	static std::unique_ptr<GPUProfiler> s_Profiler;
public:
	// Factory
	static void Create(const GPUProfilerArgs& args);
	static void Destroy();

	static GPUProfiler& Get();

public:
	virtual ~GPUProfiler() = default;

	DISALLOW_COPY(GPUProfiler)
	DISALLOW_MOVE(GPUProfiler)

	void CaptureNextFrame();

	void BeginPass(GPUProfilerQueue queue, const char* name);
	void EndPass(GPUProfilerQueue queue);

	void PushRange(GPUProfilerQueue queue, const char* name);
	void PushRange(GPUProfilerQueue queue, const char* name, ID3D12GraphicsCommandList* commandList);
	void PopRange(GPUProfilerQueue queue);
	void PopRange(GPUProfilerQueue queue, ID3D12GraphicsCommandList* commandList);

protected:
	virtual void Init(ID3D12Device* device, ID3D12CommandQueue* queue, const GPUProfilerArgs& args) = 0;

	virtual void CaptureNextFrameImpl() = 0;

	virtual void BeginPassImpl(const char* name) = 0;
	virtual void EndPassImpl() = 0;

	virtual void PushRangeImpl(const char* name) = 0;
	virtual void PushRangeImpl(const char* name, ID3D12GraphicsCommandList* commandList) = 0;
	virtual void PopRangeImpl() = 0;
	virtual void PopRangeImpl(ID3D12GraphicsCommandList* commandList) = 0;

protected:
	GPUProfilerQueue m_Queue;

	bool m_InCollection = false;

	static constexpr double s_WarmupTime = 2.0; // Wait 2s to allow the clock to stabilize before beginning to profile.
	static constexpr size_t s_MaxNumRanges = 8;
	static constexpr uint16_t s_NumNestingLevels = 2;

	LARGE_INTEGER m_ClockFreq;
	LARGE_INTEGER m_StartTimestamp;

	double m_CurrentRunTime = 0.0;
};


#ifdef ENABLE_INSTRUMENTATION

#define PROFILE_CAPTURE_NEXT_FRAME()			::GPUProfiler::Get().CaptureNextFrame()

// Direct Queue profiling
#define PROFILE_DIRECT_BEGIN_PASS(name)			::GPUProfiler::Get().BeginPass(GPUProfilerQueue::Direct, name)
#define PROFILE_DIRECT_END_PASS()				::GPUProfiler::Get().EndPass(GPUProfilerQueue::Direct)

#define PROFILE_DIRECT_PUSH_RANGE(...)			::GPUProfiler::Get().PushRange(GPUProfilerQueue::Direct, __VA_ARGS__)
#define PROFILE_DIRECT_POP_RANGE(...)			::GPUProfiler::Get().PopRange(GPUProfilerQueue::Direct, __VA_ARGS__)

// Compute Queue
#define PROFILE_COMPUTE_BEGIN_PASS(name)		::GPUProfiler::Get().BeginPass(GPUProfilerQueue::Compute, name)
#define PROFILE_COMPUTE_END_PASS()				::GPUProfiler::Get().EndPass(GPUProfilerQueue::Compute)

#define PROFILE_COMPUTE_PUSH_RANGE(...)			::GPUProfiler::Get().PushRange(GPUProfilerQueue::Compute, __VA_ARGS__)
#define PROFILE_COMPUTE_POP_RANGE(...)			::GPUProfiler::Get().PopRange(GPUProfilerQueue::Compute, __VA_ARGS__)

#else

#define PROFILE_DIRECT_CAPTURE_NEXT_FRAME()			

#define PROFILE_DIRECT_BEGIN_PASS(name)				
#define PROFILE_DIRECT_END_PASS()					

#define PROFILE_DIRECT_PUSH_RANGE(...)				
#define PROFILE_DIRECT_POP_RANGE(...)				

#define PROFILE_COMPUTE_CAPTURE_NEXT_FRAME()		

#define PROFILE_COMPUTE_BEGIN_PASS(name)			
#define PROFILE_COMPUTE_END_PASS()					

#define PROFILE_COMPUTE_PUSH_RANGE(...)				
#define PROFILE_COMPUTE_POP_RANGE(...)				

#endif
