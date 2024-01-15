#pragma once

#include "Renderer/D3DBuffer.h"

using namespace DirectX;


class AABBGeometryInstance
{
public:
	AABBGeometryInstance(UINT aabbCount, D3D12_RAYTRACING_GEOMETRY_FLAGS flags);

	void AddAABB(const XMFLOAT3& centre, const XMFLOAT3& halfExtent);
	void AddAABB(const D3D12_RAYTRACING_AABB&& aabb);

	// Getters
	inline UINT GetAABBCount() const { return m_AABBCount; }
	inline const std::vector<D3D12_RAYTRACING_AABB>& GetAABBs() const { return m_AABBs; }

	D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE GetBuffer() const;

	inline D3D12_RAYTRACING_GEOMETRY_FLAGS GetFlags() const { return m_Flags; }


private:
	std::vector<D3D12_RAYTRACING_AABB> m_AABBs;
	D3DUploadBuffer<D3D12_RAYTRACING_AABB> m_Buffer;

	D3D12_RAYTRACING_GEOMETRY_FLAGS m_Flags;

	UINT m_AABBCount = 0;
};

