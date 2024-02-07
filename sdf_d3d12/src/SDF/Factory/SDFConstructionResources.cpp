#include "pch.h"
#include "SDFConstructionResources.h"

#include "Renderer/D3DGraphicsContext.h"


SDFConstructionResources::SDFConstructionResources(UINT brickCapacity)
	: m_BrickCapacity(brickCapacity)
{
	ASSERT(m_BrickCapacity > 0, "Invalid brick capcity");
}


void SDFConstructionResources::AllocateResources(const Brick& initialBrick)
{
	const auto device = g_D3DGraphicsContext->GetDevice();

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
	m_ScannedBlockPrefixSumsBuffer.Allocate(device, blockWidth, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"Scanned block prefix sums buffer");
	m_PrefixSumsBuffer.Allocate(device, width, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"Prefix sums buffer");


	m_BrickUpload.Allocate(device, 1, 0, L"Brick upload");
	m_CounterUploadZero.Allocate(device, 1, 0, L"Counters Upload Zero");
	m_CounterUploadOne.Allocate(device, 1, 0, L"Counters Upload One");
	m_CounterReadback.Allocate(device, 1, 0, L"Counters Readback");

	m_BrickUpload.CopyElement(0, initialBrick);

	m_CounterUploadZero.CopyElement(0, 0);
	m_CounterUploadOne.CopyElement(0, 1);
}
