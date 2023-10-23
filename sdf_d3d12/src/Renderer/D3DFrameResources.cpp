#include "pch.h"
#include "D3DFrameResources.h"

#include "D3DGraphicsContext.h"


D3DFrameResources::D3DFrameResources()
{
	// Create command allocator
	THROW_IF_FAIL(g_D3DGraphicsContext->GetDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_CommandAllocator)));

	// Create per-pass and per-object constant buffer
	m_PassCB = std::make_unique<D3DUploadBuffer<PassCBType>>(1, true);
	m_ObjectCB = std::make_unique<D3DUploadBuffer<ObjectCBType>>(1, true);

	// Create constant buffer views
	// Allocate enough descriptors from the heap
	// TODO: For now this enough for 1 object
	m_CBVs = g_D3DGraphicsContext->GetSRVHeap()->Allocate(2);
	ASSERT(m_CBVs.IsValid(), "Failed to alloc descriptors");

	m_PassCBV = 1;	// = num objects (pass cbv comes after object cbv's)

	D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
	desc.BufferLocation = m_PassCB->GetAddressOfElement(0);
	desc.SizeInBytes = m_PassCB->GetElementSize();
	// Create descriptors for pass cb
	g_D3DGraphicsContext->GetDevice()->CreateConstantBufferView(&desc, m_CBVs.GetCPUHandle(m_PassCBV));

	// Create object descriptors
	desc.SizeInBytes = m_ObjectCB->GetElementSize();
	for (UINT objectIndex = 0; objectIndex < 1; ++objectIndex)
	{
		desc.BufferLocation = m_ObjectCB->GetAddressOfElement(objectIndex);
		g_D3DGraphicsContext->GetDevice()->CreateConstantBufferView(&desc, m_CBVs.GetCPUHandle(objectIndex));
	}
}

D3DFrameResources::~D3DFrameResources()
{
	// Free descriptors from heap
	g_D3DGraphicsContext->GetSRVHeap()->Free(m_CBVs);
}
