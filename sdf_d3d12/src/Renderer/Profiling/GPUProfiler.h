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
	// Column headers with descriptive names for the metrics
	std::vector<std::string> Headers;

	// Should ranges with the same name be accumulated or measured separately
	bool CombineSharedNameRanges = false;
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

	static void GetAvailableMetrics(const std::string& outfile);

	static GPUProfiler& Get();

public:
	virtual ~GPUProfiler() = default;

	DISALLOW_COPY(GPUProfiler)
	DISALLOW_MOVE(GPUProfiler)

	void CaptureNextFrame();

	// For profiling macros - to be embedded in application source
	void BeginPass(GPUProfilerQueue queue, const char* name);
	void EndPass(GPUProfilerQueue queue);

	// Index is an optional integer to place at the end of the range name
	// in the case where a range may be called in a loop but should be profiled separately
	void PushRange(GPUProfilerQueue queue, const char* name, UINT index = 0);
	void PushRange(GPUProfilerQueue queue, const char* name, ID3D12GraphicsCommandList* commandList, UINT index = 0);
	void PopRange(GPUProfilerQueue queue);
	void PopRange(GPUProfilerQueue queue, ID3D12GraphicsCommandList* commandList);

	// For data collection and processing
	inline bool IsInCollection() const { return m_InCollection; }
	inline static float GetWarmupTime() { return s_WarmupTime; }

	// Decode profiling data
	// Returns if collected data was decoded
	virtual bool DecodeData(std::vector<std::stringstream>& outMetrics) = 0;

protected:
	virtual void Init(ID3D12Device* device, ID3D12CommandQueue* queue, const GPUProfilerArgs& args) = 0;

	virtual void CaptureNextFrameImpl() = 0;

	virtual void BeginPassImpl(const char* name) = 0;
	virtual void EndPassImpl() = 0;

	virtual void PushRangeImpl(const char* name) = 0;
	virtual void PushRangeImpl(const char* name, ID3D12GraphicsCommandList* commandList) = 0;
	virtual void PopRangeImpl() = 0;
	virtual void PopRangeImpl(ID3D12GraphicsCommandList* commandList) = 0;

	virtual void LogAllMetrics(const std::string& outfilePath) const = 0;

protected:
	GPUProfilerQueue m_Queue;

	// Ready to begin collection
	bool m_InCollection = false;

	static constexpr double s_WarmupTime = 1.0; // Wait 1s to allow the clock to stabilize before beginning to profile.
	static constexpr size_t s_MaxNumRanges = 17;		// Set these as low as possible to make profiling as fast as possible
	static constexpr uint16_t s_NumNestingLevels = 3;

	LARGE_INTEGER m_ClockFreq;
	LARGE_INTEGER m_StartTimestamp;

	double m_CurrentRunTime = 0.0;

	bool m_CombineSharedNameRanges = false;
};


#ifdef ENABLE_INSTRUMENTATION

#define PROFILE_CAPTURE_NEXT_FRAME()			::GPUProfiler::Get().CaptureNextFrame()

// Direct Queue profiling
#define PROFILE_DIRECT_BEGIN_PASS(name)			::GPUProfiler::Get().BeginPass(GPUProfilerQueue::Direct, name)
#define PROFILE_DIRECT_END_PASS()				::GPUProfiler::Get().EndPass(GPUProfilerQueue::Direct)

#define PROFILE_DIRECT_PUSH_RANGE(...)			::GPUProfiler::Get().PushRange(GPUProfilerQueue::Direct, __VA_ARGS__)
#define PROFILE_DIRECT_POP_RANGE(...)			::GPUProfiler::Get().PopRange(GPUProfilerQueue::Direct, __VA_ARGS__)

// Compute Queue
#define PROFILE_COMPUTE_BEGIN_PASS(name, ...)	::GPUProfiler::Get().BeginPass(GPUProfilerQueue::Compute, name, __VA_ARGS__)
#define PROFILE_COMPUTE_END_PASS(...)			::GPUProfiler::Get().EndPass(GPUProfilerQueue::Compute, __VA_ARGS__)

#define PROFILE_COMPUTE_PUSH_RANGE(...)			::GPUProfiler::Get().PushRange(GPUProfilerQueue::Compute, __VA_ARGS__)
#define PROFILE_COMPUTE_POP_RANGE(...)			::GPUProfiler::Get().PopRange(GPUProfilerQueue::Compute, __VA_ARGS__)

#else

#define PROFILE_CAPTURE_NEXT_FRAME()			

#define PROFILE_DIRECT_BEGIN_PASS(name, ...)				
#define PROFILE_DIRECT_END_PASS(...)					

#define PROFILE_DIRECT_PUSH_RANGE(...)				
#define PROFILE_DIRECT_POP_RANGE(...)				

#define PROFILE_COMPUTE_BEGIN_PASS(name, ...)			
#define PROFILE_COMPUTE_END_PASS(...)					

#define PROFILE_COMPUTE_PUSH_RANGE(...)				
#define PROFILE_COMPUTE_POP_RANGE(...)				

#endif
