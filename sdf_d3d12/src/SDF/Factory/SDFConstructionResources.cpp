#include "pch.h"
#include "SDFConstructionResources.h"

#include "Framework/Math.h"
#include "Renderer/D3DGraphicsContext.h"



UINT expandBits(UINT v)
{
	v = (v * 0x00010001u) & 0xFF0000FFu;
	v = (v * 0x00000101u) & 0x0F00F00Fu;
	v = (v * 0x00000011u) & 0xC30C30C3u;
	v = (v * 0x00000005u) & 0x49249249u;
	return v;
}

// Calculates a 30-bit Morton code for the
// given 3D point located within the range [0,1023].
UINT morton3Du(XMUINT3 p)
{
	p.x = min(p.x, 1023u);
	p.y = min(p.y, 1023u);
	p.z = min(p.z, 1023u);
	unsigned int xx = expandBits(p.x);
	unsigned int yy = expandBits(p.y);
	unsigned int zz = expandBits(p.z);
	return xx * 4 + yy * 2 + zz;
}


void SDFConstructionResources::AllocateResources(UINT brickCapacity, const SDFEditList& editList, float evalSpaceSize)
{
	const auto device = g_D3DGraphicsContext->GetDevice();

	// Allocate resources that don't depend on brick capacity only once
	if (!m_Allocated)
	{
		m_Allocated = true;

		m_IndexCounter.Allocate(device, L"Index Counters");

		constexpr UINT dependencyCount = 1024 * 1023;
		m_EditDependencies.Allocate(device, dependencyCount * sizeof(UINT), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"Edit Dependency Indices");
		m_EditDependenciesUpload.Allocate(device, dependencyCount, 0, L"Edit Dependency Upload");
		m_EditDependenciesUpload.SetElements(0, m_EditDependenciesUpload.GetElementCount(), 0);

		for (auto& counter : m_SubBrickCounters)
		{
			counter.Allocate(device, L"Sub Brick Counters");
		}

		m_BrickUpload.Allocate(device, 64, 0, L"Brick upload");
		m_BrickCounterReadback.Allocate(device, 1, 0, L"Brick Counter Readback");

		m_IndexCounterReadback.Allocate(device, 1, 0, L"Index Counter Readback");

		m_CommandBuffer.Allocate(device, s_NumCommands * sizeof(D3D12_DISPATCH_ARGUMENTS), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"Indirect Dispatch Command Buffer");
	}

	// Allocate new brick-capacity-dependent resources whenever the required brick capacity increases
	if (brickCapacity > m_BrickCapacity)
	{
		ASSERT(brickCapacity >= 64, "Invalid brick capcity");
		m_BrickCapacity = brickCapacity;

		for (auto& buffer : m_BrickBuffers)
		{
			buffer.Allocate(device, m_BrickCapacity, D3D12_RESOURCE_STATE_COMMON, true, L"Brick buffer");
		}

		const UINT64 width = m_BrickCapacity * sizeof(UINT);
		const UINT64 blockWidth = (width + 63) / 64;
		m_SubBrickCountBuffer.Allocate(device, width, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"Sub brick count buffer");
		m_BlockPrefixSumsBuffer.Allocate(device, blockWidth, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"Block prefix sums buffer");
		m_BlockPrefixSumsOutputBuffer.Allocate(device, blockWidth, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"Block prefix sums output buffer");
		m_PrefixSumsBuffer.Allocate(device, width, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"Prefix sums buffer");
	}

	// Allocate a new edit buffer whenever the required edit capacity increases
	if (editList.GetEditCount() > m_EditBuffer.GetMaxEdits())
	{
		m_EditBuffer.Allocate(editList.GetEditCount());

		// Worst case: every brick uses every edit
		// Max Indices = Bricks * Edits
		const UINT indexBufferCapacity = editList.GetEditCount() * m_BrickCapacity;
		const UINT64 indexBufferWidth = indexBufferCapacity * sizeof(UINT16);
		for (auto& indexBuffer : m_IndexBuffers)
		{
			indexBuffer.Allocate(device, indexBufferWidth, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"Index Buffer");
		}

		// Upload only needs to contain data for the first 64 bricks
		m_IndexUpload.Allocate(device, editList.GetEditCount(), 0, L"Index Upload");

		// Populate index buffer
		for (UINT edit = 0; edit < editList.GetEditCount(); edit++)
		{
			m_IndexUpload.CopyElement(edit, edit);
		}
	}

	// Always populate the resources with new data
	m_EditBuffer.Populate(editList);

	// Populate initial constant buffer params
	m_BuildParamsCB.SDFEditCount = editList.GetEditCount();
	// The brick size will be different for each dispatch
	m_BuildParamsCB.BrickSize = evalSpaceSize / 4.0f; // size of entire evaluation space
	m_BuildParamsCB.SubBrickSize = m_BuildParamsCB.BrickSize / 4.0f; // brick size will quarter with each dispatch
	m_BuildParamsCB.EvalSpaceSize = evalSpaceSize; // This will not get reduced between iterations

	// Create and upload initial bricks
	Brick initialBrick;
	initialBrick.SubBrickMask = { 0, 0 };
	initialBrick.IndexOffset = 0;
	initialBrick.IndexCount = editList.GetEditCount();

	for (UINT x = 0; x < 4; x++)
	for (UINT y = 0; y < 4; y++)
	for (UINT z = 0; z < 4; z++)
	{
		const UINT index = morton3Du({ x, y, z });

		initialBrick.TopLeft = {
			-0.5f * evalSpaceSize + (static_cast<float>(x) * m_BuildParamsCB.BrickSize),
			-0.5f * evalSpaceSize + (static_cast<float>(y) * m_BuildParamsCB.BrickSize),
			-0.5f * evalSpaceSize + (static_cast<float>(z) * m_BuildParamsCB.BrickSize)
		};

		m_BrickUpload.CopyElement(index, initialBrick);
	}
}


void SDFConstructionResources::SwapBuffersAndRefineBrickSize()
{
	m_CurrentReadBuffers = 1 - m_CurrentReadBuffers;

	m_BuildParamsCB.BrickSize = m_BuildParamsCB.SubBrickSize;
	m_BuildParamsCB.SubBrickSize = m_BuildParamsCB.BrickSize / 4.0f;
}
