#pragma once

using Microsoft::WRL::ComPtr;


class DefaultBuffer
{
public:
	DefaultBuffer() = default;
	virtual ~DefaultBuffer() = default;

	DISALLOW_COPY(DefaultBuffer)
	DEFAULT_MOVE(DefaultBuffer)

	inline ID3D12Resource* GetResource() const { return m_Buffer.Get(); }
	inline D3D12_GPU_VIRTUAL_ADDRESS GetAddress() const { return m_Buffer->GetGPUVirtualAddress(); }
	inline ID3D12Resource* TransferResource() { return m_Buffer.Detach(); }

	void Allocate(ID3D12Device* device, UINT64 bufferSize, D3D12_RESOURCE_STATES initialResourceState, D3D12_RESOURCE_FLAGS resourceFlags, const wchar_t* resourceName = nullptr)
	{
		// Create buffer
		const CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
		const auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, resourceFlags);
		THROW_IF_FAIL(device->CreateCommittedResource(
			&defaultHeap,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			initialResourceState,
			nullptr,
			IID_PPV_ARGS(&m_Buffer)));

		if (resourceName)
		{
			m_Buffer->SetName(resourceName);
		}
	}

private:
	ComPtr<ID3D12Resource> m_Buffer;
};
