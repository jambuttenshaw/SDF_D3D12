#pragma once

using Microsoft::WRL::ComPtr;


class UAVBuffer
{
public:
	UAVBuffer() = default;

	DISALLOW_COPY(UAVBuffer)
		DEFAULT_MOVE(UAVBuffer)

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
