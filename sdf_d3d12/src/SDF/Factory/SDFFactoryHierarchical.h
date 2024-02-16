#pragma once

#include "Core.h"

#include "Renderer/D3DPipeline.h"
#include "Renderer/Buffer/UploadBuffer.h"

#include "SDFConstructionResources.h"


using Microsoft::WRL::ComPtr;

class SDFObject;
class SDFEditList;

namespace SDFFactoryPipeline
{
	enum Value
	{
		BrickCounter = 0,
		ScanGroupCountCalculator,
		ScanBlocks,
		ScanBlockSums,
		SumScans,
		BrickBuilder,
		EditTester,
		MortonEnumerator,
		AABBBuilder,
		BrickEvaluator,
		Count
	};
}

class SDFFactoryHierarchical
{
	inline static constexpr size_t s_PipelineCount = SDFFactoryPipeline::Count;
	using PipelineSet = std::array<std::unique_ptr<D3DComputePipeline>, s_PipelineCount>;

public:
	SDFFactoryHierarchical();
	virtual ~SDFFactoryHierarchical() = default;

	DISALLOW_COPY(SDFFactoryHierarchical)
	DEFAULT_MOVE(SDFFactoryHierarchical)

	virtual void BakeSDFSync(const std::wstring& pipelineName, SDFObject* object, const SDFEditList& editList);

	inline void SetMaxBrickBuildIterations(UINT maxIterations) { m_MaxBrickBuildIterations = maxIterations; }
	inline UINT GetMaxBrickBuildIterations() const { return m_MaxBrickBuildIterations; }

protected:

	void CreatePipelineSet(const std::wstring& name, const std::vector<std::wstring>& defines);

	void PerformSDFBake_CPUBlocking(const std::wstring& pipelineName, SDFObject* object, const SDFEditList& editList);


private:
	// SDF Bake stages
	// Split into functions for readability and easy multi-threading
	void BuildCommandList_Setup(const PipelineSet& pipeline, SDFObject* object, SDFConstructionResources& resources) const;
	void BuildCommandList_HierarchicalBrickBuilding(const PipelineSet& pipeline, SDFObject* object, SDFConstructionResources& resources, UINT maxIterations) const;
	void BuildCommandList_BrickEvaluation(const PipelineSet& pipeline, SDFObject* object, SDFConstructionResources& resources) const;


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
	UploadBuffer<UINT32> m_CounterUpload64;	// Used to set a counter to 1

	// Pipelines
	std::map<std::wstring, PipelineSet> m_Pipelines;


	// This fence is used to store when the last submitted work is complete
	UINT64 m_PreviousWorkFence = 0;

	std::atomic<UINT> m_MaxBrickBuildIterations = -1;

	// Temporary resources used to construct an object
	SDFConstructionResources m_Resources;
};
