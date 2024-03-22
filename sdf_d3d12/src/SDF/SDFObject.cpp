#include "pch.h"
#include "SDFObject.h"

#include "Renderer/D3DGraphicsContext.h"
#include "HlslCompat/ComputeHlslCompat.h"
#include "Renderer/Lighting/Material.h"

SDFObject::SDFObject(float brickSize, UINT brickCapacity, D3D12_RAYTRACING_GEOMETRY_FLAGS geometryFlags)
	: m_GeometryFlags(geometryFlags)
{
	ASSERT(brickSize > 0.0f, "Invalid brick size!");
	ASSERT(brickCapacity > 0, "Invalid brick capacity!");

	m_BrickCapacity = brickCapacity;

	m_NextRebuildBrickSize = brickSize;

	const auto descriptorHeap = g_D3DGraphicsContext->GetSRVHeap();

	for (auto& resources : m_Resources)
	{
		resources.BrickSize = brickSize;

		resources.ResourceViews = descriptorHeap->Allocate(DESCRIPTOR_COUNT);
		ASSERT(resources.ResourceViews.IsValid(), "Descriptor allocation failed!");
	}

	for (auto& mat : m_MaterialTable)
	{
		mat = 0;
	}
}

SDFObject::~SDFObject()
{
	for (auto& resources : m_Resources)
		resources.ResourceViews.Free();
}

void SDFObject::AllocateOptimalResources(UINT brickCount, float brickSize, UINT64 indexCount, ResourceGroup res)
{
	ASSERT(brickCount > 0, "SDF Object does not have any bricks!");

	GetResources(res).BrickSize = brickSize;

	AllocateOptimalAABBBuffer(brickCount, res);
	AllocateOptimalBrickBuffer(brickCount, res);
	AllocateOptimalBrickPool(brickCount, res);
	AllocateOptimalIndexBuffer(indexCount, res);
}


const XMUINT3& SDFObject::GetBrickPoolDimensions(ResourceGroup res) const
{
	return GetResources(res).BrickPoolDimensions;
}
UINT SDFObject::GetBrickPoolCapacity(ResourceGroup res) const
{
	const auto& dims = GetResources(res).BrickPoolDimensions;
	return dims.x * dims.y * dims.z;
}


UINT SDFObject::GetMaterialID(UINT slot) const
{
	ASSERT(slot < s_MaxMaterialsPerObject, "Invalid material slot.");
	return m_MaterialTable.at(slot);
}

void SDFObject::SetMaterial(const Material* material, UINT slot)
{
	ASSERT(slot < s_MaxMaterialsPerObject, "Invalid material slot.");
	m_MaterialTable[slot] = material->GetMaterialID();

	m_IsLocalArgsDirty = true;
}


UINT64 SDFObject::GetBrickPoolSizeBytes() const
{
	UINT64 totalSize = 0;
	for (UINT i = 0; i < RESOURCES_COUNT; i++)
	{
		auto& brickPool = GetResources(static_cast<ResourceGroup>(i)).BrickPool;
		if (!brickPool)
			continue;

		const auto desc = brickPool->GetDesc();
		const auto elements = desc.Width * desc.Height * desc.DepthOrArraySize;
		constexpr auto elementSize = sizeof(BYTE); // R8_SNORM one byte per element
		totalSize += elements * elementSize;
	}
	return totalSize;

}

UINT64 SDFObject::GetAABBBufferSizeBytes() const
{
	UINT64 totalSize = 0;
	for (UINT i = 0; i < RESOURCES_COUNT; i++)
	{
		auto& aabbBuffer = GetResources(static_cast<ResourceGroup>(i)).AABBBuffer;
		totalSize += aabbBuffer.GetElementCount() * aabbBuffer.GetElementStride();
	}
	return totalSize;
}

UINT64 SDFObject::GetBrickBufferSizeBytes() const
{
	UINT64 totalSize = 0;
	for (UINT i = 0; i < RESOURCES_COUNT; i++)
	{
		auto& brickBuffer = GetResources(static_cast<ResourceGroup>(i)).BrickBuffer;
		totalSize += brickBuffer.GetElementCount() * brickBuffer.GetElementStride();
	}
	return totalSize;
}

UINT64 SDFObject::GetIndexBufferSizeBytes() const
{
	UINT64 totalSize = 0;
	for (UINT i = 0; i < RESOURCES_COUNT; i++)
	{
		auto& indexBuffer = GetResources(static_cast<ResourceGroup>(i)).IndexBuffer;
		if (indexBuffer.GetResource())
			totalSize += indexBuffer.GetResource()->GetDesc().Width;
	}
	return totalSize;
}


UINT64 SDFObject::GetTotalMemoryUsageBytes() const
{
	UINT64 totalSize = 0;
	totalSize += GetBrickPoolSizeBytes();
	totalSize += GetAABBBufferSizeBytes();
	totalSize += GetBrickBufferSizeBytes();
	totalSize += GetIndexBufferSizeBytes();

	return totalSize;
}



void SDFObject::AllocateOptimalAABBBuffer(UINT brickCount, ResourceGroup res)
{
	auto& resources = GetResources(res);

	const auto device = g_D3DGraphicsContext->GetDevice();

	if (resources.AABBBuffer.GetElementCount() < brickCount)
	{
		resources.AABBBuffer.Allocate(device, brickCount, D3D12_RESOURCE_STATE_COMMON, L"SDF Object AABB Buffer");
	}
}

void SDFObject::AllocateOptimalBrickBuffer(UINT brickCount, ResourceGroup res)
{
	auto& resources = GetResources(res);

	const auto device = g_D3DGraphicsContext->GetDevice();

	if (resources.BrickBuffer.GetElementCount() < brickCount)
	{
		resources.BrickBuffer.Allocate(device, brickCount, D3D12_RESOURCE_STATE_COMMON, L"SDF Object Brick Buffer");
	}
}

void SDFObject::AllocateOptimalBrickPool(UINT brickCount, ResourceGroup res)
{
	auto& resources = GetResources(res);

	if (resources.BrickPool)
	{
		// Brick pool has already been allocated
		// A larger brick pool might be required
		if (brickCount > GetBrickPoolCapacity(res))
		{
			LOG_INFO("Brick pool is too small - reallocation required!");
		}
		else
		{
			// Existing brick pool is large enough, no allocation required
			// Just update brick count
			resources.BrickCount = brickCount;
			return;
		}
	}

	const auto device = g_D3DGraphicsContext->GetDevice();

	resources.BrickCount = brickCount;

	// Calculate dimensions for the brick pool such that it contains at least m_BrickCount entries
	// but is also a useful shape
	UINT nextCube = static_cast<UINT>(std::floor(std::cbrtf(static_cast<float>(resources.BrickCount)))) + 1;
	resources.BrickPoolDimensions = { nextCube, nextCube, nextCube };

	//UINT nextPow2 = 1U << (static_cast<UINT>(std::floor(log2(resources.BrickCount) / 3.0f)) + 1);
	//resources.BrickPoolDimensions = { nextPow2, nextPow2, nextPow2 };

	// Create brick pool resource
	{
		const auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		const CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex3D(
			DXGI_FORMAT_R8G8B8A8_UINT,
			static_cast<UINT64>(resources.BrickPoolDimensions.x) * SDF_BRICK_SIZE_VOXELS_ADJACENCY,
			resources.BrickPoolDimensions.y * SDF_BRICK_SIZE_VOXELS_ADJACENCY,
			static_cast<UINT16>(resources.BrickPoolDimensions.z * SDF_BRICK_SIZE_VOXELS_ADJACENCY),
			1,
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

		THROW_IF_FAIL(device->CreateCommittedResource(
			&heap,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			nullptr,
			IID_PPV_ARGS(&resources.BrickPool)
		));
		resources.BrickPool->SetName(L"SDF Brick Pool");
	}

	// Create resource views for brick pool
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_SNORM;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture3D.MostDetailedMip = 0;
		srvDesc.Texture3D.MipLevels = 1;
		srvDesc.Texture3D.ResourceMinLODClamp = 0.0f;

		device->CreateShaderResourceView(resources.BrickPool.Get(), &srvDesc, resources.ResourceViews.GetCPUHandle(POOL_SRV_SNORM));

		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UINT;
		device->CreateShaderResourceView(resources.BrickPool.Get(), &srvDesc, resources.ResourceViews.GetCPUHandle(POOL_SRV_UINT));
	}
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R8G8B8A8_SNORM;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
		uavDesc.Texture3D.MipSlice = 0;
		uavDesc.Texture3D.FirstWSlice = 0;
		uavDesc.Texture3D.WSize = -1;		// all depth slices

		device->CreateUnorderedAccessView(resources.BrickPool.Get(), nullptr, &uavDesc, resources.ResourceViews.GetCPUHandle(POOL_UAV_SNORM));

		uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UINT;
		device->CreateUnorderedAccessView(resources.BrickPool.Get(), nullptr, &uavDesc, resources.ResourceViews.GetCPUHandle(POOL_UAV_UINT));
	}
}

void SDFObject::AllocateOptimalIndexBuffer(UINT64 indexCount, ResourceGroup res)
{
	auto& resources = GetResources(res);

	const UINT64 width = indexCount * sizeof(UINT16);
	if (!resources.IndexBuffer.GetResource() || width > resources.IndexBuffer.GetResource()->GetDesc().Width)
	{
		resources.IndexBuffer.Allocate(g_D3DGraphicsContext->GetDevice(), width, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"SDF Object Index Buffer");
	}
}
