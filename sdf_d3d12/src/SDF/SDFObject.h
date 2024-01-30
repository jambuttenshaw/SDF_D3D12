#pragma once

#include "Core.h"
#include "Renderer/Memory/MemoryAllocator.h"
#include "Renderer/Hlsl/RaytracingHlslCompat.h"
#include "Renderer/Buffer/StructuredBuffer.h"


using Microsoft::WRL::ComPtr;


class SDFObject
{
	struct Resources;
public:
	enum ResourceGroup
	{
		RESOURCES_READ,
		RESOURCES_WRITE
	};

public:
	SDFObject(float brickSize, UINT brickCapacity, D3D12_RAYTRACING_GEOMETRY_FLAGS geometryFlags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE);
	~SDFObject();

	DISALLOW_COPY(SDFObject)
	DEFAULT_MOVE(SDFObject)

	// Brick Pool
	void AllocateOptimalBrickPool(UINT brickCount, ResourceGroup res = RESOURCES_READ);
	ID3D12Resource* GetBrickPool(ResourceGroup res = RESOURCES_READ) const;

	inline float GetBrickSize() const { return m_BrickSize; }

	inline UINT GetBrickBufferCapacity() const { return m_BrickCapacity; }

	UINT GetBrickCount(ResourceGroup res = RESOURCES_READ) const;
	const XMUINT3& GetBrickPoolDimensions(ResourceGroup res = RESOURCES_READ) const;
	UINT GetBrickPoolCapacity(ResourceGroup res = RESOURCES_READ) const;

	// Geometry buffer
	D3D12_GPU_VIRTUAL_ADDRESS GetAABBBufferAddress(ResourceGroup res = RESOURCES_READ) const;
	UINT GetAABBBufferStride(ResourceGroup res = RESOURCES_READ) const;

	D3D12_GPU_VIRTUAL_ADDRESS GetBrickBufferAddress(ResourceGroup res = RESOURCES_READ) const;
	UINT GetBrickBufferStride(ResourceGroup res = RESOURCES_READ) const;

	// Get Resource Views
	D3D12_GPU_DESCRIPTOR_HANDLE GetBrickPoolSRV(ResourceGroup res = RESOURCES_READ) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetBrickPoolUAV(ResourceGroup res = RESOURCES_READ) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetAABBBufferUAV(ResourceGroup res = RESOURCES_READ) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetBrickBufferUAV(ResourceGroup res = RESOURCES_READ) const;

	// Acceleration structure properties
	inline D3D12_RAYTRACING_GEOMETRY_FLAGS GetGeometryFlags() const { return m_GeometryFlags; }

	// Shader record properties
	inline UINT GetShaderRecordOffset() const { return m_ShaderRecordOffset; }
	inline void SetShaderRecordOffset(UINT shaderRecordOffset)
	{
		if (m_ShaderRecordOffset != -1) { LOG_WARN("Shader record offset has already been set."); }
		m_ShaderRecordOffset = shaderRecordOffset;
	}

	inline bool IsLocalArgsDirty() const { return m_IsLocalArgsDirty; }
	inline void ResetLocalArgsDirty() { m_IsLocalArgsDirty = false; }

	// Memory usage
	UINT64 GetBrickPoolSizeBytes(ResourceGroup res = RESOURCES_READ) const;
	UINT64 GetAABBBufferSizeBytes(ResourceGroup res = RESOURCES_READ) const;
	UINT64 GetBrickBufferSizeBytes(ResourceGroup res = RESOURCES_READ) const;


	// Flips which set are being rendered from, and which are being written to
	void FlipResources()
	{
		m_ReadResources = 1 - m_ReadResources;
		m_IsLocalArgsDirty = true;
	}

private:
	Resources& GetResources(ResourceGroup res)
	{
		return m_Resources.at(res == RESOURCES_READ ? m_ReadResources : 1 - m_ReadResources);
	}
	const Resources& GetResources(ResourceGroup res) const
	{
		return m_Resources.at(res == RESOURCES_READ ? m_ReadResources : 1 - m_ReadResources);
	}

private:
	// A complete set of resources that make up this object
	struct Resources
	{
		ComPtr<ID3D12Resource> BrickPool;
		
		// Geometry
		StructuredBuffer<D3D12_RAYTRACING_AABB> AABBBuffer;
		StructuredBuffer<BrickPointer> BrickBuffer;

		DescriptorAllocation ResourceViews;	// index 0 = brick pool SRV
											// index 1 = brick pool UAV
											// index 2 = aabb buffer UAV
											// index 3 = brick buffer UAV

		// Brick count/pool related properties are only pertinent to a specific set
		UINT BrickCount = 0; // The number of bricks that actually make up this object
		XMUINT3 BrickPoolDimensions = { 0, 0, 0 }; // The dimensions of the brick pool in number of bricks
	};
	std::vector<Resources> m_Resources;
	// Which index is to be read from
	// implies that 1 - m_ReadResources is the index to write to
	size_t m_ReadResources = 0;

	float m_BrickSize = 0.0f;
	UINT m_BrickCapacity = 0; // The maximum possible number of bricks

	// For raytracing acceleration structure
	D3D12_RAYTRACING_GEOMETRY_FLAGS m_GeometryFlags;

	// For raytracing shader table
	// If local root arguments are dirty then update the shader table record
	UINT m_ShaderRecordOffset = -1;
	bool m_IsLocalArgsDirty = true;
};
