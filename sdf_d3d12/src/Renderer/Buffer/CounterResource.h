#pragma once

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
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS(&m_CounterResource)));
		if (resourceName)
		{
			std::wstring counterName(resourceName);
			counterName += L"_Counter";
			THROW_IF_FAIL(m_CounterResource->SetName(counterName.c_str()));
		}

		// Allocate resource to write a value to the counter
		const auto uploadHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		const auto uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT32));
		THROW_IF_FAIL(device->CreateCommittedResource(
			&uploadHeap,
			D3D12_HEAP_FLAG_NONE,
			&uploadDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&m_CounterUploadResource)));
		if (resourceName)
		{
			std::wstring counterName(resourceName);
			counterName += L"_CounterUpload";
			THROW_IF_FAIL(m_CounterResource->SetName(counterName.c_str()));
		}

		// Allocate resource to read back the value in the counter
		const auto readbackHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
		const auto readbackDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT32));
		THROW_IF_FAIL(device->CreateCommittedResource(
			&readbackHeap,
			D3D12_HEAP_FLAG_NONE,
			&readbackDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_CounterReadbackResource)));
		if (resourceName)
		{
			std::wstring counterName(resourceName);
			counterName += L"_CounterReadback";
			THROW_IF_FAIL(m_CounterResource->SetName(counterName.c_str()));
		}
	}

	void SetCounterValue(ID3D12GraphicsCommandList* commandList, UINT value) const
	{
		// Load value into the upload heap
		{
			// Write initial value to the counter
			UINT32* pCounter = nullptr;
			THROW_IF_FAIL(m_CounterUploadResource->Map(0, nullptr, reinterpret_cast<void**>(&pCounter)));
			*pCounter = value;
			m_CounterUploadResource->Unmap(0, nullptr);
		}

		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_CounterResource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);
		commandList->ResourceBarrier(1, &barrier);

		commandList->CopyResource(m_CounterResource.Get(), m_CounterUploadResource.Get());

		barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_CounterResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		commandList->ResourceBarrier(1, &barrier);
	}

	void CopyCounterValue(ID3D12GraphicsCommandList* commandList) const
	{
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_CounterResource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
		commandList->ResourceBarrier(1, &barrier);

		commandList->CopyResource(m_CounterReadbackResource.Get(), m_CounterResource.Get());

		barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_CounterResource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		commandList->ResourceBarrier(1, &barrier);
	}

	UINT ReadCounterValue() const
	{
		// Read the value in the counter readback resource
		// Make sure CopyCounterValue has been called and the command list it was called with has been executed first!

		const CD3DX12_RANGE readRange(0, 1);
		UINT32* pCounter = nullptr;
		THROW_IF_FAIL(m_CounterReadbackResource->Map(0, &readRange, reinterpret_cast<void**>(&pCounter)));
		const UINT value = static_cast<UINT>(*pCounter);
		m_CounterReadbackResource->Unmap(0, nullptr);

		return value;
	}


private:
	ComPtr<ID3D12Resource> m_CounterResource;			// Default heap; provides unordered access to the counter
	ComPtr<ID3D12Resource> m_CounterUploadResource;		// Upload heap; Provides CPU write access to the counter
	ComPtr<ID3D12Resource> m_CounterReadbackResource;	// Readback heap; provides CPU read access to the counter
};
