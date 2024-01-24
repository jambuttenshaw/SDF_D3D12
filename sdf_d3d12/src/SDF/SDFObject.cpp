#include "pch.h"
#include "SDFObject.h"

#include "Renderer/D3DGraphicsContext.h"
#include "Renderer/Hlsl/ComputeHlslCompat.h"

SDFObject::SDFObject()
{
	// ASSERT(m_VolumeResolution % SDF_BRICK_SIZE_IN_VOXELS == 0, "Resolution must be a multiple of the brick size!");
	// ASSERT(m_VolumeResolution > 0, "Invalid value for resolution");

	// TODO: Temporary
	// TODO: This is part of the process of removing the dependency on volume resolution for calculations
	m_BrickCapacityPerAxis = { 2, 2, 2 };

	const auto device = g_D3DGraphicsContext->GetDevice();
	const auto descriptorHeap = g_D3DGraphicsContext->GetSRVHeap();

	// Create volume resource
	{
		auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex3D(
			DXGI_FORMAT_R8_SNORM,
			m_BrickCapacityPerAxis.x * SDF_BRICK_SIZE_IN_VOXELS,
			m_BrickCapacityPerAxis.y * SDF_BRICK_SIZE_IN_VOXELS,
			m_BrickCapacityPerAxis.z * SDF_BRICK_SIZE_IN_VOXELS,
			1,
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

		THROW_IF_FAIL(device->CreateCommittedResource(
			&heap,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			nullptr,
			IID_PPV_ARGS(&m_VolumeResource)
		));
		m_VolumeResource->SetName(L"SDF Object Volume Resource");
	}

	// Allocate Geometry buffers
	{
		m_AABBBuffer.Allocate(device, GetBrickCapacity(), D3D12_RESOURCE_STATE_COMMON, L"AABB Buffer");
		m_BrickBuffer.Allocate(device, GetBrickCapacity(), D3D12_RESOURCE_STATE_COMMON, L"AABB Primitive Data Buffer");
	}

	// Create resource views
	{
		m_ResourceViews = descriptorHeap->Allocate(4);

		device->CreateShaderResourceView(m_VolumeResource.Get(), nullptr, m_ResourceViews.GetCPUHandle(0));
		device->CreateUnorderedAccessView(m_VolumeResource.Get(), nullptr, nullptr, m_ResourceViews.GetCPUHandle(1));

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
