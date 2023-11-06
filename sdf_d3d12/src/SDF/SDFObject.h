#pragma once

#include "Renderer/D3DPipeline.h"
#include "Renderer/Memory/D3DMemoryAllocator.h"


using Microsoft::WRL::ComPtr;

class SDFObject;


/**
 * An object that will dispatch the CS to render some primitive data into a volume texture
 */
class SDFFactory
{
public:
	SDFFactory();
	~SDFFactory();

	void BakeSDFSynchronous(const SDFObject* object);

private:
	// The SDF factory runs its own command queue, allocator, and list
	ComPtr<ID3D12CommandQueue> m_CommandQueue;
	ComPtr<ID3D12CommandAllocator> m_CommandAllocator;
	ComPtr<ID3D12GraphicsCommandList> m_CommandList;

	// For signalling when SDF baking completed
	ComPtr<ID3D12Fence> m_Fence;
	UINT64 m_FenceValue = 0;
	HANDLE m_FenceEvent = nullptr;

	// Pipeline state used to bake SDFs
	std::unique_ptr<D3DComputePipeline> m_Pipeline;

private:
	// number of shader threads per group in each dimension
	inline static constexpr UINT s_NumShaderThreads = 8;
};


/**
 * A 3D volume texture that contains the SDF data of an object
 * TODO: Think of a better name for this!
 */
class SDFObject
{
public:
	SDFObject(UINT width, UINT height, UINT depth);
	~SDFObject();

	inline ID3D12Resource* GetResource() const { return m_Resource.Get(); }

	inline UINT GetWidth() const { return m_Width; }
	inline UINT GetHeight() const { return m_Height; }
	inline UINT GetDepth() const { return m_Depth; }

	inline D3D12_GPU_DESCRIPTOR_HANDLE GetSRV() const { return m_ResourceViews.GetGPUHandle(0); };
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetUAV() const{ return m_ResourceViews.GetGPUHandle(1); }

private:
	UINT m_Width = 0,
		 m_Height = 0,
		 m_Depth = 0;

	ComPtr<ID3D12Resource> m_Resource;
	D3DDescriptorAllocation m_ResourceViews;	// index 0 = SRV
												// index 1 = UAV
};
