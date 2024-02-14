#pragma once

#include "Core.h"
#include "Renderer/Memory/MemoryAllocator.h"
#include "Renderer/Hlsl/RaytracingHlslCompat.h"
#include "Renderer/Buffer/StructuredBuffer.h"


using Microsoft::WRL::ComPtr;

struct SDFEditData;


class SDFObject
{
	struct Resources;
public:
	enum ResourceGroup
	{
		RESOURCES_READ = 0,
		RESOURCES_WRITE,
		RESOURCES_COUNT
	};
	enum ResourceState
	{
		READY_COMPUTE,
		COMPUTING,
		COMPUTED,
		SWITCHING,
		RENDERING,
		RENDERED
	};

public:
	SDFObject(float minBrickSize, UINT brickCapacity, D3D12_RAYTRACING_GEOMETRY_FLAGS geometryFlags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE);
	~SDFObject();

	DISALLOW_COPY(SDFObject)
	DEFAULT_MOVE(SDFObject)

	// Brick Pool
	void AllocateOptimalResources(UINT brickCount, float brickSize, ResourceGroup res);
	ID3D12Resource* GetBrickPool(ResourceGroup res) const;

	inline float GetMinBrickSize() const { return m_MinBrickSize; }
	float GetBrickSize(ResourceGroup res) const;

	inline UINT GetBrickBufferCapacity() const { return m_BrickCapacity; }

	UINT GetBrickCount(ResourceGroup res) const;
	const XMUINT3& GetBrickPoolDimensions(ResourceGroup res) const;
	UINT GetBrickPoolCapacity(ResourceGroup res) const;

	// Geometry buffer
	ID3D12Resource* GetAABBBuffer(ResourceGroup res) const;
	D3D12_GPU_VIRTUAL_ADDRESS GetAABBBufferAddress(ResourceGroup res) const;
	UINT GetAABBBufferStride(ResourceGroup res) const;

	ID3D12Resource* GetBrickBuffer(ResourceGroup res) const;
	D3D12_GPU_VIRTUAL_ADDRESS GetBrickBufferAddress(ResourceGroup res) const;
	UINT GetBrickBufferStride(ResourceGroup res) const;

	// Get Resource Views
	D3D12_GPU_DESCRIPTOR_HANDLE GetBrickPoolSRV(ResourceGroup res) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetBrickPoolUAV(ResourceGroup res) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetAABBBufferUAV(ResourceGroup res) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetBrickBufferUAV(ResourceGroup res) const;

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
	UINT64 GetBrickPoolSizeBytes(ResourceGroup res) const;
	UINT64 GetAABBBufferSizeBytes(ResourceGroup res) const;
	UINT64 GetBrickBufferSizeBytes(ResourceGroup res) const;

	UINT64 GetTotalMemoryUsageBytes() const;


	// ASync construction

	// Flips which set are being rendered from, and which are being written to
	void FlipResources()
	{
		m_ReadIndex = 1 - m_ReadIndex;
		m_IsLocalArgsDirty = true;
	}
	inline ResourceState GetResourcesState(ResourceGroup res)
	{
		return m_ResourcesStates.at(res == RESOURCES_READ ? ReadIndex() : WriteIndex());
	}
	inline void SetResourceState(ResourceGroup res, ResourceState state)
	{
		m_ResourcesStates.at(res == RESOURCES_READ ? ReadIndex() : WriteIndex()) = state;
	}
	inline bool CheckResourceState(ResourceGroup res, ResourceState state)
	{
		const bool match = GetResourcesState(res) == state;
		ASSERT(match, "Unexpected resource state!");
		return match;
	}

	inline size_t ReadIndex() const { return m_ReadIndex; }
	inline size_t WriteIndex() const { return 1 - m_ReadIndex; }
private:
	Resources& GetResources(ResourceGroup res)
	{
		return m_Resources.at(res == RESOURCES_READ ? ReadIndex() : WriteIndex());
	}
	const Resources& GetResources(ResourceGroup res) const
	{
		return m_Resources.at(res == RESOURCES_READ ? ReadIndex() : WriteIndex());
	}


	void AllocateOptimalAABBBuffer(UINT brickCount, ResourceGroup res);
	void AllocateOptimalBrickBuffer(UINT brickCount, ResourceGroup res);
	void AllocateOptimalBrickPool(UINT brickCount, ResourceGroup res);


private:
	// A complete set of resources that make up this object
	struct Resources
	{
		ComPtr<ID3D12Resource> BrickPool;
		
		// Geometry
		StructuredBuffer<D3D12_RAYTRACING_AABB> AABBBuffer;
		StructuredBuffer<BrickPointer> BrickBuffer;

		// Edits
		StructuredBuffer<SDFEditData> EditBuffer;
		DefaultBuffer IndexBuffer;

		DescriptorAllocation ResourceViews;	// index 0 = brick pool SRV
											// index 1 = brick pool UAV
											// index 2 = aabb buffer UAV
											// index 3 = brick buffer UAV

		// Brick count/pool related properties are only pertinent to a specific set
		UINT BrickCount = 0; // The number of bricks that actually make up this object
		XMUINT3 BrickPoolDimensions = { 0, 0, 0 }; // The dimensions of the brick pool in number of bricks
		float BrickSize = 0.0f;
	};
	std::array<Resources, 2> m_Resources;
	// Which index is to be read from
	// implies that 1 - m_ReadIndex is the index to write to
	std::atomic<size_t> m_ReadIndex = 0;
	std::array<std::atomic<ResourceState>, 2> m_ResourcesStates = { READY_COMPUTE, READY_COMPUTE };

	float m_MinBrickSize = 0.0f;
	UINT m_BrickCapacity = 0; // The maximum possible number of bricks

	// For raytracing acceleration structure
	D3D12_RAYTRACING_GEOMETRY_FLAGS m_GeometryFlags;

	// For raytracing shader table
	// If local root arguments are dirty then update the shader table record
	UINT m_ShaderRecordOffset = -1;
	bool m_IsLocalArgsDirty = true;
};
