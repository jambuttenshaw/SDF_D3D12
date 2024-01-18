#pragma once

#include "Renderer/D3DPipeline.h"

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

	void BakeSDFSynchronous(SDFObject* object);

private:
	void InitializePipelines();

	void Flush();

private:
	// API objects

	// The SDF factory runs its own command queue, allocator, and list
	ComPtr<ID3D12CommandQueue> m_CommandQueue;
	ComPtr<ID3D12CommandAllocator> m_CommandAllocator;
	ComPtr<ID3D12GraphicsCommandList> m_CommandList;

	// For signalling when SDF baking completed
	ComPtr<ID3D12Fence> m_Fence;
	UINT64 m_FenceValue = 0;
	HANDLE m_FenceEvent = nullptr;


	// Pipelines to build SDF objects
	std::unique_ptr<D3DComputePipeline> m_AABBBuildPipeline;
	std::unique_ptr<D3DComputePipeline> m_BakePipeline;

private:
	// number of shader threads per group in each dimension
	inline static constexpr UINT s_NumShaderThreads = 8;
};
