#pragma once

#include "Core.h"

using Microsoft::WRL::ComPtr;


class CounterResource
{
public:
	CounterResource() = default;
	~CounterResource() = default;

	DISALLOW_COPY(CounterResource)
	DEFAULT_MOVE(CounterResource)

	// Getters
	inline ID3D12Resource* GetResource() const { return m_CounterResource.Get(); }
	inline D3D12_GPU_VIRTUAL_ADDRESS GetAddress() const { return m_CounterResource->GetGPUVirtualAddress(); }

	void Allocate(ID3D12Device* device, const wchar_t* resourceName = nullptr)
	{
		constexpr UINT64 bufferSize = sizeof(UINT32);

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
			std::wstring counterName(resourceName);
			counterName += L"_Counter";
			THROW_IF_FAIL(m_CounterResource->SetName(counterName.c_str()));
		}
	}

	void SetValue(ID3D12GraphicsCommandList* commandList, ID3D12Resource* src) const
	{
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_CounterResource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);
		commandList->ResourceBarrier(1, &barrier);

		commandList->CopyResource(m_CounterResource.Get(), src);

		barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_CounterResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		commandList->ResourceBarrier(1, &barrier);
	}

	void ReadValue(ID3D12GraphicsCommandList* commandList, ID3D12Resource* dest) const
	{
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_CounterResource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
		commandList->ResourceBarrier(1, &barrier);

		commandList->CopyResource(dest, m_CounterResource.Get());

		barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_CounterResource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		commandList->ResourceBarrier(1, &barrier);
	}


private:
	ComPtr<ID3D12Resource> m_CounterResource;

};
