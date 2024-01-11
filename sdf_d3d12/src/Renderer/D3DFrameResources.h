#pragma once

using Microsoft::WRL::ComPtr;

#include "Memory/D3DMemoryAllocator.h"
#include "D3DBuffer.h"

#include "D3DCBTypes.h"


class D3DFrameResources
{
public:
	D3DFrameResources();
	~D3DFrameResources();

	// Getters
	inline ID3D12CommandAllocator* GetCommandAllocator() const { return m_CommandAllocator.Get(); }
	inline UINT64 GetFenceValue() const { return m_FenceValue; }

	inline D3D12_GPU_DESCRIPTOR_HANDLE GetPassCBV() const { return m_CBVs.GetGPUHandle(m_PassCBV); }
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetAllObjectsCBV() const { return m_CBVs.GetGPUHandle(m_AllObjectsCBV); }
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetObjectCBV(UINT objectIndex) const { return m_CBVs.GetGPUHandle(objectIndex); }

	inline D3D12_GPU_VIRTUAL_ADDRESS GetPassCBAddress() const { return m_PassCB->GetAddressOfElement(0); }

	// Reset alloc
	inline void ResetAllocator() const { THROW_IF_FAIL(m_CommandAllocator->Reset()); }

	// Fence
	inline void IncrementFence() { m_FenceValue++; }
	inline void SetFence(UINT64 fence) { m_FenceValue = fence; }

	// Upload new constant buffer data
	inline void CopyPassData(const PassCBType& passData) const { m_PassCB->CopyElement(0, passData); }
	inline void CopyObjectData(UINT objectIndex, const ObjectCBType& objectData) const { m_ObjectCB->CopyElement(objectIndex, objectData); }

	void DeferRelease(const ComPtr<IUnknown>& resource);
	void ProcessDeferrals();

private:
	ComPtr<ID3D12CommandAllocator> m_CommandAllocator;
	UINT64 m_FenceValue = 0;

	// Constant buffers
	std::unique_ptr<D3DUploadBuffer<PassCBType>> m_PassCB;		// All data that is constant per pass
	std::unique_ptr<D3DUploadBuffer<ObjectCBType>> m_ObjectCB;	// All data that is constant per object

	// Constant buffer views
	D3DDescriptorAllocation m_CBVs;
	UINT m_AllObjectsCBV = -1;
	UINT m_PassCBV = -1;

	// Resources to be released when the GPU is finished with them
	// A release collection is required per frame resources to ensure that
	// the GPU is not currently using any of the resources
	std::vector<ComPtr<IUnknown>> m_DeferredReleases;
};
