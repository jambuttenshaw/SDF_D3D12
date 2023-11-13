#include "pch.h"
#include "SDFObject.h"

#include "Renderer/D3DGraphicsContext.h"


SDFFactory::SDFFactory()
{
	const auto device = g_D3DGraphicsContext->GetDevice();

	// Create pipeline that is used to bake SDFs into 3D textures

	// Describe the shader's parameters
	CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
	CD3DX12_ROOT_PARAMETER1 rootParameters[1];

	ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);

	rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL);

	D3DComputePipelineDesc desc;
	desc.NumRootParameters = 1;
	desc.RootParameters = rootParameters;
	desc.Shader = L"assets/shaders/sdf_baker.hlsl";
	desc.EntryPoint = "main";
	desc.Defines = nullptr;

	m_Pipeline = std::make_unique<D3DComputePipeline>(&desc);


	// Create command queue, allocator, and list
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	THROW_IF_FAIL(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_CommandQueue)));

	THROW_IF_FAIL(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_CommandAllocator)));
	THROW_IF_FAIL(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_CommandAllocator.Get(), m_Pipeline->GetPipelineState(), IID_PPV_ARGS(&m_CommandList)));

	THROW_IF_FAIL(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence)));
	m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (!m_FenceEvent)
	{
		LOG_FATAL("Failed to create fence event.");
	}
	m_FenceValue = 1;
}

SDFFactory::~SDFFactory()
{
	CloseHandle(m_FenceEvent);
}


void SDFFactory::BakeSDFSynchronous(const SDFObject* object)
{
	// Scene texture must be in unordered access state
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(object->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	m_CommandList->ResourceBarrier(1, &barrier);

	ID3D12DescriptorHeap* ppDescriptorHeaps[] = { g_D3DGraphicsContext->GetSRVHeap()->GetHeap() };
	m_CommandList->SetDescriptorHeaps(_countof(ppDescriptorHeaps), ppDescriptorHeaps);

	// Set pipeline state
	m_Pipeline->Bind(m_CommandList.Get());

	// Set resource views
	m_CommandList->SetComputeRootDescriptorTable(0, object->GetUAV());

	// Use fast ceiling of integer division
	const UINT threadGroupX = (object->GetWidth() + s_NumShaderThreads - 1) / s_NumShaderThreads;
	const UINT threadGroupY = (object->GetHeight() + s_NumShaderThreads - 1) / s_NumShaderThreads;
	const UINT threadGroupZ = (object->GetDepth() + s_NumShaderThreads - 1) / s_NumShaderThreads;

	// Dispatch
	m_CommandList->Dispatch(threadGroupX, threadGroupY, threadGroupZ);

	// Perform graphics commands

	// Scene texture will now be used to render to the back buffer
	barrier = CD3DX12_RESOURCE_BARRIER::Transition(object->GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	m_CommandList->ResourceBarrier(1, &barrier);

	THROW_IF_FAIL(m_CommandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
	m_CommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	const uint64_t fence = m_FenceValue++;
	m_CommandQueue->Signal(m_Fence.Get(), fence);
	if (m_Fence->GetCompletedValue() < fence)								// block until async compute has completed using a fence
	{
		m_Fence->SetEventOnCompletion(fence, m_FenceEvent);
		WaitForSingleObject(m_FenceEvent, INFINITE);
	}

	THROW_IF_FAIL(m_CommandAllocator->Reset());
	THROW_IF_FAIL(m_CommandList->Reset(m_CommandAllocator.Get(), m_Pipeline->GetPipelineState()));
}




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
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&m_Resource)
	));

	// Create UAV and SRV
	m_ResourceViews = g_D3DGraphicsContext->GetSRVHeap()->Allocate(2);

	device->CreateShaderResourceView(m_Resource.Get(), nullptr, m_ResourceViews.GetCPUHandle(0));
	device->CreateUnorderedAccessView(m_Resource.Get(), nullptr, nullptr, m_ResourceViews.GetCPUHandle(1));
}

SDFObject::~SDFObject()
{
	m_ResourceViews.Free();
}
