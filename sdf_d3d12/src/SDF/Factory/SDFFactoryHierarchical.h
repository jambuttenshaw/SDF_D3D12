#pragma once

#include "Core.h"

#include "Renderer/D3DPipeline.h"
#include "Renderer/Buffer/UploadBuffer.h"
#include "Renderer/Buffer/DefaultBuffer.h"


using Microsoft::WRL::ComPtr;

class SDFObject;
class SDFEditList;


class SDFFactoryHierarchical
{
public:
	SDFFactoryHierarchical();
	virtual ~SDFFactoryHierarchical() = default;

	DISALLOW_COPY(SDFFactoryHierarchical)
	DEFAULT_MOVE(SDFFactoryHierarchical)

	virtual void BakeSDFSync(SDFObject* object, SDFEditList&& editList);

protected:

	void InitializePipelines();

	void PerformSDFBake_CPUBlocking(SDFObject* object, const SDFEditList& editList);


protected:
	// API objects
	ComPtr<ID3D12CommandAllocator> m_CommandAllocator;
	ComPtr<ID3D12GraphicsCommandList> m_CommandList;

	// Indirect execution objects
	struct IndirectCommand
	{
		D3D12_DISPATCH_ARGUMENTS dispatchArgs;
	};

	ComPtr<ID3D12CommandSignature> m_CommandSignature;
	// The number of commands in the buffer
	inline static constexpr UINT s_NumCommands = 4;
	UploadBuffer<IndirectCommand> m_CommandUploadBuffer;
	DefaultBuffer m_CommandBuffer;


	// Pipelines
	std::unique_ptr<D3DComputePipeline> m_BrickCounterPipeline;

	std::unique_ptr<D3DComputePipeline> m_ScanGroupCountCalculatorPipeline;
	std::unique_ptr<D3DComputePipeline> m_ScanBlocksPipeline;
	std::unique_ptr<D3DComputePipeline> m_ScanBlockSumsPipeline;
	std::unique_ptr<D3DComputePipeline> m_SumScansPipeline;

	std::unique_ptr<D3DComputePipeline> m_BrickBuilderPipeline;

	std::unique_ptr<D3DComputePipeline> m_AABBBuilderPipeline;
	std::unique_ptr<D3DComputePipeline> m_BrickEvaluatorPipeline;


	// This fence is used to store when the last submitted work is complete
	UINT64 m_PreviousWorkFence = 0;
};