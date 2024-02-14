#pragma once

#include "Core.h"

using Microsoft::WRL::ComPtr;


class CounterResource
{
public:
	CounterResource(UINT numCounters = 1) : m_NumCounters(numCounters) {}
	~CounterResource() = default;

	DISALLOW_COPY(CounterResource)
	DEFAULT_MOVE(CounterResource)

	// Getters
	inline UINT GetNumCounters() const { return m_NumCounters; }

	inline ID3D12Resource* GetResource() const { return m_CounterResource.Get(); }
	inline D3D12_GPU_VIRTUAL_ADDRESS GetAddress(UINT counterIndex = 0) const
	{
		ASSERT(counterIndex < m_NumCounters, "Invalid counter index");
		return m_CounterResource->GetGPUVirtualAddress() + counterIndex * sizeof(UINT);
	}

	void Allocate(ID3D12Device* device, const wchar_t* resourceName = nullptr)
	{
		const UINT64 bufferSize = m_NumCounters * sizeof(UINT32);

		const auto defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		const auto counterDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		THROW_IF_FAIL(device->CreateCommittedResource(
			&defaultHeap,
			D3D12_HEAP_FLAG_NONE,
			&counterDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&m_CounterResource)));
		if (resourceName)
		{
			THROW_IF_FAIL(m_CounterResource->SetName(resourceName));
		}
	}

	void SetValue(ID3D12GraphicsCommandList* commandList, ID3D12Resource* src, D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState) const
	{
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_CounterResource.Get(), beforeState, D3D12_RESOURCE_STATE_COPY_DEST);
		commandList->ResourceBarrier(1, &barrier);

		commandList->CopyResource(m_CounterResource.Get(), src);

		barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_CounterResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, afterState);
		commandList->ResourceBarrier(1, &barrier);
	}

	void ReadValue(ID3D12GraphicsCommandList* commandList, ID3D12Resource* dest, D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState) const
	{
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_CounterResource.Get(), beforeState, D3D12_RESOURCE_STATE_COPY_SOURCE);
		commandList->ResourceBarrier(1, &barrier);

		commandList->CopyResource(dest, m_CounterResource.Get());

		barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_CounterResource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, afterState);
		commandList->ResourceBarrier(1, &barrier);
	}

private:
	ComPtr<ID3D12Resource> m_CounterResource;
	UINT m_NumCounters = 0;

};
