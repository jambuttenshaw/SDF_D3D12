#include "pch.h"
#include "D3DGraphicsContext.h"

#include "Windows/Win32Application.h"

#include "Memory/D3DMemoryAllocator.h"
#include "D3DFrameResources.h"
#include "D3DShaderCompiler.h"
#include "D3DBuffer.h"

#include "Framework/RenderItem.h"
#include "Framework/GameTimer.h"
#include "Framework/Camera.h"


D3DGraphicsContext* g_D3DGraphicsContext = nullptr;


D3DGraphicsContext::D3DGraphicsContext(HWND window, UINT width, UINT height)
	: m_WindowHandle(window)
	, m_ClientWidth(width)
	, m_ClientHeight(height)
	, m_BackBufferFormat(DXGI_FORMAT_R8G8B8A8_UNORM)
	, m_DepthStencilFormat(DXGI_FORMAT_D24_UNORM_S8_UINT)
{
	ASSERT(!g_D3DGraphicsContext, "Cannot initialize a second graphics context!");
	g_D3DGraphicsContext = this;

	LOG_INFO("Creating D3D12 Graphics Context")

	// Initialize D3D components
	CreateAdapter();
	ASSERT(CheckRaytracingSupport(), "Adapter does not support raytracing.");

	// Create device resources
	CreateDevice();
	CreateCommandQueue();
	CreateSwapChain();
	CreateDescriptorHeaps();
	CreateCommandAllocator();
	CreateCommandList();

	CreateRTVs();
	CreateDepthStencilBuffer();

	CreateFrameResources();

	// Create DXR resources
	CreateRaytracingInterface();
	CreateRaytracingRootSignatures();
	CreateRaytracingPipelineStateObject();

	CreateRaytracingOutputTexture();


	CreateViewport();
	CreateScissorRect();

	CreateFence();

	m_ImGuiResources = m_SRVHeap->Allocate(1);
	ASSERT(m_ImGuiResources.IsValid(), "Failed to alloc");

	CreateProjectionMatrix();

	CreateAssets();

	// Close the command list and execute it to begin the initial GPU setup
	// The main loop expects the command list to be closed anyway
	THROW_IF_FAIL(m_CommandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
	m_CommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	m_RenderItems.reserve(s_MaxObjectCount);

	// Wait for any GPU work executed on startup to finish before continuing
	WaitForGPU();
	// Temporary resources that were created during initialization
	// can now safely be released that the GPU has finished its work
	ProcessAllDeferrals();

	LOG_INFO("D3D12 Graphics Context created succesfully.")
}

D3DGraphicsContext::~D3DGraphicsContext()
{
	// Ensure the GPU is no longer references resources that are about
	// to be cleaned up by the destructor
	WaitForGPU();

	// Free allocations
	m_RTVs.Free();
	m_DSV.Free();
	m_ImGuiResources.Free();

	m_RayTracingOutputTextureViews.Free();

	// Free frame resources
	for (UINT n = 0; n < s_FrameCount; n++)
		m_FrameResources[n].reset();

	// Free resources that themselves might free more resources
	ProcessAllDeferrals();

	CloseHandle(m_FenceEvent);
}


void D3DGraphicsContext::Present()
{
	// TODO: This could fail for a reason, it shouldn't always throw
	const HRESULT result = m_SwapChain->Present(1, 0);
	if (result != S_OK)
	{
		switch(result)
		{
		case DXGI_ERROR_DEVICE_RESET:
			LOG_FATAL("Present failed: Device reset!");
		case DXGI_ERROR_DEVICE_REMOVED:
			LOG_FATAL("Present failed: device removed!");
		default:
			LOG_FATAL("Present failed: unknown error!");
		}
	}

	MoveToNextFrame();
	ProcessDeferrals(m_FrameIndex);
}

void D3DGraphicsContext::StartDraw() const
{
	// Command list allocators can only be reset when the associated
	// command lists have finished execution on the GPU
	m_CurrentFrameResources->ResetAllocator();

	// Command lists can (and must) be reset after ExecuteCommandList() is called and before it is repopulated
	THROW_IF_FAIL(m_CommandList->Reset(m_CurrentFrameResources->GetCommandAllocator(), nullptr));

	// Indicate the back buffer will be used as a render target
	const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_RenderTargets[m_FrameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_CommandList->ResourceBarrier(1, &barrier);

	const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_RTVs.GetCPUHandle(m_FrameIndex);
	const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_DSV.GetCPUHandle();
	m_CommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	// Clear render target
	constexpr float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	m_CommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	m_CommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	m_CommandList->RSSetViewports(1, &m_Viewport);
	m_CommandList->RSSetScissorRects(1, &m_ScissorRect);

	// Setup descriptor heaps
	ID3D12DescriptorHeap* ppHeaps[] = { m_SRVHeap->GetHeap(), m_SamplerHeap->GetHeap() };
	m_CommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
}

void D3DGraphicsContext::EndDraw() const
{

	// Indicate that the back buffer will now be used to present
	const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_RenderTargets[m_FrameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	m_CommandList->ResourceBarrier(1, &barrier);

	THROW_IF_FAIL(m_CommandList->Close());

	// Execute the command list
	ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
	m_CommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
}

void D3DGraphicsContext::DrawItems(D3DComputePipeline* pipeline) const
{
	ASSERT(false, "Deprecated");
	/*
	// Perform compute commands
	ASSERT(pipeline, "Pipeline must be valid!");

	// Scene texture must be in unordered access state
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_SceneTexture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	m_CommandList->ResourceBarrier(1, &barrier);

	// Set pipeline state
	pipeline->Bind(m_CommandList.Get());
	
	// Set resource views
	m_CommandList->SetComputeRootDescriptorTable(0, m_CurrentFrameResources->GetAllObjectsCBV());
	m_CommandList->SetComputeRootDescriptorTable(1, m_CurrentFrameResources->GetPassCBV());
	m_CommandList->SetComputeRootDescriptorTable(2, m_SceneTextureViews.GetGPUHandle(0));

	// Dispatch
	m_CommandList->Dispatch(m_ThreadGroupX, m_ThreadGroupY, 1);

	// Perform graphics commands

	// Scene texture will now be used to render to the back buffer
	barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_SceneTexture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	m_CommandList->ResourceBarrier(1, &barrier);

	// Set pipeline state
	m_GraphicsPipeline->Bind(m_CommandList.Get());

	// Set resource views
	m_CommandList->SetGraphicsRootDescriptorTable(0, m_CurrentFrameResources->GetPassCBV());
	m_CommandList->SetGraphicsRootDescriptorTable(1, m_SceneTextureViews.GetGPUHandle(1));

	// Render a full-screen quad
	m_CommandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	m_CommandList->DrawInstanced(4, 1, 0, 0);
	*/
}

void D3DGraphicsContext::DrawVolume(D3DComputePipeline* pipeline, D3D12_GPU_DESCRIPTOR_HANDLE volumeSRV) const
{
	ASSERT(false, "Deprecated");
	/*
	// Perform compute commands
	ASSERT(pipeline, "Pipeline must be valid!");

	// Scene texture must be in unordered access state
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_SceneTexture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	m_CommandList->ResourceBarrier(1, &barrier);s

	// Set pipeline state
	pipeline->Bind(m_CommandList.Get());

	// Set resource views
	m_CommandList->SetComputeRootDescriptorTable(0, m_CurrentFrameResources->GetAllObjectsCBV());
	m_CommandList->SetComputeRootDescriptorTable(1, m_CurrentFrameResources->GetPassCBV());
	m_CommandList->SetComputeRootDescriptorTable(2, m_SceneTextureViews.GetGPUHandle(0));
	m_CommandList->SetComputeRootDescriptorTable(3, volumeSRV);

	// Dispatch
	m_CommandList->Dispatch(m_ThreadGroupX, m_ThreadGroupY, 1);

	// Perform graphics commands

	// Scene texture will now be used to render to the back buffer
	barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_SceneTexture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	m_CommandList->ResourceBarrier(1, &barrier);

	// Set pipeline state
	m_GraphicsPipeline->Bind(m_CommandList.Get());

	// Set resource views
	m_CommandList->SetGraphicsRootDescriptorTable(0, m_CurrentFrameResources->GetPassCBV());
	m_CommandList->SetGraphicsRootDescriptorTable(1, m_SceneTextureViews.GetGPUHandle(1));

	// Render a full-screen quad
	m_CommandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	m_CommandList->DrawInstanced(4, 1, 0, 0);
	*/
}

void D3DGraphicsContext::DrawRaytracing() const
{
	// Scene texture must be in unordered access state
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_RayTracingOutputTexture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	m_CommandList->ResourceBarrier(1, &barrier);

	m_CommandList->SetComputeRootSignature(m_RayTracingGlobalRootSignature.Get());

	// Populate root arguments
	m_CommandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams::OutputViewSlot, m_RayTracingOutputTextureViews.GetGPUHandle(0));
	m_CommandList->SetComputeRootShaderResourceView(GlobalRootSignatureParams::AccelerationStructureSlot, m_TopLevelAccelerationStructure->GetGPUVirtualAddress());

	// Set state
	m_DXRCommandList->SetPipelineState1(m_DXRStateObject.Get());

	// Dispatch rays
	D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
	dispatchDesc.HitGroupTable.StartAddress = m_HitGroupShaderTable->GetAddress();
	dispatchDesc.HitGroupTable.SizeInBytes = m_HitGroupShaderTable->GetSize();
	dispatchDesc.HitGroupTable.StrideInBytes = m_HitGroupShaderTable->GetStride();

	dispatchDesc.MissShaderTable.StartAddress = m_MissShaderTable->GetAddress();
	dispatchDesc.MissShaderTable.SizeInBytes = m_MissShaderTable->GetSize();
	dispatchDesc.MissShaderTable.StrideInBytes = m_MissShaderTable->GetStride();

	dispatchDesc.RayGenerationShaderRecord.StartAddress = m_RayGenShaderTable->GetAddress();
	dispatchDesc.RayGenerationShaderRecord.SizeInBytes = m_RayGenShaderTable->GetSize();

	dispatchDesc.Width = m_ClientWidth;
	dispatchDesc.Height = m_ClientHeight;
	dispatchDesc.Depth = 1;

	m_DXRCommandList->DispatchRays(&dispatchDesc);


	// Perform graphics commands

	// Scene texture will now be used to render to the back buffer
	barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_RayTracingOutputTexture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	m_CommandList->ResourceBarrier(1, &barrier);

	// Set pipeline state
	m_GraphicsPipeline->Bind(m_CommandList.Get());

	// Set resource views
	m_CommandList->SetGraphicsRootDescriptorTable(0, m_CurrentFrameResources->GetPassCBV());
	m_CommandList->SetGraphicsRootDescriptorTable(1, m_RayTracingOutputTextureViews.GetGPUHandle(1));

	// Render a full-screen quad
	m_CommandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	m_CommandList->DrawInstanced(4, 1, 0, 0);
}


RenderItem* D3DGraphicsContext::CreateRenderItem()
{
	return &m_RenderItems.emplace_back();
}


void D3DGraphicsContext::UpdateObjectCBs() const
{
	for (auto renderItem : m_RenderItems)
	{
		if (renderItem.IsDirty())
		{
			// Copy per-object data into the frame CB
			ObjectCBType objectCB;
			// Inverse world matrix is required to position SDF primitives
			objectCB.InvWorldMat = XMMatrixTranspose(XMMatrixInverse(nullptr, renderItem.GetTransform().GetWorldMatrix()));
			objectCB.Scale = renderItem.GetTransform().GetScale();
			objectCB.BoundingBoxExtents = renderItem.GetBoundsExtents();

			m_CurrentFrameResources->CopyObjectData(renderItem.GetObjectIndex(), objectCB);

			renderItem.DecrementDirty();
		}
	}
}

void D3DGraphicsContext::UpdatePassCB(GameTimer* timer, Camera* camera, const RayMarchPropertiesType& rayMarchProperties)
{
	ASSERT(timer, "Must use a valid timer!");
	ASSERT(camera, "Must use a valid camera!");
	// Calculate view matrix
	const XMMATRIX view = camera->GetViewMatrix();

	const XMMATRIX viewProj = XMMatrixMultiply(view, m_ProjectionMatrix);
	const XMMATRIX invView = XMMatrixInverse(nullptr, view);
	const XMMATRIX invProj = XMMatrixInverse(nullptr, m_ProjectionMatrix);
	const XMMATRIX invViewProj = XMMatrixInverse(nullptr, viewProj);

	// Update data in main pass constant buffer
	m_MainPassCB.View = XMMatrixTranspose(view);
	m_MainPassCB.InvView = XMMatrixTranspose(invView);
	m_MainPassCB.Proj = XMMatrixTranspose(m_ProjectionMatrix);
	m_MainPassCB.InvProj = XMMatrixTranspose(invProj);
	m_MainPassCB.ViewProj = XMMatrixTranspose(viewProj);
	m_MainPassCB.InvViewProj = XMMatrixTranspose(invViewProj);

	m_MainPassCB.WorldEyePos = camera->GetPosition();

	m_MainPassCB.ObjectCount = static_cast<UINT>(m_RenderItems.size());

	m_MainPassCB.RTSize = { static_cast<float>(m_ClientWidth), static_cast<float>(m_ClientHeight) };
	m_MainPassCB.InvRTSize = { 1.0f / m_MainPassCB.RTSize.x, 1.0f / m_MainPassCB.RTSize.y };

	m_MainPassCB.NearZ = m_NearPlane;
	m_MainPassCB.FarZ = m_FarPlane;

	m_MainPassCB.TotalTime = timer->GetTimeSinceReset();
	m_MainPassCB.DeltaTime = timer->GetDeltaTime();

	m_MainPassCB.RayMarchProperties = rayMarchProperties;

	m_CurrentFrameResources->CopyPassData(m_MainPassCB);
}


void D3DGraphicsContext::Resize(UINT width, UINT height)
{
	ASSERT(width && height, "Invalid client size!");

	m_ClientWidth = width;
	m_ClientHeight = height;

	// Wait for any work currently being performed by the GPU to finish
	WaitForGPU();

	THROW_IF_FAIL(m_CommandList->Reset(m_DirectCommandAllocator.Get(), nullptr));

	// Release the previous resources that will be recreated
	for (UINT n = 0; n < s_FrameCount; ++n)
		m_RenderTargets[n].Reset();
	m_DepthStencilBuffer.Reset();

	m_RTVs.Free();
	m_DSV.Free();

	// Application-specific, window-specific resources
	m_RayTracingOutputTexture.Reset();
	m_RayTracingOutputTextureViews.Free();

	// Process all deferred frees
	ProcessAllDeferrals();

	THROW_IF_FAIL(m_SwapChain->ResizeBuffers(s_FrameCount, m_ClientWidth, m_ClientHeight, m_BackBufferFormat, 0));

	m_FrameIndex = 0;

	CreateRTVs();
	CreateDepthStencilBuffer();

	// Re-create application-specific, window specific resources
	CreateRaytracingOutputTexture();

	// Send required work to re-init buffers
	THROW_IF_FAIL(m_CommandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
	m_CommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Recreate other objects while GPU is doing work
	CreateViewport();
	CreateScissorRect();
	CreateProjectionMatrix();

	// Wait for GPU to finish its work before continuing
	WaitForGPU();
}


void D3DGraphicsContext::Flush() const
{
	THROW_IF_FAIL(m_CommandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
	m_CommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	WaitForGPU();
}

void D3DGraphicsContext::WaitForGPU() const
{
	// Signal and increment the fence
	THROW_IF_FAIL(m_CommandQueue->Signal(m_Fence.Get(), m_CurrentFrameResources->GetFenceValue()));

	THROW_IF_FAIL(m_Fence->SetEventOnCompletion(m_CurrentFrameResources->GetFenceValue(), m_FenceEvent));
	WaitForSingleObject(m_FenceEvent, INFINITE);

	m_CurrentFrameResources->IncrementFence();
}


void D3DGraphicsContext::CreateAdapter()
{
	UINT dxgiFactoryFlags = 0;

#ifdef _DEBUG
	// Enable debug layer
	ComPtr<ID3D12Debug> debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		debugController->EnableDebugLayer();

		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;

		LOG_INFO("D3D12 Debug Layer Created")
	}
#endif

	THROW_IF_FAIL(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_Factory)));

	for (UINT adapterIndex = 0;
		SUCCEEDED(m_Factory->EnumAdapterByGpuPreference(
			adapterIndex,
			DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
			IID_PPV_ARGS(&m_Adapter)));
		++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 desc;
		m_Adapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			continue;
		}

		// Test to make sure this adapter supports D3D12, but don't create the actual device yet
		if (SUCCEEDED(D3D12CreateDevice(m_Adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
		{

#ifdef _DEBUG
			std::wstring deviceInfo;
			deviceInfo += L"Selected device:\n\t";
			deviceInfo += desc.Description;
			deviceInfo += L"\n\tAvailable Dedicated Video Memory: ";
			deviceInfo += std::to_wstring(desc.DedicatedVideoMemory / 1000000000.f);
			deviceInfo += L" GB";
			LOG_INFO(deviceInfo.c_str());
#endif

			break;
		}
	}

	// make sure an adapter was created successfully
	ASSERT(m_Adapter, "Failed to find any adapter.");
}


void D3DGraphicsContext::CreateDevice()
{
	// Create the D3D12 Device object
	THROW_IF_FAIL(D3D12CreateDevice(m_Adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_Device)));
}

void D3DGraphicsContext::CreateCommandQueue()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	THROW_IF_FAIL(m_Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_CommandQueue)));
}

void D3DGraphicsContext::CreateSwapChain()
{
	// Describe and create the swap chain
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = s_FrameCount;
	swapChainDesc.Width = m_ClientWidth;
	swapChainDesc.Height = m_ClientHeight;
	swapChainDesc.Format = m_BackBufferFormat;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> swapChain;
	THROW_IF_FAIL(m_Factory->CreateSwapChainForHwnd(
		m_CommandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
		Win32Application::GetHwnd(),
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain
	));

	// Currently no support for fullscreen transitions
	THROW_IF_FAIL(m_Factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

	THROW_IF_FAIL(swapChain.As(&m_SwapChain));
	m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();
}

void D3DGraphicsContext::CreateDescriptorHeaps()
{
	m_RTVHeap = std::make_unique<D3DDescriptorHeap>(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, s_FrameCount, true);
	m_DSVHeap = std::make_unique<D3DDescriptorHeap>(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, true);

	// SRV/CBV/UAV heap
	constexpr UINT CBVCount = s_FrameCount * (s_MaxObjectCount + 2) + 1;	// frame count * (object count + 2) + sdf factory
	constexpr UINT SRVCount = 4;											// ImGui frame resource + SRV for scene texture + application resources
	constexpr UINT UAVCount = 2;											// UAV for scene texture + application resources
	m_SRVHeap = std::make_unique<D3DDescriptorHeap>(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, CBVCount + SRVCount + UAVCount, false);

	m_SamplerHeap = std::make_unique<D3DDescriptorHeap>(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 1, false);
}

void D3DGraphicsContext::CreateCommandAllocator()
{
	THROW_IF_FAIL(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_DirectCommandAllocator)));
}

void D3DGraphicsContext::CreateCommandList()
{
	// Create the command list
	THROW_IF_FAIL(m_Device->CreateCommandList(0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_DirectCommandAllocator.Get(), nullptr,
		IID_PPV_ARGS(&m_CommandList)));
}

void D3DGraphicsContext::CreateRTVs()
{
	m_RTVs = m_RTVHeap->Allocate(s_FrameCount);
	ASSERT(m_RTVs.IsValid(), "RTV descriptor alloc failed");

	// Create an RTV for each frame
	for (UINT n = 0; n < s_FrameCount; n++)
	{
		THROW_IF_FAIL(m_SwapChain->GetBuffer(n, IID_PPV_ARGS(&m_RenderTargets[n])));
		m_Device->CreateRenderTargetView(m_RenderTargets[n].Get(), nullptr, m_RTVs.GetCPUHandle(n));
	}
}

void D3DGraphicsContext::CreateDepthStencilBuffer()
{
	D3D12_RESOURCE_DESC desc;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Alignment = 0;
	desc.Width = m_ClientWidth;
	desc.Height = m_ClientHeight;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = m_DepthStencilFormat;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE clear;
	clear.Format = m_DepthStencilFormat;
	clear.DepthStencil.Depth = 1.0f;
	clear.DepthStencil.Stencil = 0;

	const CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_DEFAULT);

	THROW_IF_FAIL(m_Device->CreateCommittedResource(
		&heap,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_COMMON,
		&clear,
		IID_PPV_ARGS(&m_DepthStencilBuffer)));

	// Resource should be transitioned to be used as a depth stencil buffer
	const auto transition = CD3DX12_RESOURCE_BARRIER::Transition(m_DepthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	m_CommandList->ResourceBarrier(1, &transition);

	// Create DSV
	m_DSV = m_DSVHeap->Allocate(1);
	m_Device->CreateDepthStencilView(m_DepthStencilBuffer.Get(), nullptr, m_DSV.GetCPUHandle());
}

void D3DGraphicsContext::CreateFrameResources()
{
	m_FrameResources.clear();
	m_FrameResources.resize(s_FrameCount);
	for (auto& frameResources : m_FrameResources)
	{
		frameResources = std::make_unique<D3DFrameResources>();
	}
	m_CurrentFrameResources = m_FrameResources[m_FrameIndex].get();
}



void D3DGraphicsContext::CreateRaytracingInterface()
{
	THROW_IF_FAIL(m_Device->QueryInterface(IID_PPV_ARGS(&m_DXRDevice)));
	THROW_IF_FAIL(m_CommandList->QueryInterface(IID_PPV_ARGS(&m_DXRCommandList)));
}

void D3DGraphicsContext::CreateRaytracingRootSignatures()
{
	// Global root signature
	{
		CD3DX12_DESCRIPTOR_RANGE UAVDescriptor;
		UAVDescriptor.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

		CD3DX12_ROOT_PARAMETER rootParams[GlobalRootSignatureParams::Count];
		rootParams[GlobalRootSignatureParams::OutputViewSlot].InitAsDescriptorTable(1, &UAVDescriptor);
		rootParams[GlobalRootSignatureParams::AccelerationStructureSlot].InitAsShaderResourceView(0);

		CD3DX12_ROOT_SIGNATURE_DESC desc(ARRAYSIZE(rootParams), rootParams);
		SerializeAndCreateRootSignature(desc, &m_RayTracingGlobalRootSignature);
	}
}

void D3DGraphicsContext::CreateRaytracingPipelineStateObject()
{
	CD3DX12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

	// The DXIL library contains all of the shaders that will be used in raytracing
	// The entry points to export are specified
	const auto lib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();

	// Compile shader library from file
	ComPtr<IDxcBlob> blob;
	THROW_IF_FAIL(D3DShaderCompiler::CompileFromFile(L"assets/shaders/raytracing.hlsl", L"main", L"lib_6_3", nullptr, &blob));

	D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE(blob->GetBufferPointer(), blob->GetBufferSize());
	lib->SetDXILLibrary(&libdxil);

	// Define shader entry points to export from the library
	{
		lib->DefineExport(c_RaygenShaderName);
		lib->DefineExport(c_IntersectionShaderName);
		lib->DefineExport(c_ClosestHitShaderName);
		lib->DefineExport(c_MissShaderName);
	}

	// Define a hit group
	const auto hitGroup = raytracingPipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
	hitGroup->SetIntersectionShaderImport(c_IntersectionShaderName);
	hitGroup->SetClosestHitShaderImport(c_ClosestHitShaderName);
	hitGroup->SetHitGroupExport(c_HitGroupName);
	hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE);

	// Shader config
	const auto shaderConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
	constexpr UINT payloadSize = sizeof(XMFLOAT4); // Color
	constexpr UINT attributeSize = sizeof(XMFLOAT3);
	shaderConfig->Config(payloadSize, attributeSize);

	// Global root signature
	// This will be associated with all shaders via default export association
	const auto globalRootSig = raytracingPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	globalRootSig->SetRootSignature(m_RayTracingGlobalRootSignature.Get());

	// Pipeline config
	const auto pipelineConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
	constexpr UINT maxRecursion = 1; // primary rays only
	pipelineConfig->Config(maxRecursion);

	// Create the state object
	THROW_IF_FAIL(m_DXRDevice->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&m_DXRStateObject)));
}

void D3DGraphicsContext::CreateRaytracingOutputTexture()
{
	// Create resources
	const CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
	const auto resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(m_BackBufferFormat, m_ClientWidth, m_ClientHeight, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	THROW_IF_FAIL(m_Device->CreateCommittedResource(
		&defaultHeap,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr,
		IID_PPV_ARGS(&m_RayTracingOutputTexture)
	));

	// Create UAVs and SRVs
	m_RayTracingOutputTextureViews = m_SRVHeap->Allocate(2); // one UAV and one SRV

	// Create UAV
	m_Device->CreateUnorderedAccessView(m_RayTracingOutputTexture.Get(), nullptr, nullptr, m_RayTracingOutputTextureViews.GetCPUHandle(0));
	// Create SRV
	m_Device->CreateShaderResourceView(m_RayTracingOutputTexture.Get(), nullptr, m_RayTracingOutputTextureViews.GetCPUHandle(1));
}


void D3DGraphicsContext::BuildGeometry()
{
	// Create one AABB to ray-trace against
	constexpr D3D12_RAYTRACING_AABB aabb = {
		-0.5f, 0.5f,
		-0.5f, 0.5f,
		-0.5f, 0.5f
	};

	m_GeometryBuffer = std::make_unique<D3DUploadBuffer<D3D12_RAYTRACING_AABB>>(m_Device.Get(), 1, D3D12_RAYTRACING_AABB_BYTE_ALIGNMENT);
	m_GeometryBuffer->CopyData(0, aabb);
}

void D3DGraphicsContext::BuildAccelerationStructures()
{
	m_CommandList->Reset(m_DirectCommandAllocator.Get(), nullptr);

	D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
	geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS;
	geometryDesc.AABBs.AABBCount = 1;
	geometryDesc.AABBs.AABBs.StartAddress = m_GeometryBuffer->GetAddressOfElement(0);
	geometryDesc.AABBs.AABBs.StrideInBytes = m_GeometryBuffer->GetElementSize();

	geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

	// Get required size for an acceleration structure
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS topLevelInputs = {};
	topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	topLevelInputs.Flags = buildFlags;
	topLevelInputs.NumDescs = 1;
	topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
	m_DXRDevice->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
	ASSERT(topLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0, "Failed to get top level prebuild info.");

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS bottomLevelInputs = topLevelInputs;
	bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	bottomLevelInputs.pGeometryDescs = &geometryDesc;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};
	m_DXRDevice->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
	ASSERT(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0, "Failed to get top level prebuild info.");

	const auto defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	// Create scratch resource
	ComPtr<ID3D12Resource> scratchResource;
	{
		const auto desc = CD3DX12_RESOURCE_DESC::Buffer(max(topLevelPrebuildInfo.ScratchDataSizeInBytes, bottomLevelPrebuildInfo.ScratchDataSizeInBytes), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		THROW_IF_FAIL(m_Device->CreateCommittedResource(
			&defaultHeap,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS(&scratchResource)
		));
	}

	// Allocate resources for acceleration structures
	{
		const auto desc = CD3DX12_RESOURCE_DESC::Buffer(topLevelPrebuildInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		THROW_IF_FAIL(m_Device->CreateCommittedResource(
			&defaultHeap,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS(&m_TopLevelAccelerationStructure)
		));
	}
	{
		const auto desc = CD3DX12_RESOURCE_DESC::Buffer(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		THROW_IF_FAIL(m_Device->CreateCommittedResource(
			&defaultHeap,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS(&m_BottomLevelAccelerationStructure)
		));
	}

	// Create instance desc for bottom level acceleration structure
	D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
	// identity transform
	instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1;
	instanceDesc.InstanceMask = 1;
	instanceDesc.AccelerationStructure = m_BottomLevelAccelerationStructure->GetGPUVirtualAddress();

	// Allocate upload buffer for instances
	D3DUploadBuffer<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescs(m_Device.Get(), 1, false);
	instanceDescs.CopyData(0, instanceDesc);

	// Bottom level acceleration structure desc
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
	{
		bottomLevelBuildDesc.Inputs = bottomLevelInputs;
		bottomLevelBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
		bottomLevelBuildDesc.DestAccelerationStructureData = m_BottomLevelAccelerationStructure->GetGPUVirtualAddress();
	}

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
	{
		topLevelInputs.InstanceDescs = instanceDescs.GetAddressOfElement(0);
		topLevelBuildDesc.Inputs = topLevelInputs;
		topLevelBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
		topLevelBuildDesc.DestAccelerationStructureData = m_TopLevelAccelerationStructure->GetGPUVirtualAddress();
	}

	// Build acceleration structures
	m_DXRCommandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);

	const auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(m_BottomLevelAccelerationStructure.Get());
	m_CommandList->ResourceBarrier(1, &barrier);

	m_DXRCommandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
}

void D3DGraphicsContext::BuildShaderTables()
{
	void* rayGenShaderIdentifier;
	void* hitGroupShaderIdentifier;
	void* missShaderIdentifier;

	// Get shader identifiers
	UINT shaderIdentifierSize;
	{
		ComPtr<ID3D12StateObjectProperties> stateObjectProperties;
		THROW_IF_FAIL(m_DXRStateObject.As(&stateObjectProperties));
		rayGenShaderIdentifier = stateObjectProperties->GetShaderIdentifier(c_RaygenShaderName);
		hitGroupShaderIdentifier = stateObjectProperties->GetShaderIdentifier(c_HitGroupName);
		missShaderIdentifier = stateObjectProperties->GetShaderIdentifier(c_MissShaderName);
		shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	}

	// Create shader tables

	// Raygen shader table
	{
		constexpr UINT numRecords = 1;
		const UINT shaderRecordSize = shaderIdentifierSize;
		m_RayGenShaderTable = std::make_unique<D3DShaderTable>(m_Device.Get(), numRecords, shaderRecordSize);
		m_RayGenShaderTable->AddRecord({ rayGenShaderIdentifier, shaderIdentifierSize });
	}

	// Miss shader table
	{
		constexpr UINT numRecords = 1;
		const UINT shaderRecordSize = shaderIdentifierSize;
		m_MissShaderTable = std::make_unique<D3DShaderTable>(m_Device.Get(), numRecords, shaderRecordSize);
		m_MissShaderTable->AddRecord({ missShaderIdentifier, shaderIdentifierSize });
	}

	// Hit group shader table
	{
		constexpr UINT numRecords = 1;
		const UINT shaderRecordSize = shaderIdentifierSize;
		m_HitGroupShaderTable = std::make_unique<D3DShaderTable>(m_Device.Get(), numRecords, shaderRecordSize);
		m_HitGroupShaderTable->AddRecord({ hitGroupShaderIdentifier, shaderIdentifierSize });
	}
}


bool D3DGraphicsContext::CheckRaytracingSupport() const
{
	ComPtr<ID3D12Device> testDevice;
	D3D12_FEATURE_DATA_D3D12_OPTIONS5 featureSupportData = {};

	return SUCCEEDED(D3D12CreateDevice(m_Adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&testDevice)))
		&& SUCCEEDED(testDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &featureSupportData, sizeof(featureSupportData)))
		&& featureSupportData.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
}

void D3DGraphicsContext::SerializeAndCreateRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* rootSig) const
{
	ComPtr<ID3DBlob> blob;
	ComPtr<ID3DBlob> error;

	THROW_IF_FAIL(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error));
	THROW_IF_FAIL(m_Device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(*rootSig))));
}



void D3DGraphicsContext::CreateViewport()
{
	m_Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(m_ClientWidth), static_cast<float>(m_ClientHeight));
}

void D3DGraphicsContext::CreateScissorRect()
{
	m_ScissorRect = CD3DX12_RECT(0, 0, static_cast<LONG>(m_ClientWidth), static_cast<LONG>(m_ClientHeight));
}

void D3DGraphicsContext::CreateFence()
{
	THROW_IF_FAIL(m_Device->CreateFence(0,
		D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence)));
	m_CurrentFrameResources->IncrementFence();

	// Create an event handle to use for frame synchronization
	m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (m_FenceEvent == nullptr)
	{
		THROW_IF_FAIL(HRESULT_FROM_WIN32(GetLastError()));
	}
}

void D3DGraphicsContext::CreateProjectionMatrix()
{
	m_ProjectionMatrix = XMMatrixPerspectiveFovLH(m_FOV, GetAspectRatio(), m_NearPlane, m_FarPlane);
}


void D3DGraphicsContext::CreateAssets()
{
	m_GraphicsPipeline = std::make_unique<D3DGraphicsPipeline>();

	// Create raytracing assets
	BuildGeometry();
	BuildAccelerationStructures();
	BuildShaderTables();
}


void D3DGraphicsContext::MoveToNextFrame()
{
	const UINT64 currentFenceValue = m_CurrentFrameResources->GetFenceValue();
	THROW_IF_FAIL(m_CommandQueue->Signal(m_Fence.Get(), currentFenceValue));

	// Update the frame index
	m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();
	m_CurrentFrameResources = m_FrameResources[m_FrameIndex].get();

	// if the next frame is not ready to be rendered yet, wait until it is ready
	if (m_Fence->GetCompletedValue() < m_CurrentFrameResources->GetFenceValue())
	{
		THROW_IF_FAIL(m_Fence->SetEventOnCompletion(m_CurrentFrameResources->GetFenceValue(), m_FenceEvent));
		WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
	}

	// Set the fence value for the next frame
	m_CurrentFrameResources->SetFence(currentFenceValue + 1);
}

void D3DGraphicsContext::ProcessDeferrals(UINT frameIndex) const
{
	if (m_FrameResources[frameIndex])
		m_FrameResources[frameIndex]->ProcessDeferrals();

	m_RTVHeap->ProcessDeferredFree(frameIndex);
	m_DSVHeap->ProcessDeferredFree(frameIndex);
	m_SRVHeap->ProcessDeferredFree(frameIndex);
}

void D3DGraphicsContext::ProcessAllDeferrals() const
{
	for (UINT n = 0; n < s_FrameCount; ++n)
		ProcessDeferrals(n);
}
