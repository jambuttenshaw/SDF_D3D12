#pragma once

#include "Core.h"


class D3DQueue;


class Profiler
{
protected:
	Profiler() = default;
public:
	// Factory
	static std::unique_ptr<Profiler> Create(ID3D12Device* device, const D3DQueue* queue);

public:
	virtual ~Profiler() = default;

	DISALLOW_COPY(Profiler)
	DISALLOW_MOVE(Profiler)

	virtual void BeginPass() = 0;
	virtual void EndPass() = 0;

	virtual void PushRange(const char* name) = 0;
	virtual void PopRange() = 0;
};
