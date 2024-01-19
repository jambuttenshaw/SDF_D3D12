#pragma once

using Microsoft::WRL::ComPtr;

#include "Memory/MemoryAllocator.h"
#include "Buffer/UploadBuffer.h"

#include "Hlsl/RaytracingHlslCompat.h"


class D3DFrameResources
{
public:
	D3DFrameResources();
	~D3DFrameResources();

	// Getters
	inline ID3D12CommandAllocator* GetCommandAllocator() const { return m_CommandAllocator.Get(); }
	inline UINT64 GetFenceValue() const { return m_FenceValue; }

	inline D3D12_GPU_DESCRIPTOR_HANDLE GetPassCBV() const { return m_CBVs.GetGPUHandle(m_PassCBV); }
	inline D3D12_GPU_VIRTUAL_ADDRESS GetPassCBAddress() const { return m_PassCB.GetAddressOfElement(0); }

	// Reset alloc
	inline void ResetAllocator() const { THROW_IF_FAIL(m_CommandAllocator->Reset()); }

	// Fence
	inline void IncrementFence() { m_FenceValue++; }
	inline void SetFence(UINT64 fence) { m_FenceValue = fence; }

	// Upload new constant buffer data
	inline void CopyPassData(const PassConstantBuffer& passData) const { m_PassCB.CopyElement(0, passData); }

	void DeferRelease(const ComPtr<IUnknown>& resource);
	void ProcessDeferrals();

private:
	ComPtr<ID3D12CommandAllocator> m_CommandAllocator;
	UINT64 m_FenceValue = 0;

	// Constant buffers
	UploadBuffer<PassConstantBuffer> m_PassCB;		// All data that is constant per pass

	// Constant buffer views
	DescriptorAllocation m_CBVs;
	UINT m_PassCBV = -1;

	// Resources to be released when the GPU is finished with them
	// A release collection is required per frame resources to ensure that
	// the GPU is not currently using any of the resources
	std::vector<ComPtr<IUnknown>> m_DeferredReleases;
};
