#pragma once

#include "D3DGraphicsContext.h"

using Microsoft::WRL::ComPtr;


#ifndef ALIGN_CONSTANT_BUFFER
#define ALIGN_CONSTANT_BUFFER(v) (((v) + 255) & ~(255))
#endif


/**
 *	An UploadBuffer stores an array of elements of type T in an upload heap
 *	The contents are mapped for CPU modification,
 *	but care must be taken to not modify the buffer while the GPU may be accessing it.
 *
 *	This class can be used for both generic upload buffers and constant buffers.
 *	Constant buffer elements must be 256-byte aligned
 */
template<typename T>
class D3DUploadBuffer
{
public:
	D3DUploadBuffer(UINT elementCount, bool isConstantBuffer)
		: m_ElementCount(elementCount)
		, m_IsConstantBuffer(isConstantBuffer)
	{
		ASSERT(m_ElementCount > 0, "Cannot create a buffer with 0 elements!");

		m_ElementStride = sizeof(T);

		if (m_IsConstantBuffer)
		{
			// Align m_ElementStride to 256 bytes
			m_ElementStride = ALIGN_CONSTANT_BUFFER(m_ElementStride);
		}

		// Create buffer
		const CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
		const auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(m_ElementStride * m_ElementCount);
		THROW_IF_FAIL(g_D3DGraphicsContext->GetDevice()->CreateCommittedResource(
			&uploadHeap,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_UploadBuffer)));

		// Map buffer
		THROW_IF_FAIL(m_UploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_MappedData)));

		// We do not need to un-map the resource for its entire lifetime
		// However, be careful to not write to the buffer from the CPU while the GPU may be reading from it!
	}
	~D3DUploadBuffer()
	{
		// Un-map
		if (m_UploadBuffer != nullptr)
			m_UploadBuffer->Unmap(0, nullptr);
		m_MappedData = nullptr;
	}

	// Getters
	inline ID3D12Resource* GetResource() const { return m_UploadBuffer.Get(); }
	inline D3D12_GPU_VIRTUAL_ADDRESS GetAddressOfElement(UINT elementIndex) const
	{
		ASSERT(elementIndex < m_ElementCount, "Out of bounds access");
		return m_UploadBuffer->GetGPUVirtualAddress() + elementIndex * m_ElementStride;
	}
	inline UINT GetElementSize() const { return m_ElementStride; }

	void CopyData(UINT elementIndex, const T& data)
	{
		ASSERT(elementIndex < m_ElementCount, "Out of bounds access");
		memcpy(&m_MappedData[elementIndex * m_ElementStride], &data, sizeof(T));
	}

private:
	ComPtr<ID3D12Resource> m_UploadBuffer;
	UINT8* m_MappedData = nullptr;

	UINT m_ElementStride = 0;
	UINT m_ElementCount = 0;
	bool m_IsConstantBuffer = false;
};
