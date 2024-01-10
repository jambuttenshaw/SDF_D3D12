#include "pch.h"
#include "D3DFrameResources.h"

#include "D3DGraphicsContext.h"


D3DFrameResources::D3DFrameResources()
{
	// Create command allocator
	THROW_IF_FAIL(g_D3DGraphicsContext->GetDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_CommandAllocator)));

	const UINT numObjects = D3DGraphicsContext::GetMaxObjectCount();

	// Create per-pass and per-object constant buffer
	m_PassCB = std::make_unique<D3DUploadBuffer<PassCBType>>(g_D3DGraphicsContext->GetDevice(), 1, 256, L"Pass Constant Buffer");	
	m_ObjectCB = std::make_unique<D3DUploadBuffer<ObjectCBType>>(g_D3DGraphicsContext->GetDevice(), numObjects, 256, L"Object Constant Buffer");

	// Create constant buffer views
	// Allocate enough descriptors from the heap
	m_CBVs = g_D3DGraphicsContext->GetSRVHeap()->Allocate(numObjects + 2);	// one for each object + one for entire object buffer + pass cb
	ASSERT(m_CBVs.IsValid(), "Failed to alloc descriptors");

	m_AllObjectsCBV = numObjects;	// = num objects (all objects cbv comes after object cbv's)
	m_PassCBV = numObjects + 1;		// = num objects + 1 (pass cbv comes after object cbv's and all objects cbv)

	D3D12_CONSTANT_BUFFER_VIEW_DESC desc;

	// Create descriptors for pass cb
	desc.BufferLocation = m_PassCB->GetAddressOfElement(0);
	desc.SizeInBytes = m_PassCB->GetElementSize();
	g_D3DGraphicsContext->GetDevice()->CreateConstantBufferView(&desc, m_CBVs.GetCPUHandle(m_PassCBV));

	// Create descriptor for entire object buffer
	desc.BufferLocation = m_ObjectCB->GetAddressOfElement(0);
	desc.SizeInBytes = m_ObjectCB->GetElementSize() * numObjects;
	g_D3DGraphicsContext->GetDevice()->CreateConstantBufferView(&desc, m_CBVs.GetCPUHandle(m_AllObjectsCBV));

	// Create object descriptors
	desc.SizeInBytes = m_ObjectCB->GetElementSize();
	for (UINT objectIndex = 0; objectIndex < numObjects; ++objectIndex)
	{
		desc.BufferLocation = m_ObjectCB->GetAddressOfElement(objectIndex);
		g_D3DGraphicsContext->GetDevice()->CreateConstantBufferView(&desc, m_CBVs.GetCPUHandle(objectIndex));
	}
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
