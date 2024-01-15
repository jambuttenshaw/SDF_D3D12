#pragma once

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
class D3DUploadBuffer
{
public:
	D3DUploadBuffer() = default;
	D3DUploadBuffer(ID3D12Device* device, UINT elementCount, UINT instanceCount, UINT alignment, const wchar_t* name)
	{
		Allocate(device, elementCount, instanceCount, alignment, name);
	}
	~D3DUploadBuffer()
	{
		// Un-map
		if (m_UploadBuffer != nullptr)
			m_UploadBuffer->Unmap(0, nullptr);
		m_MappedData = nullptr;
	}

	void Allocate(ID3D12Device* device, UINT elementCount, UINT instanceCount, UINT alignment, const wchar_t* name)
	{
		ASSERT(elementCount > 0, "Cannot create a buffer with 0 elements!");
		ASSERT(instanceCount > 0, "Cannot create a buffer with 0 instances!");

		m_ElementCount = elementCount;
		m_InstanceCount = instanceCount;
		m_Alignment = alignment;

		m_ElementStride = sizeof(T);

		if (alignment > 0)
		{
			// Align m_ElementStride to 256 bytes
			m_ElementStride = Align(m_ElementStride, m_Alignment);
		}

		m_InstanceStride = m_ElementStride * m_ElementCount;

		const UINT64 width = m_InstanceStride * m_InstanceCount;

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
	inline D3D12_GPU_VIRTUAL_ADDRESS GetAddressOfElement(UINT elementIndex, UINT instanceIndex) const
	{
		ASSERT(elementIndex < m_ElementCount && instanceIndex < m_InstanceCount, "Out of bounds access");
		return m_UploadBuffer->GetGPUVirtualAddress() + GetIndex(elementIndex, instanceIndex);
	}
	inline UINT GetElementStride() const { return m_ElementStride; }
	inline UINT GetElementCount() const { return m_ElementCount; }
	inline UINT GetInstanceStride() const { return m_InstanceStride; }
	inline UINT GetInstanceCount() const { return m_InstanceCount; }

	void CopyElement(UINT elementIndex, UINT instanceIndex, const T& data)
	{
		ASSERT(elementIndex < m_ElementCount && instanceIndex < m_InstanceCount, "Out of bounds access");
		memcpy(&m_MappedData[GetIndex(elementIndex, instanceIndex)], &data, sizeof(T));
	}
	void CopyElements(UINT start, UINT count, UINT instanceIndex, const T* const data)
	{
		ASSERT(start + count - 1 < m_ElementCount && instanceIndex < m_InstanceCount, "Out of bounds access");
		memcpy(&m_MappedData[GetIndex(start, instanceIndex)], data, count * sizeof(T));
	}

private:
	inline UINT GetIndex(UINT elementIndex, UINT instanceIndex) const
	{
		return instanceIndex * m_InstanceStride + elementIndex * m_ElementStride;
	}

private:
	ComPtr<ID3D12Resource> m_UploadBuffer;
	UINT8* m_MappedData = nullptr;

	UINT m_ElementStride = 0;
	UINT m_ElementCount = 0;

	UINT m_InstanceStride = 0;
	UINT m_InstanceCount = 0;

	UINT m_Alignment = 0;
};


class D3DUAVBuffer
{
public:
	D3DUAVBuffer() = default;

	inline ID3D12Resource* GetResource() const { return m_UAVBuffer.Get(); }
	inline D3D12_GPU_VIRTUAL_ADDRESS GetAddress() const { return m_UAVBuffer->GetGPUVirtualAddress(); }

	void Allocate(ID3D12Device* device, UINT64 bufferSize, D3D12_RESOURCE_STATES initialResourceState, const wchar_t* name = nullptr)
	{
		// Create buffer
		const CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_DEFAULT);
		const auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		THROW_IF_FAIL(device->CreateCommittedResource(
			&uploadHeap,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			initialResourceState,
			nullptr,
			IID_PPV_ARGS(&m_UAVBuffer)));
		if (name)
			m_UAVBuffer->SetName(name);
	}

private:
	ComPtr<ID3D12Resource> m_UAVBuffer;
};
