#pragma once

#include "Renderer/D3DBuffer.h"
#include "Renderer/D3DPipeline.h"
#include "Renderer/Memory/D3DMemoryAllocator.h"

using Microsoft::WRL::ComPtr;


class SDFObject;

/**
 * An object that will dispatch the CS to render some primitive data into a volume texture
 */
class SDFFactory
{
	struct BakeDataBufferType
	{
		UINT PrimitiveCount;
	};

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

	// Constant data used to hold non-object specific data about how to bake the SDF
	std::unique_ptr<D3DUploadBuffer<BakeDataBufferType>> m_BakeDataBuffer;
	D3DDescriptorAllocation m_BakeDataCBV;

private:
	// number of shader threads per group in each dimension
	inline static constexpr UINT s_NumShaderThreads = 8;
};
