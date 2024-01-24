#pragma once

#include "Renderer/Memory/MemoryAllocator.h"
#include "Renderer/Raytracing/AABBGeometry.h"
#include "Renderer/Hlsl/RaytracingHlslCompat.h"
#include "Renderer/Buffer/StructuredBuffer.h"


using Microsoft::WRL::ComPtr;

/**
 * A 3D volume texture that contains the SDF data of an object
 */
class SDFObject : public BaseAABBGeometry
{
public:
	SDFObject(float brickSize, UINT brickCapacity);
	virtual ~SDFObject();

	DISALLOW_COPY(SDFObject)
	DEFAULT_MOVE(SDFObject)

	// Brick Pool
	void AllocateOptimalBrickPool(UINT brickCount);

	inline ID3D12Resource* GetBrickPool() const { return m_BrickPool.Get(); }

	inline UINT GetBrickCount() const { return m_BrickCount; }
	inline float GetBrickSize() const { return m_BrickSize; }

	inline UINT GetBrickBufferCapacity() const { return m_BrickCapacity; }

	inline const XMUINT3& GetBrickPoolDimensions() const { return m_BrickPoolDimensions; }
	inline UINT GetBrickPoolCapacity() const { return m_BrickPoolDimensions.x * m_BrickPoolDimensions.y * m_BrickPoolDimensions.z; }

	// Geometry Interface
	inline virtual UINT GetAABBCount() const override { return m_BrickCount; }

	inline virtual D3D12_GPU_VIRTUAL_ADDRESS GetAABBBufferAddress() const override { return m_AABBBuffer.GetAddress(); }
	inline virtual D3D12_GPU_VIRTUAL_ADDRESS GetPrimitiveDataBufferAddress() const override { return m_BrickBuffer.GetAddress(); }

	// Get Resource Views
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetBrickPoolSRV() const { return m_ResourceViews.GetGPUHandle(0); };
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetBrickPoolUAV() const { return m_ResourceViews.GetGPUHandle(1); }
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetAABBBufferUAV() const { return m_ResourceViews.GetGPUHandle(2); }
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetBrickBufferUAV() const { return m_ResourceViews.GetGPUHandle(3); }

private:
	// Volume
	ComPtr<ID3D12Resource> m_BrickPool;
	
	// Geometry
	StructuredBuffer<D3D12_RAYTRACING_AABB> m_AABBBuffer;
	StructuredBuffer<BrickPointer> m_BrickBuffer;

	DescriptorAllocation m_ResourceViews;	// index 0 = volume SRV
											// index 1 = volume UAV
											// index 2 = aabb buffer uav
											// index 3 = brick buffer uav

	float m_BrickSize = 0.0f;
	UINT m_BrickCapacity = 0; // The maximum possible number of bricks

	UINT m_BrickCount = 0; // The number of bricks that actually make up this object
	XMUINT3 m_BrickPoolDimensions = { 0, 0, 0 }; // The dimensions of the brick pool in number of bricks
};
