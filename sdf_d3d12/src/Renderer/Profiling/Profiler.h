#pragma once

#include "Core.h"


enum class ProfilerQueue
{
	Direct,
	Compute,
	Count
};

struct ProfilerArgs
{
	ProfilerQueue Queue;
};


class Profiler
{
protected:
	Profiler(const ProfilerArgs& args);

	static std::unique_ptr<Profiler> s_Profiler;
public:
	// Factory
	static void Create(const ProfilerArgs& args);
	static void Destroy();

	static Profiler& Get();

public:
	virtual ~Profiler() = default;

	DISALLOW_COPY(Profiler)
	DISALLOW_MOVE(Profiler)

	void CaptureNextFrame();

	void BeginPass(ProfilerQueue queue, const char* name);
	void EndPass(ProfilerQueue queue);

	void PushRange(ProfilerQueue queue, const char* name);
	void PushRange(ProfilerQueue queue, const char* name, ID3D12GraphicsCommandList* commandList);
	void PopRange(ProfilerQueue queue);
	void PopRange(ProfilerQueue queue, ID3D12GraphicsCommandList* commandList);

protected:
	virtual void Init(ID3D12Device* device, ID3D12CommandQueue* queue, const ProfilerArgs& args) = 0;

	virtual void CaptureNextFrameImpl() = 0;

	virtual void BeginPassImpl(const char* name) = 0;
	virtual void EndPassImpl() = 0;

	virtual void PushRangeImpl(const char* name) = 0;
	virtual void PushRangeImpl(const char* name, ID3D12GraphicsCommandList* commandList) = 0;
	virtual void PopRangeImpl() = 0;
	virtual void PopRangeImpl(ID3D12GraphicsCommandList* commandList) = 0;

protected:
	ProfilerQueue m_Queue;

	bool m_InCollection = false;

	static constexpr double s_WarmupTime = 2.0; // Wait 2s to allow the clock to stabilize before beginning to profile.
	static constexpr size_t s_MaxNumRanges = 8;
	static constexpr uint16_t s_NumNestingLevels = 2;

	LARGE_INTEGER m_ClockFreq;
	LARGE_INTEGER m_StartTimestamp;

	double m_CurrentRunTime = 0.0;
};


#ifdef ENABLE_INSTRUMENTATION

#define PROFILE_CAPTURE_NEXT_FRAME()			::Profiler::Get().CaptureNextFrame()

// Direct Queue profiling
#define PROFILE_DIRECT_BEGIN_PASS(name)			::Profiler::Get().BeginPass(ProfilerQueue::Direct, name)
#define PROFILE_DIRECT_END_PASS()				::Profiler::Get().EndPass(ProfilerQueue::Direct)

#define PROFILE_DIRECT_PUSH_RANGE(...)			::Profiler::Get().PushRange(ProfilerQueue::Direct, __VA_ARGS__)
#define PROFILE_DIRECT_POP_RANGE(...)			::Profiler::Get().PopRange(ProfilerQueue::Direct, __VA_ARGS__)

// Compute Queue
#define PROFILE_COMPUTE_BEGIN_PASS(name)		::Profiler::Get().BeginPass(ProfilerQueue::Compute, name)
#define PROFILE_COMPUTE_END_PASS()				::Profiler::Get().EndPass(ProfilerQueue::Compute)

#define PROFILE_COMPUTE_PUSH_RANGE(...)			::Profiler::Get().PushRange(ProfilerQueue::Compute, __VA_ARGS__)
#define PROFILE_COMPUTE_POP_RANGE(...)			::Profiler::Get().PopRange(ProfilerQueue::Compute, __VA_ARGS__)

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
