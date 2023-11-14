#include "pch.h"
#include "D3DGraphicsContext.h"

#include "Windows/Win32Application.h"

#include "Memory/D3DMemoryAllocator.h"
#include "D3DFrameResources.h"

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
	CreateDevice();
	CreateCommandQueue();
	CreateSwapChain();
	CreateDescriptorHeaps();
	CreateCommandAllocator();
	CreateCommandList();

	CreateRTVs();
	CreateDepthStencilBuffer();

	CreateFrameResources();

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

	m_SceneTextureViews.Free();

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
}

void D3DGraphicsContext::DrawVolume(D3DComputePipeline* pipeline, D3D12_GPU_DESCRIPTOR_HANDLE volumeSRV) const
{
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
			objectCB.InvWorldMat = XMMatrixTranspose(XMMatrixInverse(nullptr, renderItem.GetWorldMatrix()));
			objectCB.Scale = renderItem.GetScale();

			const SDFPrimitive& primitive = renderItem.GetSDFPrimitiveData();

			objectCB.Shape = static_cast<UINT>(primitive.Shape);
			memcpy_s(&objectCB.ShapeProperties, sizeof(SDFShapeProperties), &primitive.ShapeProperties, sizeof(SDFShapeProperties));

			objectCB.Operation = static_cast<UINT>(primitive.Operation);
			objectCB.BlendingFactor = primitive.BlendingFactor;
			objectCB.Color = primitive.Color;

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
	m_SceneTexture.Reset();
	m_SceneTextureViews.Free();

	// Process all deferred frees
	ProcessAllDeferrals();

	THROW_IF_FAIL(m_SwapChain->ResizeBuffers(s_FrameCount, m_ClientWidth, m_ClientHeight, m_BackBufferFormat, 0));

	m_FrameIndex = 0;

	CreateRTVs();
	CreateDepthStencilBuffer();

	// Re-create application-specific, window specific resources
	CreateSceneTexture();

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



void D3DGraphicsContext::CreateDevice()
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

	ComPtr<IDXGIAdapter1> hardwareAdapter;
	for (UINT adapterIndex = 0;
		SUCCEEDED(m_Factory->EnumAdapterByGpuPreference(
			adapterIndex,
			DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
			IID_PPV_ARGS(&hardwareAdapter)));
		++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 desc;
		hardwareAdapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			continue;
		}

		if (SUCCEEDED(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_Device))))
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
	// make sure a device was created successfully
	ASSERT(m_Device, "Failed to create any device; no hardware device found.");
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
	constexpr UINT CBVCount = s_FrameCount * (s_MaxObjectCount + 2);	// frame count * (object count + 2)
	constexpr UINT SRVCount = 3;										// ImGui frame resource + SRV for scene textures + application resources
	constexpr UINT UAVCount = 2;										// UAV for scene textures + application resources
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

	CreateSceneTexture();
}


void D3DGraphicsContext::CreateSceneTexture()
{
	// Create resources
	const CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
	const auto resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, m_ClientWidth, m_ClientHeight, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	THROW_IF_FAIL(m_Device->CreateCommittedResource(
		&defaultHeap,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&m_SceneTexture)
	));

	// Use fast ceiling of integer division
	m_ThreadGroupX = (m_ClientWidth + s_NumShaderThreads  - 1) / s_NumShaderThreads;
	m_ThreadGroupY = (m_ClientHeight + s_NumShaderThreads - 1) / s_NumShaderThreads;

	// Create UAVs and SRVs
	m_SceneTextureViews = m_SRVHeap->Allocate(2); // one UAV and one SRV

	// Create UAV
	m_Device->CreateUnorderedAccessView(m_SceneTexture.Get(), nullptr, nullptr, m_SceneTextureViews.GetCPUHandle(0));
	// Create SRV
	m_Device->CreateShaderResourceView(m_SceneTexture.Get(), nullptr, m_SceneTextureViews.GetCPUHandle(1));
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
