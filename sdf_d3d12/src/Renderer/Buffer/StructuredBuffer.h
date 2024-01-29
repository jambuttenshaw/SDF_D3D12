#pragma once

#include "DefaultBuffer.h"

using Microsoft::WRL::ComPtr;


template <typename T>
class StructuredBuffer : public DefaultBuffer
{
public:
	StructuredBuffer() = default;
	virtual ~StructuredBuffer() = default;

	DISALLOW_COPY(StructuredBuffer)
	DEFAULT_MOVE(StructuredBuffer)

	inline UINT GetElementCount() const { return m_ElementCount; }
	inline static constexpr UINT GetElementStride() { return sizeof(T); }

	void Allocate(ID3D12Device* device, UINT elementCount, D3D12_RESOURCE_STATES initialResourceState, bool readWrite = false, const wchar_t* resourceName = nullptr)
	{
		ASSERT(elementCount > 0, "Cannot allocate buffer with 0 elements");

		m_ElementCount = elementCount;
		const UINT64 bufferSize = elementCount * sizeof(T);

		DefaultBuffer::Allocate(
			device, 
			bufferSize, 
			initialResourceState, 
			readWrite ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE, 
			resourceName);
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC CreateSRVDesc() const
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = m_ElementCount;
		srvDesc.Buffer.StructureByteStride = sizeof(T);
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		return srvDesc;
	}

	D3D12_UNORDERED_ACCESS_VIEW_DESC CreateUAVDesc() const
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = m_ElementCount;
		uavDesc.Buffer.StructureByteStride = sizeof(T);
		uavDesc.Buffer.CounterOffsetInBytes = 0;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		return uavDesc;
	}

private:
	UINT m_ElementCount = 0;
};
