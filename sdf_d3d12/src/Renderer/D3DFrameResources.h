#pragma once

using Microsoft::WRL::ComPtr;

#include "Memory/D3DMemoryAllocator.h"
#include "D3DBuffer.h"


struct PassCBType
{
	XMMATRIX ViewMat;
	XMMATRIX InvView;
	XMMATRIX Proj;
	XMMATRIX InvProj;
	XMMATRIX ViewProj;
	XMMATRIX InvViewProj;
	XMFLOAT3 WorldEyePos;
	float padding;
	XMFLOAT2 RTSize;
	XMFLOAT2 InvRTSize;
	float NearZ;
	float FarZ;
	float TotalTime;
	float DeltaTime;
};


struct ObjectCBType
{
	XMMATRIX WorldMat;
};


class D3DFrameResources
{
public:
	D3DFrameResources();
	~D3DFrameResources();

	// Getters
	inline ID3D12CommandAllocator* GetCommandAllocator() const { return m_CommandAllocator.Get(); }
	inline UINT64 GetFenceValue() const { return m_FenceValue; }

	inline D3D12_GPU_DESCRIPTOR_HANDLE GetPassCBV() const { return m_CBVs.GetGPUHandle(m_PassCBV); }
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetObjectCBV(UINT objectIndex) const { return m_CBVs.GetGPUHandle(objectIndex); }

	// Fence
	inline void IncrementFence() { m_FenceValue++; }
	inline void SetFence(UINT64 fence) { m_FenceValue = fence; }

	// Reset alloc
	inline void ResetAllocator() const { THROW_IF_FAIL(m_CommandAllocator->Reset()); }

private:
	ComPtr<ID3D12CommandAllocator> m_CommandAllocator;
	UINT64 m_FenceValue = 0;

	// Constant buffers
	std::unique_ptr<D3DUploadBuffer<PassCBType>> m_PassCB;		// All data that is constant per pass
	std::unique_ptr<D3DUploadBuffer<ObjectCBType>> m_ObjectCB;	// All data that is constant per object

	// Constant buffer views
	D3DDescriptorAllocation m_CBVs;
	UINT m_PassCBV = 0;
};
