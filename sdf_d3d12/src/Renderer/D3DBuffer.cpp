#include "pch.h"
#include "D3DBuffer.h"

#include "D3DGraphicsContext.h"


D3DConstantBuffer::D3DConstantBuffer(void* initialData, UINT size)
	: m_BufferSize(size)
{
	ASSERT(m_BufferSize % 256 == 0, "Constant buffers must be 256 byte aligned");

	const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
	const auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(static_cast<UINT64>(m_BufferSize * D3DGraphicsContext::GetBackBufferCount()));

	THROW_IF_FAIL(g_D3DGraphicsContext->GetDevice()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_Buffer)));

	// Create the buffer views
	m_CBVs = g_D3DGraphicsContext->GetSRVHeap()->Allocate(D3DGraphicsContext::GetBackBufferCount());
	ASSERT(m_CBVs.IsValid(), "Failed to allocate CBVs");

	for (UINT n = 0; n < D3DGraphicsContext::GetBackBufferCount(); n++)
	{
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = m_Buffer->GetGPUVirtualAddress() + m_BufferSize * n;
		cbvDesc.SizeInBytes = m_BufferSize;

		g_D3DGraphicsContext->GetDevice()->CreateConstantBufferView(&cbvDesc, m_CBVs.GetCPUHandle(n));
	}

	// Map and initialize the constant buffer
	// This buffer is kept mapped for its entire lifetime
	CD3DX12_RANGE readRange(0, 0); // We do not intend on reading from this resource on the CPU
	THROW_IF_FAIL(m_Buffer->Map(0, &readRange, reinterpret_cast<void**>(&m_MappedAddress)));
	for (UINT n = 0; n < D3DGraphicsContext::GetBackBufferCount(); n++)
		memcpy(m_MappedAddress + static_cast<size_t>(n * m_BufferSize), initialData, m_BufferSize);
}

D3DConstantBuffer::~D3DConstantBuffer()
{
	g_D3DGraphicsContext->GetSRVHeap()->Free(m_CBVs);
}

D3D12_GPU_DESCRIPTOR_HANDLE D3DConstantBuffer::GetCBV() const
{
	return m_CBVs.GetGPUHandle(g_D3DGraphicsContext->GetCurrentBackBuffer());
}

void D3DConstantBuffer::CopyData(void* data) const
{
	memcpy(m_MappedAddress + static_cast<size_t>(g_D3DGraphicsContext->GetCurrentBackBuffer() * m_BufferSize), data, m_BufferSize);
}
