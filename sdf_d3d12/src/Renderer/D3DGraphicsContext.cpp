#include "pch.h"
#include "D3DGraphicsContext.h"

#include "D3DDebugTools.h"
#include "Windows/Win32Application.h"

#include "D3DFrameResources.h"

#include "Framework/GameTimer.h"
#include "Framework/Camera.h"

#include "pix3.h"


D3DGraphicsContext* g_D3DGraphicsContext = nullptr;


D3DGraphicsContext::D3DGraphicsContext(HWND window, UINT width, UINT height)
	: m_WindowHandle(window)
	, m_ClientWidth(width)
	, m_ClientHeight(height)
	, m_BackBufferFormat(DXGI_FORMAT_R8G8B8A8_UNORM)
{
	ASSERT(!g_D3DGraphicsContext, "Cannot initialize a second graphics context!");
	g_D3DGraphicsContext = this;

	LOG_INFO("Creating D3D12 Graphics Context");

	// Init PIX
	if (s_EnablePIXCaptures)
	{
		m_PIXCaptureModule = PIXLoadLatestWinPixGpuCapturerLibrary();
		if (!m_PIXCaptureModule)
		{
			LOG_ERROR("Failed to load PIX capture library.");
		}
	}

	// Initialize D3D components
	CreateAdapter();
	ASSERT(CheckRaytracingSupport(), "Adapter does not support raytracing.");
 
	// Create m_Device resources
	CreateDevice();
	CreateCommandQueues();
	CreateSwapChain();
	CreateDescriptorHeaps();
	CreateCommandAllocator();
	CreateCommandList();

	CreateRTVs();
	CreateFrameResources();

	// Setup for raytracing
	{
		// Create DXR resources
		CreateRaytracingInterfaces();
	}

	m_ImGuiResources = m_SRVHeap->Allocate(1);
	ASSERT(m_ImGuiResources.IsValid(), "Failed to alloc");

	CreateProjectionMatrix();

	// Close the command list and execute it to begin the initial GPU setup
	// The main loop expects the command list to be closed anyway
	THROW_IF_FAIL(m_CommandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
	const auto fenceValue = m_DirectQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	m_CurrentFrameResources->SetFence(fenceValue);

	// Wait for any GPU work executed on startup to finish before continuing
	m_DirectQueue->WaitForIdleCPUBlocking();

	// Temporary resources that were created during initialization
	// can now safely be released that the GPU has finished its work
	ProcessAllDeferrals();

	LOG_INFO("D3D12 Graphics Context created succesfully.");
}

D3DGraphicsContext::~D3DGraphicsContext()
{
	// Ensure the GPU is no longer references resources that are about
	// to be cleaned up by the destructor
	WaitForGPUIdle();

	// Free allocations
	m_RTVs.Free();
	m_ImGuiResources.Free();

	// Free frame resources
	for (UINT n = 0; n < s_FrameCount; n++)
		m_FrameResources[n].reset();

	// Free resources that themselves might free more resources
	ProcessAllDeferrals();

	if (m_InfoQueue)
	{
		(void)m_InfoQueue->UnregisterMessageCallback(m_MessageCallbackCookie);
	}

	if (m_PIXCaptureModule)
	{
		FreeLibrary(m_PIXCaptureModule);
	}
}


void D3DGraphicsContext::Present()
{
	PIXBeginEvent(PIX_COLOR_INDEX(4), L"Present");

	const auto flags = m_WindowedMode && !m_VSyncEnabled ? DXGI_PRESENT_ALLOW_TEARING : 0;
	const auto interval = m_VSyncEnabled ? 1 : 0;

	const HRESULT result = m_SwapChain->Present(interval, flags);
	if (FAILED(result))
	{
		switch(result)
		{
		case DXGI_ERROR_DEVICE_RESET:
		case DXGI_ERROR_DEVICE_REMOVED:
			LOG_FATAL("Present Failed: Device Removed!");
			break;
		default:
			LOG_FATAL("Present Failed: Unknown Error!");
			break;
		}
	}

	MoveToNextFrame();
	ProcessDeferrals(m_FrameIndex);

	PIXEndEvent();
}

bool D3DGraphicsContext::CheckDeviceRemovedStatus() const
{
	const HRESULT result = m_Device->GetDeviceRemovedReason();
	if (result != S_OK)
	{
		LOG_ERROR(L"Device removed reason: {}", DXException(result).ToString().c_str());
		return true;
	}
	return false;
}


void D3DGraphicsContext::StartDraw() const
{
	PIXBeginEvent(PIX_COLOR_INDEX(2), L"Start Draw");

	// Command list allocators can only be reset when the associated
	// command lists have finished execution on the GPU
	m_CurrentFrameResources->ResetAllocator();

	// Command lists can (and must) be reset after ExecuteCommandList() is called and before it is repopulated
	THROW_IF_FAIL(m_CommandList->Reset(m_CurrentFrameResources->GetCommandAllocator(), nullptr));

	PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(2), "Begin Frame");

	// Indicate the back buffer will be used as a render target
	const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_RenderTargets[m_FrameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_CommandList->ResourceBarrier(1, &barrier);

	const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_RTVs.GetCPUHandle(m_FrameIndex);
	m_CommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	// Setup descriptor heaps
	ID3D12DescriptorHeap* ppHeaps[] = { m_SRVHeap->GetHeap(), m_SamplerHeap->GetHeap() };
	m_CommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	PIXEndEvent();
}

void D3DGraphicsContext::EndDraw() const
{
	PIXBeginEvent(PIX_COLOR_INDEX(3), L"End Draw");

	// Indicate that the back buffer will now be used to present
	const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_RenderTargets[m_FrameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	m_CommandList->ResourceBarrier(1, &barrier);

	PIXEndEvent(m_CommandList.Get());

	THROW_IF_FAIL(m_CommandList->Close());

	// Execute the command list
	ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
	const auto fenceValue = m_DirectQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	m_CurrentFrameResources->SetFence(fenceValue);

	PIXEndEvent();
}

void D3DGraphicsContext::CopyRaytracingOutput(ID3D12Resource* raytracingOutput) const
{
	PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(71), "Copy Raytracing Output");

	const auto renderTarget = m_RenderTargets[m_FrameIndex].Get();

	D3D12_RESOURCE_BARRIER preCopyBarriers[2];
	preCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
	preCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(raytracingOutput, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	m_CommandList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);

	m_CommandList->CopyResource(renderTarget, raytracingOutput);

	D3D12_RESOURCE_BARRIER postCopyBarriers[2];
	postCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
	postCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(raytracingOutput, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	m_CommandList->ResourceBarrier(ARRAYSIZE(postCopyBarriers), postCopyBarriers);

	PIXEndEvent(m_CommandList.Get());
}


void D3DGraphicsContext::UpdatePassCB(GameTimer* timer, Camera* camera, UINT flags)
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

	m_MainPassCB.Flags = flags;

	m_MainPassCB.RTSize = { static_cast<float>(m_ClientWidth), static_cast<float>(m_ClientHeight) };
	m_MainPassCB.InvRTSize = { 1.0f / m_MainPassCB.RTSize.x, 1.0f / m_MainPassCB.RTSize.y };

	m_MainPassCB.TotalTime = timer->GetTimeSinceReset();
	m_MainPassCB.DeltaTime = timer->GetDeltaTime();

	m_CurrentFrameResources->CopyPassData(m_MainPassCB);
}

D3D12_GPU_VIRTUAL_ADDRESS D3DGraphicsContext::GetPassCBAddress() const
{
	return m_CurrentFrameResources->GetPassCBAddress();
}

void D3DGraphicsContext::DeferRelease(const ComPtr<IUnknown>& resource) const
{
	m_CurrentFrameResources->DeferRelease(resource);
}



void D3DGraphicsContext::Resize(UINT width, UINT height)
{
	ASSERT(width && height, "Invalid client size!");

	m_ClientWidth = width;
	m_ClientHeight = height;

	// Wait for any work currently being performed by the GPU to finish
	WaitForGPUIdle();

	THROW_IF_FAIL(m_CommandList->Reset(m_DirectCommandAllocator.Get(), nullptr));

	// Release the previous resources that will be recreated
	for (UINT n = 0; n < s_FrameCount; ++n)
		m_RenderTargets[n].Reset();
	m_RTVs.Free();

	// Process all deferred frees
	ProcessAllDeferrals();

	const auto flags = m_TearingSupport ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
	THROW_IF_FAIL(m_SwapChain->ResizeBuffers(s_FrameCount, m_ClientWidth, m_ClientHeight, m_BackBufferFormat, flags));

	BOOL fullscreenState;
	THROW_IF_FAIL(m_SwapChain->GetFullscreenState(&fullscreenState, nullptr));
	m_WindowedMode = !fullscreenState;

	m_FrameIndex = 0;
	CreateRTVs();

	// Send required work to re-init buffers
	THROW_IF_FAIL(m_CommandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
	const auto fenceValue = m_DirectQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	m_CurrentFrameResources->SetFence(fenceValue);

	// Recreate other objects while GPU is doing work
	CreateProjectionMatrix();

	// Wait for GPU to finish its work before continuing
	m_DirectQueue->WaitForIdleCPUBlocking();
}

void D3DGraphicsContext::WaitForGPUIdle() const
{
	m_DirectQueue->WaitForIdleCPUBlocking();
	m_ComputeQueue->WaitForIdleCPUBlocking();
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

	// Enable DRED
	ComPtr<ID3D12DeviceRemovedExtendedDataSettings1> dredSettings;
	if (s_EnableDRED && SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dredSettings))))
	{
		// Turn on AutoBreadcrumbs and Page Fault reporting
		dredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
		dredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);

		LOG_INFO("DRED Enabled")
	}

#endif

	THROW_IF_FAIL(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_Factory)));

	{
		BOOL allowTearing = FALSE;
		const HRESULT hr = m_Factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
		m_TearingSupport = SUCCEEDED(hr) && allowTearing;
	}

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

		// Test to make sure this adapter supports D3D12, but don't create the actual m_Device yet
		if (SUCCEEDED(D3D12CreateDevice(m_Adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
		{

#ifdef _DEBUG
			std::wstring deviceInfo;
			deviceInfo += L"Selected m_Device:\n\t";
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
	D3D_NAME(m_Device);

#ifdef _DEBUG
	// Set up the info queue for the device
	// Debug layer must be enabled for this: so only perform this in debug
	// Note that means that m_InfoQueue should always be checked for existence before use
	if (SUCCEEDED(m_Device->QueryInterface(IID_PPV_ARGS(&m_InfoQueue))))
	{
		// Set up message callback
		THROW_IF_FAIL(m_InfoQueue->RegisterMessageCallback(D3DDebugTools::D3DMessageHandler, D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, &m_MessageCallbackCookie));
		LOG_INFO("D3D Info Queue message callback created.");
	}
	else
	{
		LOG_WARN("D3D Info Queue interface not available! D3D messages will not be received.");
	}
#endif
}

void D3DGraphicsContext::CreateCommandQueues()
{
	m_DirectQueue = std::make_unique<D3DQueue>(m_Device.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT, L"Direct Queue");
	m_ComputeQueue = std::make_unique<D3DQueue>(m_Device.Get(), D3D12_COMMAND_LIST_TYPE_COMPUTE, L"Compute Queue");
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
	swapChainDesc.Flags = m_TearingSupport ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	ComPtr<IDXGISwapChain1> swapChain;
	THROW_IF_FAIL(m_Factory->CreateSwapChainForHwnd(
		m_DirectQueue->GetCommandQueue(),			// Swap chain needs the queue so that it can force a flush on it.
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
	m_RTVHeap = std::make_unique<DescriptorHeap>(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, s_FrameCount, true);

	// SRV/CBV/UAV heap
	constexpr UINT Count = 64;
	m_SRVHeap = std::make_unique<DescriptorHeap>(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, Count, false);

	m_SamplerHeap = std::make_unique<DescriptorHeap>(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 8, false);
}

void D3DGraphicsContext::CreateCommandAllocator()
{
	THROW_IF_FAIL(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_DirectCommandAllocator)));
	D3D_NAME(m_DirectCommandAllocator);
}

void D3DGraphicsContext::CreateCommandList()
{
	// Create the command list
	THROW_IF_FAIL(m_Device->CreateCommandList(0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_DirectCommandAllocator.Get(), nullptr,
		IID_PPV_ARGS(&m_CommandList)));
	D3D_NAME(m_CommandList);
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
		D3D_NAME(m_RenderTargets[n]);
	}
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

bool D3DGraphicsContext::CheckRaytracingSupport() const
{
	ComPtr<ID3D12Device> testDevice;
	D3D12_FEATURE_DATA_D3D12_OPTIONS5 featureSupportData = {};

	return SUCCEEDED(D3D12CreateDevice(m_Adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&testDevice)))
		&& SUCCEEDED(testDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &featureSupportData, sizeof(featureSupportData)))
		&& featureSupportData.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
}

// Create raytracing m_Device and command list.
void D3DGraphicsContext::CreateRaytracingInterfaces()
{
	THROW_IF_FAIL(m_Device->QueryInterface(IID_PPV_ARGS(&m_DXRDevice)));
	D3D_NAME(m_DXRDevice);
	THROW_IF_FAIL(m_CommandList->QueryInterface(IID_PPV_ARGS(&m_DXRCommandList)));
	D3D_NAME(m_DXRCommandList);
}


void D3DGraphicsContext::CreateProjectionMatrix()
{
	m_ProjectionMatrix = XMMatrixPerspectiveFovLH(m_FOV, GetAspectRatio(), m_NearPlane, m_FarPlane);
}

void D3DGraphicsContext::MoveToNextFrame()
{
	PIXBeginEvent(PIX_COLOR_INDEX(5), L"Move to next frame");

	// Change the frame resources
	m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();
	m_CurrentFrameResources = m_FrameResources[m_FrameIndex].get();

	// If they are still being processed by the GPU, then wait until they are ready
	m_DirectQueue->WaitForFenceCPUBlocking(m_CurrentFrameResources->GetFenceValue());

	PIXEndEvent();
}

void D3DGraphicsContext::ProcessDeferrals(UINT frameIndex) const
{
	if (m_FrameResources[frameIndex])
		m_FrameResources[frameIndex]->ProcessDeferrals();

	m_RTVHeap->ProcessDeferredFree(frameIndex);
	m_SRVHeap->ProcessDeferredFree(frameIndex);
	m_SamplerHeap->ProcessDeferredFree(frameIndex);
}

void D3DGraphicsContext::ProcessAllDeferrals() const
{
	for (UINT n = 0; n < s_FrameCount; ++n)
		ProcessDeferrals(n);
}
