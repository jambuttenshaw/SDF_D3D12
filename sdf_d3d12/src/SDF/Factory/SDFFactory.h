#pragma once

#include "Core.h"
#include "Renderer/D3DPipeline.h"
#include "Renderer/Buffer/CounterResource.h"
#include "Renderer/Memory/MemoryAllocator.h"

using Microsoft::WRL::ComPtr;


class SDFObject;
class SDFEditList;


// Root signatures
namespace AABBBuilderComputeRootSignature
{
	enum Value
	{
		BuildParameterSlot = 0,
		EditListSlot,
		CounterResourceSlot,
		AABBBufferSlot,
		BrickBufferSlot,
		Count
	};
}

namespace BrickBuildComputeRootSignature
{
	enum Value
	{
		BuildParameterSlot = 0,
		EditListSlot,
		BrickBufferSlot,
		BrickPoolSlot,
		Count
	};
}


/**
 * An object that will dispatch the CS to render some primitive data into a volume texture
 */
class SDFFactory
{
protected:
	SDFFactory();
public:
	virtual ~SDFFactory();

	DISALLOW_COPY(SDFFactory)
	DEFAULT_MOVE(SDFFactory)

	// The default SDF Factory only supports sync building
	virtual void BakeSDFSync(SDFObject* object, SDFEditList&& editList);

private:
	void InitializePipelines();

protected:

	// Builds and submits the SDF bake operations to the GPU. This can block the CPU while GPU operations are happening,
	// but it will not make the GPU wait on other GPU operations from other queues
	void PerformSDFBake_CPUBlocking(SDFObject* object, const SDFEditList& editList);

	static void LogBuildParameters(const struct SDFBuilderConstantBuffer& buildParams);

protected:
	// API objects

	// The SDF factory builds its own command list
	ComPtr<ID3D12CommandAllocator> m_CommandAllocator;
	ComPtr<ID3D12GraphicsCommandList> m_CommandList;

	// Pipelines to build SDF objects
	std::unique_ptr<D3DComputePipeline> m_BrickBuilderPipeline;
	std::unique_ptr<D3DComputePipeline> m_BrickEvaluatorPipeline;

	CounterResource m_CounterResource;
	DescriptorAllocation m_CounterResourceUAV;

	// The fence value that will signal when the previous bake has completed
	UINT64 m_PreviousBakeFence = 0;		
};
