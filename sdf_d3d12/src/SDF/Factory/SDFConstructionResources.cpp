#include "pch.h"
#include "SDFConstructionResources.h"

#include "Renderer/D3DGraphicsContext.h"


SDFConstructionResources::SDFConstructionResources(UINT brickCapacity)
	: m_BrickCapacity(brickCapacity)
{
	ASSERT(m_BrickCapacity > 0, "Invalid brick capcity");
}


void SDFConstructionResources::AllocateResources(const SDFEditList& editList, float evalSpaceSize)
{
	ASSERT(!m_Allocated, "Resources have already been allocated!");
	m_Allocated = true;

	const auto device = g_D3DGraphicsContext->GetDevice();

	m_EditBuffer.Allocate(editList.GetEditCount());
	m_EditBuffer.Populate(editList);


	// Populate initial constant buffer params
	m_BuildParamsCB.SDFEditCount = editList.GetEditCount();
	// The brick size will be different for each dispatch
	m_BuildParamsCB.BrickSize = evalSpaceSize; // size of entire evaluation space
	m_BuildParamsCB.SubBrickSize = m_BuildParamsCB.BrickSize / 4.0f; // brick size will quarter with each dispatch


	for (auto& buffer : m_BrickBuffers)
	{
		buffer.Allocate(device, m_BrickCapacity, D3D12_RESOURCE_STATE_COMMON, true, L"Brick buffer");
	}
	for (auto& counter : m_SubBrickCounters)
	{
		counter.Allocate(device, L"SDF Factory Counters");
	}


	const UINT64 width = m_BrickCapacity * sizeof(UINT);
	const UINT64 blockWidth = (width + 63) / 64;
	m_SubBrickCountBuffer.Allocate(device, width, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"Sub brick count buffer");
	m_BlockPrefixSumsBuffer.Allocate(device, blockWidth, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"Block prefix sums buffer");
	m_PrefixSumsBuffer.Allocate(device, width, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"Prefix sums buffer");

	m_BrickUpload.Allocate(device, 1, 0, L"Brick upload");
	m_CounterReadback.Allocate(device, 1, 0, L"Counters Readback");

	// Create and upload initial brick
	Brick initialBrick;
	initialBrick.TopLeft_EvalSpace = {
		-0.5f * evalSpaceSize,
		-0.5f * evalSpaceSize,
		-0.5f * evalSpaceSize
	};
	m_BrickUpload.CopyElement(0, initialBrick);

	m_CommandBuffer.Allocate(device, s_NumCommands * sizeof(D3D12_DISPATCH_ARGUMENTS), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"Indirect Dispatch Command Buffer");;
}


void SDFConstructionResources::SwapBuffersAndRefineBrickSize()
{
	m_CurrentReadBuffers = 1 - m_CurrentReadBuffers;

	m_BuildParamsCB.BrickSize = m_BuildParamsCB.SubBrickSize;
	m_BuildParamsCB.SubBrickSize = m_BuildParamsCB.BrickSize / 4.0f;
}

