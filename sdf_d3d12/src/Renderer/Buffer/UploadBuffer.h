#pragma once

#include "Core.h"

using Microsoft::WRL::ComPtr;


/**
 *	An UploadBuffer stores an array of elements of type T in an upload heap
 *	The contents are mapped for CPU modification,
 *	but care must be taken to not modify the buffer while the GPU may be accessing it.
 *
 *	This class can be used for both generic upload buffers and constant buffers.
 *	Constant buffer elements must be 256-byte aligned
 */
template<typename T>
class UploadBuffer
{
public:
	UploadBuffer() = default;
	~UploadBuffer()
	{
		// Un-map
		if (m_UploadBuffer != nullptr)
			m_UploadBuffer->Unmap(0, nullptr);
		m_MappedData = nullptr;
	}

	// Disallow copying
	DISALLOW_COPY(UploadBuffer)
	// Allow moving
	DEFAULT_MOVE(UploadBuffer)

	void Allocate(ID3D12Device* device, UINT elementCount, UINT alignment, const wchar_t* name)
	{
		ASSERT(elementCount > 0, "Cannot create a buffer with 0 elements!");

		m_ElementCount = elementCount;
		m_Alignment = alignment;

		m_ElementStride = sizeof(T);

		if (alignment > 0)
		{
			// Align m_ElementStride to 256 bytes
			m_ElementStride = static_cast<UINT>(Align(m_ElementStride, m_Alignment));
		}

		const UINT64 width = m_ElementStride * m_ElementCount;

		// Create buffer
		const CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
		const auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(width);
		THROW_IF_FAIL(device->CreateCommittedResource(
			&uploadHeap,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_UploadBuffer)));
		if (name)
			m_UploadBuffer->SetName(name);

		// Map buffer
		THROW_IF_FAIL(m_UploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_MappedData)));

		// We do not need to un-map the resource for its entire lifetime
		// However, be careful to not write to the buffer from the CPU while the GPU may be reading from it!
	}


	// Getters
	inline ID3D12Resource* GetResource() const { return m_UploadBuffer.Get(); }
	inline D3D12_GPU_VIRTUAL_ADDRESS GetAddressOfElement(UINT elementIndex) const
	{
		ASSERT(elementIndex < m_ElementCount, "Out of bounds access");
		return m_UploadBuffer->GetGPUVirtualAddress() + elementIndex * m_ElementStride;
	}
	inline UINT GetElementStride() const { return m_ElementStride; }
	inline UINT GetElementCount() const { return m_ElementCount; }

	void CopyElement(UINT elementIndex, const T& data) const
	{
		ASSERT(elementIndex < m_ElementCount, "Out of bounds access");
		memcpy(&m_MappedData[elementIndex * m_ElementStride], &data, sizeof(T));
	}
	void CopyElements(UINT start, UINT count, const T* const data) const
	{
		ASSERT(start + count - 1 < m_ElementCount, "Out of bounds access");
		memcpy(&m_MappedData[start * m_ElementStride], data, count * sizeof(T));
	}

private:
	ComPtr<ID3D12Resource> m_UploadBuffer;
	UINT8* m_MappedData = nullptr;

	UINT m_ElementStride = 0;
	UINT m_ElementCount = 0;

	UINT m_Alignment = 0;
};
