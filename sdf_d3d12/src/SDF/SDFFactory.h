#pragma once

#include "Renderer/D3DPipeline.h"
#include "Renderer/Buffer/CounterResource.h"
#include "Renderer/Memory/MemoryAllocator.h"

using Microsoft::WRL::ComPtr;


class SDFObject;
class SDFEditList;

/**
 * An object that will dispatch the CS to render some primitive data into a volume texture
 */
class SDFFactory
{
public:
	SDFFactory();
	~SDFFactory();

	DISALLOW_COPY(SDFFactory)
	DEFAULT_MOVE(SDFFactory)

	void BakeSDFSynchronous(SDFObject* object, const SDFEditList& editList);

private:
	void InitializePipelines();

	void Flush();

private:
	// API objects

	// The SDF factory runs its own command queue, allocator, and list
	ComPtr<ID3D12CommandQueue> m_CommandQueue;
	ComPtr<ID3D12CommandAllocator> m_CommandAllocator;
	ComPtr<ID3D12GraphicsCommandList> m_CommandList;

	// For signalling when SDF baking completed
	ComPtr<ID3D12Fence> m_Fence;
	UINT64 m_FenceValue = 0;
	HANDLE m_FenceEvent = nullptr;


	// Pipelines to build SDF objects
	std::unique_ptr<D3DComputePipeline> m_BrickBuilderPipeline;
	std::unique_ptr<D3DComputePipeline> m_BrickEvaluatorPipeline;

	CounterResource m_CounterResource;
	DescriptorAllocation m_CounterResourceUAV;
};
