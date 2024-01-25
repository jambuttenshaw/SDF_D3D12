#include "pch.h"
#include "SDFObject.h"

#include "Renderer/D3DGraphicsContext.h"
#include "Renderer/Hlsl/ComputeHlslCompat.h"

SDFObject::SDFObject(float brickSize, UINT brickCapacity, D3D12_RAYTRACING_GEOMETRY_FLAGS geometryFlags)
	: m_GeometryFlags(geometryFlags)
{
	ASSERT(brickSize > 0.0f, "Invalid brick size!");
	ASSERT(brickCapacity > 0, "Invalid brick capacity!");

	m_BrickSize = brickSize;
	m_BrickCapacity = brickCapacity;

	const auto device = g_D3DGraphicsContext->GetDevice();
	const auto descriptorHeap = g_D3DGraphicsContext->GetSRVHeap();

	// Allocate Geometry buffers
	{
		m_AABBBuffer.Allocate(device, GetBrickBufferCapacity(), D3D12_RESOURCE_STATE_COMMON, L"AABB Buffer");
		m_BrickBuffer.Allocate(device, GetBrickBufferCapacity(), D3D12_RESOURCE_STATE_COMMON, L"AABB Primitive Data Buffer");
	}

	// Create resource views
	{
		m_ResourceViews = descriptorHeap->Allocate(4);
		ASSERT(m_ResourceViews.IsValid(), "Descriptor allocation failed!");

		{
			const auto uavDesc = m_AABBBuffer.CreateUAVDesc();
			device->CreateUnorderedAccessView(m_AABBBuffer.GetResource(), nullptr, &uavDesc, m_ResourceViews.GetCPUHandle(2));
		}
		{
			const auto uavDesc = m_BrickBuffer.CreateUAVDesc();
			device->CreateUnorderedAccessView(m_BrickBuffer.GetResource(), nullptr, &uavDesc, m_ResourceViews.GetCPUHandle(3));
		}
	}
}

SDFObject::~SDFObject()
{
	m_ResourceViews.Free();
}

void SDFObject::AllocateOptimalBrickPool(UINT brickCount)
{
	ASSERT(brickCount > 0, "SDF Object does not have any bricks!");
	m_BrickCount = brickCount;

	// Calculate dimensions for the brick pool such that it contains at least m_BrickCount entries
	// but is also a useful shape
	UINT nextCube = static_cast<UINT>(std::floor(std::cbrtf(static_cast<float>(m_BrickCount)))) + 1;
	m_BrickPoolDimensions = { nextCube, nextCube, nextCube };

	const auto device = g_D3DGraphicsContext->GetDevice();

	// Create brick pool resource
	{
		const auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		const CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex3D(
			DXGI_FORMAT_R8_SNORM,
			static_cast<UINT64>(m_BrickPoolDimensions.x) * SDF_BRICK_SIZE_VOXELS_ADJACENCY,
			m_BrickPoolDimensions.y * SDF_BRICK_SIZE_VOXELS_ADJACENCY,
			static_cast<UINT16>(m_BrickPoolDimensions.z * SDF_BRICK_SIZE_VOXELS_ADJACENCY),
			1,
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

		THROW_IF_FAIL(device->CreateCommittedResource(
			&heap,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			nullptr,
			IID_PPV_ARGS(&m_BrickPool)
		));
		m_BrickPool->SetName(L"SDF Brick Pool");
	}

	// Create resource views for brick pool
	{
		device->CreateShaderResourceView(m_BrickPool.Get(), nullptr, m_ResourceViews.GetCPUHandle(0));
		device->CreateUnorderedAccessView(m_BrickPool.Get(), nullptr, nullptr, m_ResourceViews.GetCPUHandle(1));
	}
}

UINT64 SDFObject::GetBrickPoolSizeBytes() const
{
	if (!m_BrickPool)
		return 0;

	const auto desc = m_BrickPool->GetDesc();
	const auto elements = desc.Width * desc.Height * desc.DepthOrArraySize;
	constexpr auto elementSize = sizeof(BYTE); // R8_SNORM one byte per element
	return elements * elementSize;
}

UINT64 SDFObject::GetAABBBufferSizeBytes() const
{
	return m_AABBBuffer.GetElementCount() * m_AABBBuffer.GetElementStride();
}

UINT64 SDFObject::GetBrickBufferSizeBytes() const
{
	return m_BrickBuffer.GetElementCount() * m_BrickBuffer.GetElementStride();
}
