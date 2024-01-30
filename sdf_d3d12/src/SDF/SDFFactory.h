#pragma once

#include "Core.h"
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

	void BakeSDFAsync(SDFObject* object, const SDFEditList& editList);

private:
	void InitializePipelines();

	void FactoryAsyncProc();

private:
	// API objects

	// The SDF factory builds its own command list
	ComPtr<ID3D12CommandAllocator> m_CommandAllocator;
	ComPtr<ID3D12GraphicsCommandList> m_CommandList;

	// Pipelines to build SDF objects
	std::unique_ptr<D3DComputePipeline> m_BrickBuilderPipeline;
	std::unique_ptr<D3DComputePipeline> m_BrickEvaluatorPipeline;

	CounterResource m_CounterResource;
	DescriptorAllocation m_CounterResourceUAV;

	// Synchronized Bake
	UINT64 m_PreviousBakeFence = 0;		// The fence value that will signal when the previous bake has completed
};
