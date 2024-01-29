#include "pch.h"
#include "SDFObject.h"

#include "Renderer/D3DGraphicsContext.h"
#include "Renderer/Hlsl/ComputeHlslCompat.h"

SDFObject::SDFObject(float brickSize, UINT brickCapacity, D3D12_RAYTRACING_GEOMETRY_FLAGS geometryFlags)
	: m_Resources(1)
	, m_GeometryFlags(geometryFlags)
{
	ASSERT(brickSize > 0.0f, "Invalid brick size!");
	ASSERT(brickCapacity > 0, "Invalid brick capacity!");

	m_BrickSize = brickSize;
	m_BrickCapacity = brickCapacity;

	const auto device = g_D3DGraphicsContext->GetDevice();
	const auto descriptorHeap = g_D3DGraphicsContext->GetSRVHeap();

	for (auto& resources : m_Resources)
	{
		// Allocate Geometry buffers
		{
			resources.m_AABBBuffer.Allocate(device, GetBrickBufferCapacity(), D3D12_RESOURCE_STATE_COMMON, L"AABB Buffer");
			resources.m_BrickBuffer.Allocate(device, GetBrickBufferCapacity(), D3D12_RESOURCE_STATE_COMMON, L"AABB Primitive Data Buffer");
		}

		// Create resource views
		{
			resources.m_ResourceViews = descriptorHeap->Allocate(4);
			ASSERT(resources.m_ResourceViews.IsValid(), "Descriptor allocation failed!");

			{
				const auto uavDesc = resources.m_AABBBuffer.CreateUAVDesc();
				device->CreateUnorderedAccessView(resources.m_AABBBuffer.GetResource(), nullptr, &uavDesc, resources.m_ResourceViews.GetCPUHandle(2));
			}
			{
				const auto uavDesc = resources.m_BrickBuffer.CreateUAVDesc();
				device->CreateUnorderedAccessView(resources.m_BrickBuffer.GetResource(), nullptr, &uavDesc, resources.m_ResourceViews.GetCPUHandle(3));
			}
		}
	}
}

SDFObject::~SDFObject()
{
	for (auto& resources : m_Resources)
		resources.m_ResourceViews.Free();
}

void SDFObject::AllocateOptimalBrickPool(UINT brickCount)
{
	ASSERT(brickCount > 0, "SDF Object does not have any bricks!");
	const auto current = 0;

	if (m_Resources.at(current).m_BrickPool)
	{
		// Brick pool has already been allocated
		// A larger brick pool might be required
		if (brickCount > GetBrickPoolCapacity())
		{
			// TODO: In this case a new brick pool should be allocated and the previous one released
			LOG_ERROR("Brick pool is too small - reallocation required!");
		}
		else
		{
			// Existing brick pool is large enough, no allocation required
			// Just update brick count
			m_BrickCount = brickCount;
			return;
		}
	}

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
			IID_PPV_ARGS(&m_Resources.at(current).m_BrickPool)
		));
		m_Resources.at(current).m_BrickPool->SetName(L"SDF Brick Pool");
	}

	// Create resource views for brick pool
	{
		device->CreateShaderResourceView(m_Resources.at(current).m_BrickPool.Get(), nullptr, m_Resources.at(current).m_ResourceViews.GetCPUHandle(0));
		device->CreateUnorderedAccessView(m_Resources.at(current).m_BrickPool.Get(), nullptr, nullptr, m_Resources.at(current).m_ResourceViews.GetCPUHandle(1));
	}

	// Local arguments for shading have changed
	// The shader table will need updated
	m_AreLocalArgumentsDirty = true;
}


ID3D12Resource* SDFObject::GetBrickPool() const
{
	const auto current = 0;
	return m_Resources.at(current).m_BrickPool.Get();
}

D3D12_GPU_VIRTUAL_ADDRESS SDFObject::GetAABBBufferAddress() const
{
	const auto current = 0;
	return m_Resources.at(current).m_AABBBuffer.GetAddress();
}
UINT SDFObject::GetAABBBufferStride() const
{
	const auto current = 0;
	return m_Resources.at(current).m_AABBBuffer.GetElementStride();
}

D3D12_GPU_VIRTUAL_ADDRESS SDFObject::GetBrickBufferAddress() const
{
	const auto current = 0;
	return m_Resources.at(current).m_BrickBuffer.GetAddress();
}
UINT SDFObject::GetBrickBufferStride() const
{
	const auto current = 0;
	return m_Resources.at(current).m_BrickBuffer.GetElementStride();
}

D3D12_GPU_DESCRIPTOR_HANDLE SDFObject::GetBrickPoolSRV() const
{
	const auto current = 0;
	return m_Resources.at(current).m_ResourceViews.GetGPUHandle(0);
};
D3D12_GPU_DESCRIPTOR_HANDLE SDFObject::GetBrickPoolUAV() const
{
	const auto current = 0;
	return m_Resources.at(current).m_ResourceViews.GetGPUHandle(1);
}
D3D12_GPU_DESCRIPTOR_HANDLE SDFObject::GetAABBBufferUAV() const
{
	const auto current = 0;
	return m_Resources.at(current).m_ResourceViews.GetGPUHandle(2);
}
D3D12_GPU_DESCRIPTOR_HANDLE SDFObject::GetBrickBufferUAV() const
{
	const auto current = 0;
	return m_Resources.at(current).m_ResourceViews.GetGPUHandle(3);
}


UINT64 SDFObject::GetBrickPoolSizeBytes() const
{
	const auto current = 0;

	if (!m_Resources.at(current).m_BrickPool)
		return 0;

	const auto desc = m_Resources.at(current).m_BrickPool->GetDesc();
	const auto elements = desc.Width * desc.Height * desc.DepthOrArraySize;
	constexpr auto elementSize = sizeof(BYTE); // R8_SNORM one byte per element
	return elements * elementSize;
}

UINT64 SDFObject::GetAABBBufferSizeBytes() const
{
	const auto current = 0;
	return m_Resources.at(current).m_AABBBuffer.GetElementCount() * m_Resources.at(current).m_AABBBuffer.GetElementStride();
}

UINT64 SDFObject::GetBrickBufferSizeBytes() const
{
	const auto current = 0;
	return m_Resources.at(current).m_BrickBuffer.GetElementCount() * m_Resources.at(current).m_BrickBuffer.GetElementStride();
}
