#pragma once

#include "Core.h"

using Microsoft::WRL::ComPtr;


template<typename T>
class ReadbackBuffer
{
public:
	ReadbackBuffer() = default;
	~ReadbackBuffer() = default;

	// Disallow copying
	DISALLOW_COPY(ReadbackBuffer)
	// Allow moving
	DEFAULT_MOVE(ReadbackBuffer)

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
		const CD3DX12_HEAP_PROPERTIES readbackHeap(D3D12_HEAP_TYPE_READBACK);
		const auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(width);
		THROW_IF_FAIL(device->CreateCommittedResource(
			&readbackHeap,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&m_ReadbackBuffer)));
		if (name)
			m_ReadbackBuffer->SetName(name);
	}

	// Getters
	inline ID3D12Resource* GetResource() const { return m_ReadbackBuffer.Get(); }
	inline UINT GetElementStride() const { return m_ElementStride; }
	inline UINT GetElementCount() const { return m_ElementCount; }

	T ReadElement(UINT elementIndex) const
	{
		ASSERT(elementIndex < m_ElementCount, "Out of bounds access");

		T* pData;
		T value;

		const CD3DX12_RANGE readRange(0, m_ElementCount);

		THROW_IF_FAIL(m_ReadbackBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pData)));
		memcpy(&value, pData + elementIndex, sizeof(T));
		m_ReadbackBuffer->Unmap(0, nullptr);

		return value;
	}

private:
	ComPtr<ID3D12Resource> m_ReadbackBuffer;

	UINT m_ElementStride = 0;
	UINT m_ElementCount = 0;

	UINT m_Alignment = 0;
};
