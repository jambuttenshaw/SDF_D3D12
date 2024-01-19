#pragma once

using Microsoft::WRL::ComPtr;

#include "Renderer/Memory/MemoryAllocator.h"


template <typename T>
class RWStructuredBuffer
{
public:
	RWStructuredBuffer() = default;
	~RWStructuredBuffer()
	{
		if (m_UAV.IsValid())
			m_UAV.Free();
	}

	DISALLOW_COPY(RWStructuredBuffer)
	DEFAULT_MOVE(RWStructuredBuffer)

	// Getters
	inline D3D12_GPU_VIRTUAL_ADDRESS GetAddress() const { return m_StructuredBuffer->GetGPUVirtualAddress(); }
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetUAV() const { return m_UAV.GetGPUHandle(); }

	void Allocate(ID3D12Device* device, UINT elementCount, D3D12_RESOURCE_STATES initialResourceState, const wchar_t* resourceName = nullptr)
	{
		ASSERT(elementCount > 0, "Cannot allocate buffer with 0 elements");

		m_ElementCount = elementCount;
		const UINT64 bufferSize = elementCount * sizeof(T);

		// Allocate buffer resource
		const auto defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		const auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		THROW_IF_FAIL(device->CreateCommittedResource(
			&defaultHeap,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			initialResourceState,
			nullptr,
			IID_PPV_ARGS(&m_StructuredBuffer)));
		if (resourceName)
		{
			THROW_IF_FAIL(m_StructuredBuffer->SetName(resourceName));
		}
	}

	void CreateUAV(ID3D12Device* device, DescriptorHeap* descriptorHeap)
	{
		// Allocate descriptor for UAV
		m_UAV = descriptorHeap->Allocate(1);
		ASSERT(m_UAV.IsValid(), "Failed to allocate UAV descriptor");

		// Create UAV
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = m_ElementCount;
		uavDesc.Buffer.StructureByteStride = sizeof(T);
		uavDesc.Buffer.CounterOffsetInBytes = 0;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		device->CreateUnorderedAccessView(
			m_StructuredBuffer.Get(),
			nullptr,
			&uavDesc,
			m_UAV.GetCPUHandle());
	}

private:
	ComPtr<ID3D12Resource> m_StructuredBuffer;

	UINT m_ElementCount = 0;
	DescriptorAllocation m_UAV;
};
