#pragma once

#include "Renderer/Memory/MemoryAllocator.h"

#include "Renderer/D3DGraphicsContext.h"

using Microsoft::WRL::ComPtr;


template<typename T>
class AppendBuffer
{
public:
	AppendBuffer() = default;
	~AppendBuffer()
	{
		if (m_UAV.IsValid())
			m_UAV.Free();
	}

	DISALLOW_COPY(AppendBuffer)
	DEFAULT_MOVE(AppendBuffer)

	// Getters
	inline D3D12_GPU_VIRTUAL_ADDRESS GetAddress() const { return m_AppendBuffer->GetGPUVirtualAddress(); }

	void Allocate(UINT elementCount, UINT64 alignment, D3D12_RESOURCE_STATES initialResourceState, const wchar_t* resourceName = nullptr)
	{
		// TODO: Combine this resource with the counter resource
		// The counter implemented in this class will not work (cannot be placed in upload heap)
		NOT_IMPLEMENTED

		ASSERT(elementCount > 0, "Cannot allocate buffer with 0 elements");

		m_ElementCount = elementCount;

		// Align structured buffers to 16 bytes
		const UINT64 bufferSize = elementCount * sizeof(T);

		// Allocate buffer resource
		const auto defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		const auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, alignment);
		THROW_IF_FAIL(g_D3DGraphicsContext->GetDevice()->CreateCommittedResource(
			&defaultHeap,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			initialResourceState,
			nullptr,
			IID_PPV_ARGS(&m_AppendBuffer)));
		if (resourceName)
		{
			THROW_IF_FAIL(m_AppendBuffer->SetName(resourceName));
		}

		// Allocate counter resource
		const auto uploadHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		const auto counterDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT32), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		THROW_IF_FAIL(g_D3DGraphicsContext->GetDevice()->CreateCommittedResource(
			&uploadHeap,
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

		// Allocate resource to read back the value in the counter
		const auto readbackHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
		const auto readbackDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT32));
		THROW_IF_FAIL(g_D3DGraphicsContext->GetDevice()->CreateCommittedResource(
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

		// Allocate descriptor for UAV
		m_UAV = g_D3DGraphicsContext->GetSRVHeap()->Allocate(1);
		ASSERT(m_UAV.IsValid(), "Failed to allocate UAV descriptor");

		// Create UAV
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = elementCount;
		uavDesc.Buffer.StructureByteStride = sizeof(T);
		uavDesc.Buffer.CounterOffsetInBytes = 0;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		g_D3DGraphicsContext->GetDevice()->CreateUnorderedAccessView(
			m_AppendBuffer.Get(),
			m_CounterResource.Get(),
			&uavDesc,
			m_UAV.GetCPUHandle());

		// Write 0 to the counter
		UINT32* pCounter = nullptr;
		THROW_IF_FAIL(m_CounterResource->Map(0, nullptr, reinterpret_cast<void**>(&pCounter)));
		*pCounter = 0;
		m_CounterResource->Unmap(0, nullptr);
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
	ComPtr<ID3D12Resource> m_AppendBuffer;
	ComPtr<ID3D12Resource> m_CounterResource;
	ComPtr<ID3D12Resource> m_CounterReadbackResource;

	UINT m_ElementCount = 0;

	DescriptorAllocation m_UAV;
};
