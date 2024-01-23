#include "pch.h"
#include "SDFObject.h"

#include "Renderer/D3DGraphicsContext.h"
#include "Renderer/Hlsl/ComputeHlslCompat.h"

SDFObject::SDFObject(UINT resolution)
	: m_VolumeResolution(resolution)
{
	ASSERT(m_VolumeResolution % SDF_BRICK_SIZE == 0, "Resolution must be a multiple of the brick size!");
	ASSERT(m_VolumeResolution > 0, "Invalid value for resolution");

	UINT divisions = m_VolumeResolution / SDF_BRICK_SIZE;
	m_MaxAABBCount = divisions * divisions * divisions;

	const auto device = g_D3DGraphicsContext->GetDevice();
	const auto descriptorHeap = g_D3DGraphicsContext->GetSRVHeap();

	// Create volume resource
	{
		auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex3D(DXGI_FORMAT_R8_SNORM, m_VolumeResolution, m_VolumeResolution, m_VolumeResolution, 1, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

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
		m_AABBBuffer.Allocate(device, m_MaxAABBCount, D3D12_RESOURCE_STATE_COMMON, L"AABB Buffer");
		m_PrimitiveDataBuffer.Allocate(device, m_MaxAABBCount, D3D12_RESOURCE_STATE_COMMON, L"AABB Primitive Data Buffer");
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
			const auto uavDesc = m_PrimitiveDataBuffer.CreateUAVDesc();
			device->CreateUnorderedAccessView(m_PrimitiveDataBuffer.GetResource(), nullptr, &uavDesc, m_ResourceViews.GetCPUHandle(3));
		}
	}
}

SDFObject::~SDFObject()
{
	m_ResourceViews.Free();
}
