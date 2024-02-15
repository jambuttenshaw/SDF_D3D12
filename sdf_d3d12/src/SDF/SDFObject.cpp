#include "pch.h"
#include "SDFObject.h"

#include "Renderer/D3DGraphicsContext.h"
#include "Renderer/Hlsl/ComputeHlslCompat.h"

SDFObject::SDFObject(float minBrickSize, UINT brickCapacity, D3D12_RAYTRACING_GEOMETRY_FLAGS geometryFlags)
	: m_GeometryFlags(geometryFlags)
{
	ASSERT(minBrickSize > 0.0f, "Invalid brick size!");
	ASSERT(brickCapacity > 0, "Invalid brick capacity!");

	m_MinBrickSize = minBrickSize;
	m_BrickCapacity = brickCapacity;

	const auto descriptorHeap = g_D3DGraphicsContext->GetSRVHeap();

	for (auto& resources : m_Resources)
	{
		resources.ResourceViews = descriptorHeap->Allocate(2);
		ASSERT(resources.ResourceViews.IsValid(), "Descriptor allocation failed!");
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

	auto& resources = GetResources(res);

	resources.BrickSize = brickSize;

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


UINT64 SDFObject::GetBrickPoolSizeBytes(ResourceGroup res) const
{
	auto& brickPool = GetResources(res).BrickPool;
	if (!brickPool)
		return 0;

	const auto desc = brickPool->GetDesc();
	const auto elements = desc.Width * desc.Height * desc.DepthOrArraySize;
	constexpr auto elementSize = sizeof(BYTE); // R8_SNORM one byte per element
	return elements * elementSize;
}

UINT64 SDFObject::GetAABBBufferSizeBytes(ResourceGroup res) const
{
	auto& aabbBuffer = GetResources(res).AABBBuffer;
	return aabbBuffer.GetElementCount() * aabbBuffer.GetElementStride();
}

UINT64 SDFObject::GetBrickBufferSizeBytes(ResourceGroup res) const
{
	auto& brickBuffer = GetResources(res).BrickBuffer;
	return brickBuffer.GetElementCount() * brickBuffer.GetElementStride();
}


UINT64 SDFObject::GetTotalMemoryUsageBytes() const
{
	UINT64 totalSize = 0;
	for (UINT i = 0; i < RESOURCES_COUNT; i++)
	{
		const auto res = static_cast<ResourceGroup>(i);
		totalSize += GetBrickPoolSizeBytes(res);
		totalSize += GetAABBBufferSizeBytes(res);
		totalSize += GetBrickBufferSizeBytes(res);
	}

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
			DXGI_FORMAT_R8_SNORM,
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
		device->CreateShaderResourceView(
			resources.BrickPool.Get(),
			nullptr,
			resources.ResourceViews.GetCPUHandle(0));
		device->CreateUnorderedAccessView(
			resources.BrickPool.Get(),
			nullptr,
			nullptr,
			resources.ResourceViews.GetCPUHandle(1));
	}
}

void SDFObject::AllocateOptimalIndexBuffer(UINT64 indexCount, ResourceGroup res)
{
	auto& resources = GetResources(res);

	const UINT64 width = indexCount * sizeof(UINT);
	if (!resources.IndexBuffer.GetResource() || width > resources.IndexBuffer.GetResource()->GetDesc().Width)
	{
		resources.IndexBuffer.Allocate(g_D3DGraphicsContext->GetDevice(), width, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"SDF Object Index Buffer");
	}
}
