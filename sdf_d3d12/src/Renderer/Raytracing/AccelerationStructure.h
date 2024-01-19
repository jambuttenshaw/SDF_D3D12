#pragma once

#include "Renderer/Buffer/UploadBuffer.h"
#include "AABBGeometry.h"
#include "Renderer/D3DGraphicsContext.h"
#include "Renderer/Buffer/DefaultBuffer.h"


using Microsoft::WRL::ComPtr;
using namespace DirectX;


// Acceleration Structure base class for functionality shared between bottom- and top-level structures
class AccelerationStructure
{
public:
	AccelerationStructure() = default;
	virtual ~AccelerationStructure() = default;

	DISALLOW_COPY(AccelerationStructure)
	DEFAULT_MOVE(AccelerationStructure)
		
	inline UINT64 GetRequiredScratchSize() const { return max(m_PrebuildInfo.ScratchDataSizeInBytes, m_PrebuildInfo.UpdateScratchDataSizeInBytes); }
	inline UINT64 GetRequiredResultDataSize() const { return m_PrebuildInfo.ResultDataMaxSizeInBytes; }

	inline ID3D12Resource* GetResource() const { return m_AccelerationStructure.GetResource(); }

	inline const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO& GetPrebuildInfo() const { return m_PrebuildInfo; }
	inline const std::wstring& GetName() const { return m_Name; }

	inline void SetDirty(bool dirty) { m_IsDirty = dirty; }
	inline bool IsDirty() const { return m_IsDirty; }

	inline UINT64 GetResourceSize() const { return m_AccelerationStructure.GetResource()->GetDesc().Width; }

protected:
	void AllocateResource();
	void AllocateScratchResource();

protected:
	DefaultBuffer m_AccelerationStructure;
	DefaultBuffer m_ScratchResource;

	// Previous resources to be released after next build
	// This is in the case that the structure is rebuilt but requires a larger buffer than before
	ComPtr<ID3D12Resource> m_PreviousAccelerationStructure;

	// Store build flags and prebuild info
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS m_BuildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO m_PrebuildInfo = {};

	std::wstring m_Name;

	bool m_IsBuilt = false; // has this structure ever been built?
	bool m_IsDirty = true; // does this structure require a rebuild

	bool m_UpdateOnBuild = false;
	bool m_AllowUpdate = false;
};


// A description of all the geometry contained within a bottom level acceleration structure
// Currently this only describes AABB geometry
struct BottomLevelAccelerationStructureGeometry
{
	~BottomLevelAccelerationStructureGeometry() = default;
	DISALLOW_COPY(BottomLevelAccelerationStructureGeometry)
	DEFAULT_MOVE(BottomLevelAccelerationStructureGeometry)

	std::wstring Name;

	// An instance of a geometry
	// Supplies instance-specific data such as the volume texture and geometry flags
	std::vector<AABBGeometryInstance> GeometryInstances;
};


class BottomLevelAccelerationStructure : public AccelerationStructure
{
public:
	BottomLevelAccelerationStructure() = default;
	virtual ~BottomLevelAccelerationStructure() = default;

	DISALLOW_COPY(BottomLevelAccelerationStructure)
	DEFAULT_MOVE(BottomLevelAccelerationStructure)

	void Initialize(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags, const BottomLevelAccelerationStructureGeometry& geometry,	bool allowUpdate = false, bool updateOnBuild = false);
	void Build();

	void UpdateGeometry(const BottomLevelAccelerationStructureGeometry& geometry);

	inline void SetInstanceContributionToHitGroupIndex(UINT index) { m_InstanceContributionToHitGroupIndex = index; }
	inline UINT GetInstanceContributionToHitGroupIndex() const { return m_InstanceContributionToHitGroupIndex; }

private:
	void BuildGeometryDescs(const BottomLevelAccelerationStructureGeometry& geometry);
	void ComputePrebuildInfo();

private:
	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> m_GeometryDescs;

	UINT m_CurrentID = 0;
	std::vector<std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>> m_CachedGeometryDescs;

	UINT m_InstanceContributionToHitGroupIndex = 0;
};


class TopLevelAccelerationStructure : public AccelerationStructure
{
public:
	TopLevelAccelerationStructure() = default;
	virtual ~TopLevelAccelerationStructure() = default;

	DISALLOW_COPY(TopLevelAccelerationStructure)
	DEFAULT_MOVE(TopLevelAccelerationStructure)

	void Initialize(UINT numBottomLevelASInstanceDescs, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags, bool allowUpdate = false, bool updateOnBuild = false, const wchar_t* resourceName = nullptr);
	void Build(UINT numInstanceDescs, D3D12_GPU_VIRTUAL_ADDRESS instanceDescs);

private:
	void ComputePrebuildInfo(UINT numBottomLevelASInstanceDescs);

};


class RaytracingAccelerationStructureManager
{
public:
	RaytracingAccelerationStructureManager(UINT numBottomLevelInstances);
	~RaytracingAccelerationStructureManager() = default;

	DISALLOW_COPY(RaytracingAccelerationStructureManager)
	DEFAULT_MOVE(RaytracingAccelerationStructureManager)

	void AddBottomLevelAS(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags, const BottomLevelAccelerationStructureGeometry& geometry, bool allowUpdate, bool updateOnBuild);
	void UpdateBottomLevelASGeometry(const BottomLevelAccelerationStructureGeometry& geometry);

	UINT AddBottomLevelASInstance(const std::wstring& bottomLevelASName, const XMMATRIX& transform, BYTE instanceMask);

	void InitializeTopLevelAS(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags, bool allowUpdate, bool updateOnBuild, const wchar_t* resourceName);

	void Build(bool forceBuild = false);


	// Manipulate acceleration structure instances
	void SetInstanceTransform(UINT instanceIndex, const XMMATRIX& transform);
	void SetBLASInstanceContributionToHitGroup(const std::wstring& blas, UINT instanceContributionToHitGroupIndex);

	// Getters
	inline D3D12_GPU_VIRTUAL_ADDRESS GetAccelerationStructureAddress() const { return m_TopLevelAS.GetResource()->GetGPUVirtualAddress(); }

private:
	TopLevelAccelerationStructure m_TopLevelAS;
	std::map<std::wstring, BottomLevelAccelerationStructure> m_BottomLevelAS;

	// Multiple upload buffers are required to buffer this resource
	// As its data will be updated each frame
	std::array<UploadBuffer<D3D12_RAYTRACING_INSTANCE_DESC>, D3DGraphicsContext::GetBackBufferCount()> m_BottomLevelInstanceDescs;
	// A staging buffer where instance descs are collected
	// They are then copied into the upload buffer when the acceleration structure is built
	std::vector<D3D12_RAYTRACING_INSTANCE_DESC> m_BottomLevelInstanceDescsStaging;
	UINT m_NumBottomLevelInstances = 0;
}; 
