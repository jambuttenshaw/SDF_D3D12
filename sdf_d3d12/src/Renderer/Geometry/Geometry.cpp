#include "pch.h"
#include "Geometry.h"

#include "Renderer/D3DGraphicsContext.h"


AABBGeometryInstance::AABBGeometryInstance(UINT aabbCount, D3D12_RAYTRACING_GEOMETRY_FLAGS flags)
	: m_Flags(flags)
{
	m_AABBs.resize(aabbCount);
	m_Buffer.Allocate(g_D3DGraphicsContext->GetDevice(), aabbCount, 1, D3D12_RAYTRACING_AABB_BYTE_ALIGNMENT, L"AABB Buffer");
}

void AABBGeometryInstance::AddAABB(const XMFLOAT3& centre, const XMFLOAT3& halfExtent)
{
	AddAABB({
			centre.x - halfExtent.x, centre.y - halfExtent.y, centre.z - halfExtent.z,
			centre.x + halfExtent.x, centre.y + halfExtent.y, centre.z + halfExtent.z,
	});
}

void AABBGeometryInstance::AddAABB(const D3D12_RAYTRACING_AABB&& aabb)
{
	ASSERT(m_AABBCount < m_Buffer.GetElementCount(), "No space in aabb buffer.");

	const UINT index = m_AABBCount++;
	m_AABBs[index] = aabb;

	m_Buffer.CopyElement(index, 0, aabb);
}


D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE AABBGeometryInstance::GetBuffer() const
{
	return { m_Buffer.GetAddressOfElement(0, 0), m_Buffer.GetElementStride() };
}
