#pragma once

#include "Core.h"


class D3DQueue;


class Profiler
{
protected:
	Profiler() = default;

	static std::unique_ptr<Profiler> s_Profiler;
public:
	// Factory
	static void Create(ID3D12Device* device, const D3DQueue* queue);
	static void Destroy();

	static Profiler& Get();

public:
	virtual ~Profiler() = default;

	DISALLOW_COPY(Profiler)
	DISALLOW_MOVE(Profiler)

	virtual void CaptureNextFrame() = 0;

	virtual void BeginPass(const char* name) = 0;
	virtual void EndPass() = 0;

	virtual void PushRange(const char* name) = 0;
	virtual void PushRange(const char* name, ID3D12GraphicsCommandList* commandList) = 0;
	virtual void PopRange() = 0;
	virtual void PopRange(ID3D12GraphicsCommandList* commandList) = 0;
};


#ifdef ENABLE_INSTRUMENTATION

#define PROFILER_CAPTURE_NEXT_FRAME()				::Profiler::Get().CaptureNextFrame()

#define PROFILER_BEGIN_PASS(name)					::Profiler::Get().BeginPass(name)
#define PROFILER_END_PASS()							::Profiler::Get().EndPass()

#define PROFILER_PUSH_RANGE(name)					::Profiler::Get().PushRange(name)
#define PROFILER_PUSH_CMD_LIST_RANGE(name, ...)		::Profiler::Get().PushRange(name, __VA_ARGS__)
#define PROFILER_POP_RANGE()						::Profiler::Get().PopRange()
#define PROFILER_POP_CMD_LIST_RANGE(...)			::Profiler::Get().PopRange(__VA_ARGS__)

#else

#define PROFILER_CAPTURE_NEXT_FRAME()				

#define PROFILER_BEGIN_PASS()						
#define PROFILER_END_PASS()							

#define PROFILER_PUSH_RANGE(name)					
#define PROFILER_PUSH_CMD_LIST_RANGE(name, ...)		
#define PROFILER_POP_RANGE()						
#define PROFILER_POP_CMD_LIST_RANGE(...)			

#endif
