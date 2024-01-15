#pragma once

#include "Renderer/D3DBuffer.h"

using namespace DirectX;

#include "../Hlsl/RaytracingHlslCompat.h"


class AABBGeometry
{
public:
	AABBGeometry(UINT aabbCount);

	void AddAABB(const XMFLOAT3& centre, const XMFLOAT3& halfExtent);
	void AddAABB(const D3D12_RAYTRACING_AABB&& aabb);

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
	AABBGeometryInstance(const AABBGeometry& geometry, D3D12_RAYTRACING_GEOMETRY_FLAGS flags, D3D12_GPU_DESCRIPTOR_HANDLE volumeSRV = {})
		: m_Geometry(&geometry)
		, m_Flags(flags)
		, m_VolumeSRV(volumeSRV)
	{}

	// Getters
	inline const AABBGeometry& GetGeometry() const { return *m_Geometry; };

	inline UINT GetAABBCount() const { return m_Geometry->GetAABBCount(); }
	inline D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE GetAABBBuffer() const { return m_Geometry->GetAABBBuffer(); }
	inline D3D12_GPU_VIRTUAL_ADDRESS GetPrimitiveDataBuffer() const { return m_Geometry->GetPrimitiveDataBuffer(); }

	inline D3D12_RAYTRACING_GEOMETRY_FLAGS GetFlags() const { return m_Flags; }

	inline void SetVolumeSRV(D3D12_GPU_DESCRIPTOR_HANDLE handle) { m_VolumeSRV = handle; }
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetVolumeSRV() const { return m_VolumeSRV; }

private:
	const AABBGeometry* m_Geometry = nullptr;

	D3D12_RAYTRACING_GEOMETRY_FLAGS m_Flags;

	// The volume to be rendered within this aabb geometry
	D3D12_GPU_DESCRIPTOR_HANDLE m_VolumeSRV;
};
