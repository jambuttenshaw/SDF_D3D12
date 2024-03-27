#pragma once

#include "Core.h"
#include "GeometryInstance.h"
#include "Renderer/Buffer/UploadBuffer.h"
#include "Renderer/D3DGraphicsContext.h"
#include "Renderer/Buffer/DefaultBuffer.h"


using Microsoft::WRL::ComPtr;
using namespace DirectX;

class SDFObject;


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
	inline bool IsBuilt() const { return m_IsBuilt; }

	inline UINT64 GetResourcesSize() const { return m_AccelerationStructure.GetResource()->GetDesc().Width + m_ScratchResource.GetResource()->GetDesc().Width; }

protected:
	void AllocateResource();
	void AllocateScratchResource();

protected:
	DefaultBuffer m_AccelerationStructure;
	DefaultBuffer m_ScratchResource;

	// Store build flags and prebuild info
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS m_BuildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO m_PrebuildInfo = {};

	std::wstring m_Name;

	bool m_IsBuilt = false; // has this structure ever been built?
	bool m_IsDirty = true; // does this structure require a rebuild

	bool m_UpdateOnBuild = false;
	bool m_AllowUpdate = false;
};


class BottomLevelAccelerationStructure : public AccelerationStructure
{
public:
	BottomLevelAccelerationStructure() = default;
	virtual ~BottomLevelAccelerationStructure() = default;

	DISALLOW_COPY(BottomLevelAccelerationStructure)
	DEFAULT_MOVE(BottomLevelAccelerationStructure)

	void Initialize(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags, const std::wstring& name, SDFObject* geometry, bool allowUpdate = false, bool updateOnBuild = false);
	void Build();

	void UpdateGeometry();
	void UpdateGeometry(SDFObject* geometry);

	inline SDFObject* GetGeometry() const { return m_Geometry; }

	inline void SetInstanceContributionToHitGroupIndex(UINT index) { m_InstanceContributionToHitGroupIndex = index; }
	inline UINT GetInstanceContributionToHitGroupIndex() const { return m_InstanceContributionToHitGroupIndex; }

private:
	void BuildGeometryDescs(const SDFObject* geometry);
	void ComputePrebuildInfo();

private:
	SDFObject* m_Geometry;

	D3D12_RAYTRACING_GEOMETRY_DESC m_GeometryDesc;
	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> m_CachedGeometryDesc;

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

	void AddBottomLevelAS(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags, const std::wstring& name, SDFObject* geometry, bool allowUpdate, bool updateOnBuild);
	UINT AddBottomLevelASInstance(const std::wstring& bottomLevelASName, const XMMATRIX& transform, BYTE instanceMask);

	void InitializeTopLevelAS(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags, bool allowUpdate, bool updateOnBuild, const wchar_t* resourceName);

	void UpdateBottomLevelASGeometry(const SDFObject* geometry);
	void UpdateInstanceDescs(std::vector<SDFGeometryInstance>& instances);

	void Build(bool forceBuild = false);


	// Getters
	inline D3D12_GPU_VIRTUAL_ADDRESS GetTopLevelAccelerationStructureAddress() const { return m_TopLevelAS.GetResource()->GetGPUVirtualAddress(); }

	inline TopLevelAccelerationStructure& GetTopLevelAccelerationStructure() { return m_TopLevelAS; }

	inline size_t GetBottomLevelAccelerationStructureIndex(const std::wstring& name) const { return m_BottomLevelASNames.at(name); }
	BottomLevelAccelerationStructure& GetBottomLevelAccelerationStructure(const SDFObject* geometry);

private:
	TopLevelAccelerationStructure m_TopLevelAS;
	std::map<std::wstring, size_t> m_BottomLevelASNames;
	std::vector<BottomLevelAccelerationStructure> m_BottomLevelAS;

	// Multiple upload buffers are required to buffer this resource
	// As its data will be updated each frame
	std::array<UploadBuffer<D3D12_RAYTRACING_INSTANCE_DESC>, D3DGraphicsContext::GetBackBufferCount()> m_BottomLevelInstanceDescs;

	// A staging buffer where instance descs are collected
	// They are then copied into the upload buffer when the acceleration structure is built
	std::vector<D3D12_RAYTRACING_INSTANCE_DESC> m_BottomLevelInstanceDescsStaging;
	UINT m_NumBottomLevelInstances = 0;
}; 
