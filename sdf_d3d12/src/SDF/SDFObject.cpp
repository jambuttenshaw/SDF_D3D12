#include "pch.h"
#include "SDFObject.h"

#include "Framework/RenderItem.h"
#include "Renderer/D3DGraphicsContext.h"


SDFObject::SDFObject(UINT width, UINT height, UINT depth)
	: m_Width(width)
	, m_Height(height)
	, m_Depth(depth)
{
	// Create the 3D texture resource that will store the sdf data
	const auto device = g_D3DGraphicsContext->GetDevice();

	auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex3D(DXGI_FORMAT_R8_UNORM, m_Width, m_Height, m_Depth, 1, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	THROW_IF_FAIL(device->CreateCommittedResource(
		&heap,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&m_Resource)
	));
	m_Resource->SetName(L"SDF Object Volume Resource");

	// Create UAV and SRV
	m_ResourceViews = g_D3DGraphicsContext->GetSRVHeap()->Allocate(2);

	device->CreateShaderResourceView(m_Resource.Get(), nullptr, m_ResourceViews.GetCPUHandle(0));
	device->CreateUnorderedAccessView(m_Resource.Get(), nullptr, nullptr, m_ResourceViews.GetCPUHandle(1));

	// Create a render item for this object
	m_RenderItem = g_D3DGraphicsContext->CreateRenderItem();

	// Setup bounds extents such that the bounds preserves the aspect ratio of the texture and its longest length is 1
	const auto fWidth = static_cast<float>(width);
	const auto fHeight = static_cast<float>(height);
	const auto fDepth = static_cast<float>(depth);
	const float maxDim = max(max(fWidth, fHeight), fDepth);
	m_RenderItem->SetBoundsExtents({
		fWidth / maxDim,
		fHeight / maxDim,
		fDepth / maxDim
		});
}

SDFObject::~SDFObject()
{
	m_ResourceViews.Free();
}


void SDFObject::AddPrimitive(SDFPrimitive&& primitive)
{
	m_Primitives.emplace_back(primitive);
}
