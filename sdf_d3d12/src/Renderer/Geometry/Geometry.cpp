#include "pch.h"
#include "Geometry.h"

#include "Renderer/D3DGraphicsContext.h"


AABBGeometry::AABBGeometry(UINT aabbCount)
{
	m_AABBs.resize(aabbCount);
	m_AABBBuffer.Allocate(g_D3DGraphicsContext->GetDevice(), aabbCount, 1, D3D12_RAYTRACING_AABB_BYTE_ALIGNMENT, L"AABB Buffer");
	m_PrimitiveDataBuffer.Allocate(g_D3DGraphicsContext->GetDevice(), aabbCount, 1, 16, L"AABB Primitive Data Buffer");
}

void AABBGeometry::AddAABB(const XMFLOAT3& centre, const XMFLOAT3& halfExtent)
{
	AddAABB({
			centre.x - halfExtent.x, centre.y - halfExtent.y, centre.z - halfExtent.z,
			centre.x + halfExtent.x, centre.y + halfExtent.y, centre.z + halfExtent.z,
	});
}

void AABBGeometry::AddAABB(const D3D12_RAYTRACING_AABB&& aabb)
{
	ASSERT(m_AABBCount < m_AABBBuffer.GetElementCount(), "No space in aabb buffer.");

	const UINT index = m_AABBCount++;
	m_AABBs[index] = aabb;

	m_AABBBuffer.CopyElement(index, 0, aabb);

	// Build primitive data
	AABBPrimitiveData primitiveData;

	const XMVECTOR translation =
		0.5f * (XMLoadFloat3((XMFLOAT3*)(&aabb.MinX))
			+ XMLoadFloat3((XMFLOAT3*)(&aabb.MaxX)));
	const XMMATRIX transform = XMMatrixTranslationFromVector(translation);

	primitiveData.AABBMin = { aabb.MinX, aabb.MinY, aabb.MinZ, 1.0f };
	primitiveData.AABBMax = { aabb.MaxX, aabb.MaxY, aabb.MaxZ, 1.0f };
	primitiveData.LocalSpaceToBottomLevelAS = XMMatrixTranspose(transform);
	primitiveData.BottomLevelASToLocalSpace = XMMatrixTranspose(XMMatrixInverse(nullptr, transform));

	// Copy into the buffer
	m_PrimitiveDataBuffer.CopyElement(index, 0, primitiveData);
}


D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE AABBGeometry::GetAABBBuffer() const
{
	return { m_AABBBuffer.GetAddressOfElement(0, 0), m_AABBBuffer.GetElementStride() };
}
