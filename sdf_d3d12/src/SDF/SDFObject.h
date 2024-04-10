#pragma once

#include "Core.h"
#include "Renderer/Memory/MemoryAllocator.h"
#include "HlslCompat/StructureHlslCompat.h"
#include "Renderer/Buffer/StructuredBuffer.h"


using Microsoft::WRL::ComPtr;

struct SDFEditData;
class Material;


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
		RENDERING
	};
	enum SDFObjectDescriptor
	{
		POOL_SRV = 0,
		POOL_UAV,
		DESCRIPTOR_COUNT
	};

public:
	SDFObject(float brickSize, UINT brickCapacity, D3D12_RAYTRACING_GEOMETRY_FLAGS geometryFlags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE);
	~SDFObject();

	DISALLOW_COPY(SDFObject)
	DEFAULT_MOVE(SDFObject)

	// Brick Pool
	void AllocateOptimalResources(UINT brickCount, float brickSize, UINT64 indexCount, ResourceGroup res);
	inline ID3D12Resource* GetBrickPool(ResourceGroup res) const { return GetResources(res).BrickPool.Get(); }

	inline float GetBrickSize(ResourceGroup res) const { return GetResources(res).BrickSize; }
	inline UINT GetBrickBufferCapacity() const { return m_BrickCapacity; }

	inline float GetNextRebuildBrickSize() const { return m_NextRebuildBrickSize; }
	inline void SetNextRebuildBrickSize(float size) { m_NextRebuildBrickSize = size; }

	inline UINT GetBrickCount(ResourceGroup res) const { return GetResources(res).BrickCount; }
	const XMUINT3& GetBrickPoolDimensions(ResourceGroup res) const;
	XMUINT3 GetBrickPoolResolution(ResourceGroup res) const;
	UINT GetBrickPoolCapacity(ResourceGroup res) const;

	// Geometry buffer
	inline ID3D12Resource* GetAABBBuffer(ResourceGroup res) const { return GetResources(res).AABBBuffer.GetResource(); }
	inline D3D12_GPU_VIRTUAL_ADDRESS GetAABBBufferAddress(ResourceGroup res) const { return GetResources(res).AABBBuffer.GetAddress(); }
	inline UINT GetAABBBufferStride(ResourceGroup res) const { return GetResources(res).AABBBuffer.GetElementStride(); }

	inline ID3D12Resource* GetBrickBuffer(ResourceGroup res) const { return GetResources(res).BrickBuffer.GetResource(); }
	inline D3D12_GPU_VIRTUAL_ADDRESS GetBrickBufferAddress(ResourceGroup res) const { return GetResources(res).BrickBuffer.GetAddress(); }
	inline UINT GetBrickBufferStride(ResourceGroup res) const { return GetResources(res).BrickBuffer.GetElementStride(); }

	inline ID3D12Resource* GetIndexBuffer(ResourceGroup res) const { return GetResources(res).IndexBuffer.GetResource(); }
	inline D3D12_GPU_VIRTUAL_ADDRESS GetIndexBufferAddress(ResourceGroup res) const { return GetResources(res).IndexBuffer.GetAddress(); }

	// Get Resource Views
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetDescriptor(ResourceGroup res, SDFObjectDescriptor descriptor) const { return GetResources(res).ResourceViews.GetGPUHandle(descriptor); }

	// Acceleration structure properties
	inline D3D12_RAYTRACING_GEOMETRY_FLAGS GetGeometryFlags() const { return m_GeometryFlags; }

	// Materials
	UINT GetMaterialID(UINT slot) const;
	void SetMaterial(const Material* material, UINT slot);

	inline static UINT GetMaxMaterialsPerObject() { return s_MaxMaterialsPerObject; }
	const UINT* GetMaterialTablePtr() const { return m_MaterialTable.data(); }
	size_t GetMaterialTableSize() const { return m_MaterialTable.size() * sizeof(m_MaterialTable.at(0)); }

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
	UINT64 GetBrickPoolSizeBytes(bool distOnly = false) const;
	UINT64 GetAABBBufferSizeBytes() const;
	UINT64 GetBrickBufferSizeBytes() const;
	UINT64 GetIndexBufferSizeBytes() const;

	UINT64 GetTotalMemoryUsageBytes(bool distOnly = false) const;


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
	void AllocateOptimalIndexBuffer(UINT64 indexCount, ResourceGroup res);

private:
	// A complete set of resources that make up this object
	struct Resources
	{
		ComPtr<ID3D12Resource> BrickPool;
		
		// Geometry
		StructuredBuffer<D3D12_RAYTRACING_AABB> AABBBuffer;
		StructuredBuffer<Brick> BrickBuffer;

		// Edits
		DefaultBuffer IndexBuffer;

		DescriptorAllocation ResourceViews;	// index 0 = brick pool SRV
											// index 1 = brick pool UAV

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

	float m_NextRebuildBrickSize = 0.0f;
	UINT m_BrickCapacity = 0; // The maximum possible number of bricks

	// Materials
	inline static constexpr UINT s_MaxMaterialsPerObject = 4;
	// Store a table of material IDs
	std::array<UINT, s_MaxMaterialsPerObject> m_MaterialTable;

	// For raytracing acceleration structure
	D3D12_RAYTRACING_GEOMETRY_FLAGS m_GeometryFlags;

	// For raytracing shader table
	// If local root arguments are dirty then update the shader table record
	UINT m_ShaderRecordOffset = -1;
	bool m_IsLocalArgsDirty = true;
};
