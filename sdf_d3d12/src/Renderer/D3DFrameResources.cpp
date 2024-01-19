#include "pch.h"
#include "D3DFrameResources.h"

#include "D3DGraphicsContext.h"


D3DFrameResources::D3DFrameResources()
{
	// Create command allocator
	THROW_IF_FAIL(g_D3DGraphicsContext->GetDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_CommandAllocator)));
	D3D_NAME(m_CommandAllocator);

	// Create per-pass and per-object constant buffer
	m_PassCB = std::make_unique<UploadBuffer<PassConstantBuffer>>(g_D3DGraphicsContext->GetDevice(), 1, 1, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, L"Pass Constant Buffer");	

	// Create constant buffer views
	// Allocate enough descriptors from the heap
	m_CBVs = g_D3DGraphicsContext->GetSRVHeap()->Allocate(1);
	ASSERT(m_CBVs.IsValid(), "Failed to alloc descriptors");

	m_PassCBV = 0; // Pass CBV is the first descriptor in the allocation

	D3D12_CONSTANT_BUFFER_VIEW_DESC desc;

	// Create descriptors for pass cb
	desc.BufferLocation = m_PassCB->GetAddressOfElement(0, 0);
	desc.SizeInBytes = m_PassCB->GetElementStride();
	g_D3DGraphicsContext->GetDevice()->CreateConstantBufferView(&desc, m_CBVs.GetCPUHandle(m_PassCBV));
}

D3DFrameResources::~D3DFrameResources()
{
	// Free descriptors from heap
	m_CBVs.Free();
}

void D3DFrameResources::DeferRelease(const ComPtr<IUnknown>& resource)
{
	m_DeferredReleases.push_back(resource);
}

void D3DFrameResources::ProcessDeferrals()
{
	// As long as the ComPtr's held in the collection aren't held anywhere else,
	// then the resources pointed to will be automatically released
	m_DeferredReleases.clear();
}
