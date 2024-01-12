#pragma once
#include "Renderer/D3DBuffer.h"


using Microsoft::WRL::ComPtr;


// Acceleration Structure base class for functionality shared between bottom- and top-level structures
class AccelerationStructure
{
public:
	virtual ~AccelerationStructure() = default;

	inline UINT64 GetRequiredScratchSize() const { return max(m_PrebuildInfo.ScratchDataSizeInBytes, m_PrebuildInfo.UpdateScratchDataSizeInBytes); }
	inline UINT64 GetRequiredResultDataSize() const { return m_PrebuildInfo.ResultDataMaxSizeInBytes; }

	inline ID3D12Resource* GetResource() const { return m_AccelerationStructure.GetResource(); }

	inline const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO& GetPrebuildInfo() const { return m_PrebuildInfo; }
	inline const std::wstring& GetName() const { return m_Name; }

	inline void SetDirty(bool dirty) { m_IsDirty = dirty; }
	inline bool GetDirty() const { return m_IsDirty; }

	inline UINT64 GetResourceSize() const { return m_AccelerationStructure.GetResource()->GetDesc().Width; }

protected:
	void AllocateResource();

protected:
	D3DUAVBuffer m_AccelerationStructure;

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
class BottomLevelAccelerationStructureGeometry
{
public:


	inline const std::wstring& GetName() const;

};


class BottomLevelAccelerationStructure : public AccelerationStructure
{
public:
	virtual ~BottomLevelAccelerationStructure() = default;

	void Initialize(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags, BottomLevelAccelerationStructureGeometry& geometry,	bool allowUpdate = false, bool updateOnBuild = false);
	void Build(ID3D12Resource* scratch);

	inline void SetInstanceContributionToHitGroupIndex(UINT index) { m_InstanceContributionToHitGroupIndex = index; }
	inline UINT GetInstanceContributionToHitGroupIndex() const { return m_InstanceContributionToHitGroupIndex; }

private:
	void BuildGeometryDescs(BottomLevelAccelerationStructureGeometry& geometry);
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
	virtual ~TopLevelAccelerationStructure() = default;

	void Initialize(UINT numBottomLevelASInstanceDescs, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags, bool allowUpdate = false, bool updateOnBuild = false, const wchar_t* resourceName = nullptr);
	void Build(UINT numInstanceDescs, D3D12_GPU_VIRTUAL_ADDRESS instanceDescs, ID3D12Resource* scratch);

private:
	void ComputePrebuildInfo(UINT numBottomLevelASInstanceDescs);

};


class RaytracingAccelerationStructureManager
{
	RaytracingAccelerationStructureManager(UINT numBottomLevelInstances, UINT frameCount);

	void AddBottomLevelAS();
	void AddBottomLevelASInstance();

	void InitializeTopLevelAS();

	void Build();

private:
	TopLevelAccelerationStructure m_TopLevelAS;
	std::map<std::wstring, BottomLevelAccelerationStructure> m_BottomLevelAS;

	D3DUploadBuffer<D3D12_RAYTRACING_INSTANCE_DESC> m_BottomLevelInstanceDescs;

	ComPtr<ID3D12Resource> m_ScratchResource;
	UINT m_ScratchResourceSize = 0;
}; 
