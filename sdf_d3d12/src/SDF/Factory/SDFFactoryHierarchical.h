#pragma once

#include "Core.h"

#include "Renderer/D3DPipeline.h"
#include "Renderer/Buffer/UploadBuffer.h"
#include "Renderer/Buffer/DefaultBuffer.h"


using Microsoft::WRL::ComPtr;

class SDFObject;
class SDFEditList;
class SDFConstructionResources;


class SDFFactoryHierarchical
{
public:
	SDFFactoryHierarchical();
	virtual ~SDFFactoryHierarchical() = default;

	DISALLOW_COPY(SDFFactoryHierarchical)
	DEFAULT_MOVE(SDFFactoryHierarchical)

	virtual void BakeSDFSync(SDFObject* object, const SDFEditList& editList);

	inline void SetMaxBrickBuildIterations(UINT maxIterations) { m_MaxBrickBuildIterations = maxIterations; }
	inline UINT GetMaxBrickBuildIterations() const { return m_MaxBrickBuildIterations; }

protected:

	void InitializePipelines();

	void PerformSDFBake_CPUBlocking(SDFObject* object, const SDFEditList& editList);


private:
	// SDF Bake stages
	// Split into functions for readability and easy multi-threading
	void BuildCommandList_Setup(SDFObject* object, SDFConstructionResources& resources) const;
	void BuildCommandList_HierarchicalBrickBuilding(SDFObject* object, SDFConstructionResources& resources, UINT maxIterations) const;
	void BuildCommandList_BrickEvaluation(SDFObject* object, SDFConstructionResources& resources) const;


protected:
	// API objects
	ComPtr<ID3D12CommandAllocator> m_CommandAllocator;
	ComPtr<ID3D12GraphicsCommandList> m_CommandList;

	// Indirect execution objects
	ComPtr<ID3D12CommandSignature> m_CommandSignature;
	// The number of commands in the buffer
	inline static constexpr UINT s_NumCommands = 4;
	UploadBuffer<D3D12_DISPATCH_ARGUMENTS> m_CommandUploadBuffer;

	// Upload buffers that can be re-used for any build
	// The counter reset buffers never have their contents changed so they can be part of the factory
	UploadBuffer<UINT32> m_CounterUploadZero;	// Used to set a counter to 0
	UploadBuffer<UINT32> m_CounterUploadOne;	// Used to set a counter to 1

	// Pipelines
	std::unique_ptr<D3DComputePipeline> m_BrickCounterPipeline;

	std::unique_ptr<D3DComputePipeline> m_ScanGroupCountCalculatorPipeline;
	std::unique_ptr<D3DComputePipeline> m_ScanBlocksPipeline;
	std::unique_ptr<D3DComputePipeline> m_ScanBlockSumsPipeline;
	std::unique_ptr<D3DComputePipeline> m_SumScansPipeline;

	std::unique_ptr<D3DComputePipeline> m_BrickBuilderPipeline;

	std::unique_ptr<D3DComputePipeline> m_MortonEnumeratorPipeline;
	std::unique_ptr<D3DComputePipeline> m_AABBBuilderPipeline;
	std::unique_ptr<D3DComputePipeline> m_BrickEvaluatorPipeline;


	// This fence is used to store when the last submitted work is complete
	UINT64 m_PreviousWorkFence = 0;

	std::atomic<UINT> m_MaxBrickBuildIterations = -1;
};
