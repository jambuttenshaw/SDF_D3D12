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

	void BakeSDFSynchronous(SDFObject* object, const SDFEditList& editList) const;

private:
	void InitializePipelines();

	void Flush() const;

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
};
