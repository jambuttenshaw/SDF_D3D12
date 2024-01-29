#pragma once

#include "Core.h"
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

	ID3D12Resource* GetBrickPool() const;

	inline UINT GetBrickCount() const { return m_BrickCount; }
	inline float GetBrickSize() const { return m_BrickSize; }

	inline UINT GetBrickBufferCapacity() const { return m_BrickCapacity; }

	inline const XMUINT3& GetBrickPoolDimensions() const { return m_BrickPoolDimensions; }
	inline UINT GetBrickPoolCapacity() const { return m_BrickPoolDimensions.x * m_BrickPoolDimensions.y * m_BrickPoolDimensions.z; }

	// Geometry buffer
	D3D12_GPU_VIRTUAL_ADDRESS GetAABBBufferAddress() const;
	UINT GetAABBBufferStride() const;

	D3D12_GPU_VIRTUAL_ADDRESS GetBrickBufferAddress() const;
	UINT GetBrickBufferStride() const;

	// Get Resource Views
	D3D12_GPU_DESCRIPTOR_HANDLE GetBrickPoolSRV() const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetBrickPoolUAV() const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetAABBBufferUAV() const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetBrickBufferUAV() const;

	// Acceleration structure properties
	inline D3D12_RAYTRACING_GEOMETRY_FLAGS GetGeometryFlags() const { return m_GeometryFlags; }

	// Shader record properties
	inline UINT GetShaderRecordOffset() const { return m_ShaderRecordOffset; }
	inline void SetShaderRecordOffset(UINT shaderRecordOffset)
	{
		if (m_ShaderRecordOffset != -1) { LOG_WARN("Shader record offset has already been set."); }
		m_ShaderRecordOffset = shaderRecordOffset;
	}

	inline bool AreLocalArgumentsDirty() const { return m_AreLocalArgumentsDirty; }
	inline void ClearLocalArgumentsDirty() { }

	// Memory usage
	UINT64 GetBrickPoolSizeBytes() const;
	UINT64 GetAABBBufferSizeBytes() const;
	UINT64 GetBrickBufferSizeBytes() const;

private:
	struct FrameResources
	{
		ComPtr<ID3D12Resource> m_BrickPool;
		
		// Geometry
		StructuredBuffer<D3D12_RAYTRACING_AABB> m_AABBBuffer;
		StructuredBuffer<BrickPointer> m_BrickBuffer;

		DescriptorAllocation m_ResourceViews;	// index 0 = brick pool SRV
												// index 1 = brick pool UAV
												// index 2 = aabb buffer uav
												// index 3 = brick buffer uav
	};
	std::vector<FrameResources> m_Resources;

	float m_BrickSize = 0.0f;
	UINT m_BrickCapacity = 0; // The maximum possible number of bricks

	UINT m_BrickCount = 0; // The number of bricks that actually make up this object
	XMUINT3 m_BrickPoolDimensions = { 0, 0, 0 }; // The dimensions of the brick pool in number of bricks

	// For raytracing acceleration structure
	D3D12_RAYTRACING_GEOMETRY_FLAGS m_GeometryFlags;

	// For raytracing shader table
	// If local root arguments are dirty then update the shader table record
	UINT m_ShaderRecordOffset = -1;
	bool m_AreLocalArgumentsDirty = true;
};
