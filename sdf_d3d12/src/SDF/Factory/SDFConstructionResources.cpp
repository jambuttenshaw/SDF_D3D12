#include "pch.h"
#include "SDFConstructionResources.h"

#include "Framework/Math.h"
#include "Renderer/D3DGraphicsContext.h"


void SDFConstructionResources::AllocateResources(UINT brickCapacity, const SDFEditList& editList, float evalSpaceSize)
{
	const auto device = g_D3DGraphicsContext->GetDevice();

	// Allocate resources that don't depend on brick capacity only once
	if (!m_Allocated)
	{
		m_Allocated = true;

		for (auto& counter : m_SubBrickCounters)
		{
			counter.Allocate(device, L"SDF Factory Counters");
		}

		m_BrickUpload.Allocate(device, 1, 0, L"Brick upload");
		m_CounterReadback.Allocate(device, 1, 0, L"Counters Readback");

		m_CommandBuffer.Allocate(device, s_NumCommands * sizeof(D3D12_DISPATCH_ARGUMENTS), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"Indirect Dispatch Command Buffer");
	}

	// Allocate new brick-capacity-dependent resources whenever the required brick capacity increases
	if (brickCapacity > m_BrickCapacity)
	{
		ASSERT(brickCapacity > 0, "Invalid brick capcity");
		m_BrickCapacity = brickCapacity;

		for (auto& buffer : m_BrickBuffers)
		{
			buffer.Allocate(device, m_BrickCapacity, D3D12_RESOURCE_STATE_COMMON, true, L"Brick buffer");
		}

		const UINT64 width = m_BrickCapacity * sizeof(UINT);
		const UINT64 blockWidth = (width + 63) / 64;
		m_SubBrickCountBuffer.Allocate(device, width, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"Sub brick count buffer");
		m_BlockPrefixSumsBuffer.Allocate(device, blockWidth, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"Block prefix sums buffer");
		m_PrefixSumsBuffer.Allocate(device, width, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"Prefix sums buffer");
	}

	// Allocate a new edit buffer whenever the required edit capacity increases
	if (editList.GetEditCount() > m_EditBuffer.GetMaxEdits())
	{
		m_EditBuffer.Allocate(editList.GetEditCount());
	}

	// Always populate the resources with new data
	m_EditBuffer.Populate(editList);

	// Populate initial constant buffer params
	m_BuildParamsCB.SDFEditCount = editList.GetEditCount();
	// The brick size will be different for each dispatch
	m_BuildParamsCB.BrickSize = evalSpaceSize; // size of entire evaluation space
	m_BuildParamsCB.SubBrickSize = m_BuildParamsCB.BrickSize / 4.0f; // brick size will quarter with each dispatch
	m_BuildParamsCB.EvalSpaceSize = evalSpaceSize; // This will not get reduced between iterations

	// Create and upload initial brick
	Brick initialBrick;
	initialBrick.TopLeft_EvalSpace = {
		-0.5f * evalSpaceSize,
		-0.5f * evalSpaceSize,
		-0.5f * evalSpaceSize
	};
	m_BrickUpload.CopyElement(0, initialBrick);

}


void SDFConstructionResources::SwapBuffersAndRefineBrickSize()
{
	m_CurrentReadBuffers = 1 - m_CurrentReadBuffers;

	m_BuildParamsCB.BrickSize = m_BuildParamsCB.SubBrickSize;
	m_BuildParamsCB.SubBrickSize = m_BuildParamsCB.BrickSize / 4.0f;
}
