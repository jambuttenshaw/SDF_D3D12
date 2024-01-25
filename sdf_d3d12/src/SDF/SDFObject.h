#pragma once

#include "Renderer/Memory/MemoryAllocator.h"
#include "Renderer/Hlsl/RaytracingHlslCompat.h"
#include "Renderer/Buffer/StructuredBuffer.h"


using Microsoft::WRL::ComPtr;


class SDFObject
{
public:
	SDFObject(float brickSize, UINT brickCapacity, D3D12_RAYTRACING_GEOMETRY_FLAGS geometryFlags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE);
	~SDFObject();

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

	// Geometry buffer
	inline D3D12_GPU_VIRTUAL_ADDRESS GetAABBBufferAddress() const { return m_AABBBuffer.GetAddress(); }
	inline UINT GetAABBBufferStride() const { return m_AABBBuffer.GetElementStride(); }

	inline D3D12_GPU_VIRTUAL_ADDRESS GetBrickBufferAddress() const { return m_BrickBuffer.GetAddress(); }
	inline UINT GetBrickBufferStride() const { return m_BrickBuffer.GetElementStride(); }

	// Get Resource Views
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetBrickPoolSRV() const { return m_ResourceViews.GetGPUHandle(0); };
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetBrickPoolUAV() const { return m_ResourceViews.GetGPUHandle(1); }
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetAABBBufferUAV() const { return m_ResourceViews.GetGPUHandle(2); }
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetBrickBufferUAV() const { return m_ResourceViews.GetGPUHandle(3); }

	inline D3D12_RAYTRACING_GEOMETRY_FLAGS GetGeometryFlags() const { return m_GeometryFlags; }

	// Memory usage
	UINT64 GetBrickPoolSizeBytes() const;
	UINT64 GetAABBBufferSizeBytes() const;
	UINT64 GetBrickBufferSizeBytes() const;

private:
	ComPtr<ID3D12Resource> m_BrickPool;
	
	// Geometry
	StructuredBuffer<D3D12_RAYTRACING_AABB> m_AABBBuffer;
	StructuredBuffer<BrickPointer> m_BrickBuffer;

	DescriptorAllocation m_ResourceViews;	// index 0 = brick pool SRV
											// index 1 = brick pool UAV
											// index 2 = aabb buffer uav
											// index 3 = brick buffer uav

	float m_BrickSize = 0.0f;
	UINT m_BrickCapacity = 0; // The maximum possible number of bricks

	UINT m_BrickCount = 0; // The number of bricks that actually make up this object
	XMUINT3 m_BrickPoolDimensions = { 0, 0, 0 }; // The dimensions of the brick pool in number of bricks

	// For raytracing acceleration structure
	D3D12_RAYTRACING_GEOMETRY_FLAGS m_GeometryFlags;
};
