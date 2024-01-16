#pragma once

#include "Renderer/D3DBuffer.h"

using namespace DirectX;

#include "../Hlsl/RaytracingHlslCompat.h"


class AABBGeometry
{
public:
	AABBGeometry(UINT aabbCount);
	~AABBGeometry() = default;

	DISALLOW_COPY(AABBGeometry)
	DEFAULT_MOVE(AABBGeometry)

	void AddAABB(const XMFLOAT3& centre, const XMFLOAT3& halfExtent, const XMFLOAT3& uvwMin, const XMFLOAT3& uvwMax);

	// Getters
	inline UINT GetAABBCount() const { return m_AABBCount; }
	inline const std::vector<D3D12_RAYTRACING_AABB>& GetAABBs() const { return m_AABBs; }

	D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE GetAABBBuffer() const;
	inline D3D12_GPU_VIRTUAL_ADDRESS GetPrimitiveDataBuffer() const { return m_PrimitiveDataBuffer.GetAddressOfElement(0, 0); }

private:
	std::vector<D3D12_RAYTRACING_AABB> m_AABBs;
	D3DUploadBuffer<D3D12_RAYTRACING_AABB> m_AABBBuffer;
	D3DUploadBuffer<AABBPrimitiveData> m_PrimitiveDataBuffer;

	UINT m_AABBCount = 0;
};


class AABBGeometryInstance
{
public:
	AABBGeometryInstance(const AABBGeometry& geometry, D3D12_RAYTRACING_GEOMETRY_FLAGS flags, D3D12_GPU_DESCRIPTOR_HANDLE volumeSRV, UINT volumeResolution)
		: m_Geometry(&geometry)
		, m_Flags(flags)
		, m_VolumeSRV(volumeSRV)
		, m_VolumeResolution(volumeResolution)
	{}

	// Getters
	inline const AABBGeometry& GetGeometry() const { return *m_Geometry; };

	inline UINT GetAABBCount() const { return m_Geometry->GetAABBCount(); }
	inline D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE GetAABBBuffer() const { return m_Geometry->GetAABBBuffer(); }
	inline D3D12_GPU_VIRTUAL_ADDRESS GetPrimitiveDataBuffer() const { return m_Geometry->GetPrimitiveDataBuffer(); }

	inline D3D12_RAYTRACING_GEOMETRY_FLAGS GetFlags() const { return m_Flags; }

	inline void SetVolume(D3D12_GPU_DESCRIPTOR_HANDLE handle, UINT resolution) { m_VolumeSRV = handle; m_VolumeResolution = resolution; }
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetVolumeSRV() const { return m_VolumeSRV; }
	inline UINT GetVolumeResolution() const { return m_VolumeResolution; }

private:
	const AABBGeometry* m_Geometry = nullptr;

	D3D12_RAYTRACING_GEOMETRY_FLAGS m_Flags;

	// The volume to be rendered within this aabb geometry
	D3D12_GPU_DESCRIPTOR_HANDLE m_VolumeSRV;
	// The resolution of the volume
	UINT m_VolumeResolution = 0;
};
