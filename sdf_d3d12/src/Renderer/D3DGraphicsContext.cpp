#include "pch.h"
#include "D3DGraphicsContext.h"

#include "Memory/D3DMemoryAllocator.h"
#include "Windows/Win32Application.h"


D3DGraphicsContext* g_D3DGraphicsContext = nullptr;


D3DGraphicsContext::D3DGraphicsContext(HWND window, UINT width, UINT height)
	: m_WindowHandle(window)
	, m_ClientWidth(width)
	, m_ClientHeight(height)
	, m_BackBufferFormat(DXGI_FORMAT_R8G8B8A8_UNORM)
{
	assert(!g_D3DGraphicsContext && "Cannot initialize a second graphics context!");
	g_D3DGraphicsContext = this;

	// Initialize D3D components
	CreateDevice();
	CreateCommandQueue();
	CreateSwapChain();
	CreateDescriptorHeaps();
	CreateCommandAllocator();
	CreateRenderTargetViews();
	CreateFrameResources();

	CreateViewport();
	CreateScissorRect();

	CreateFence();

	m_ImGuiResources = m_SRVHeap->Allocate(1);
	ASSERT(m_ImGuiResources.IsValid(), "Failed to alloc");

	CreateAssets();

	// Wait for any GPU work executed on startup to finish before continuing
	WaitForGPU();
}

D3DGraphicsContext::~D3DGraphicsContext()
{
	// Ensure the GPU is no longer references resources that are about
	// to be cleaned up by the destructor
	WaitForGPU();

	// Free allocations
	m_RTVHeap->Free(m_RTVs);
	m_SRVHeap->Free(m_ImGuiResources);

	for (UINT n = 0; n < s_FrameCount; n++)
		ProcessDeferrals(n);

	CloseHandle(m_FenceEvent);
}


void D3DGraphicsContext::Present()
{
	// TODO: This could fail for a reason, it shouldn't always throw
	THROW_IF_FAIL(m_SwapChain->Present(1, 0));

	MoveToNextFrame();
	ProcessDeferrals(m_FrameIndex);
}

void D3DGraphicsContext::StartDraw() const
{
	// Command list allocators can only be reset when the associated
	// command lists have finished execution on the GPU
	THROW_IF_FAIL(m_FrameResources[m_FrameIndex].CommandAllocator->Reset());

	// Command lists can (and must) be reset after ExecuteCommandList() is called and before it is repopulated
	THROW_IF_FAIL(m_CommandList->Reset(m_FrameResources[m_FrameIndex].CommandAllocator.Get(), m_PipelineState.Get()));

	// Indicate the back buffer will be used as a render target
	const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_RenderTargets[m_FrameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_CommandList->ResourceBarrier(1, &barrier);

	const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_RTVs.GetCPUHandle(m_FrameIndex);
	m_CommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	// Clear render target
	constexpr float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	m_CommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

	m_CommandList->RSSetViewports(1, &m_Viewport);
	m_CommandList->RSSetScissorRects(1, &m_ScissorRect);

	// Setup descriptor heaps
	ID3D12DescriptorHeap* ppHeaps[] = { m_SRVHeap->GetHeap() };
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

void D3DGraphicsContext::PopulateCommandList(D3D12_GPU_DESCRIPTOR_HANDLE cbv) const
{
	// These are user commands that draw whatever is desired

	// Set necessary state
	m_CommandList->SetGraphicsRootSignature(m_RootSignature.Get());

	// Get gpu virtual address of constant buffer contents to use for rendering
	m_CommandList->SetGraphicsRootDescriptorTable(0, cbv);

	// render the triangle
	m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_CommandList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
	m_CommandList->IASetIndexBuffer(&m_IndexBufferView);
	m_CommandList->DrawIndexedInstanced(3, 1, 0, 0, 0);
}



void D3DGraphicsContext::Flush()
{
	THROW_IF_FAIL(m_CommandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
	m_CommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	WaitForGPU();
}

void D3DGraphicsContext::WaitForGPU()
{
	// Signal and increment the fence
	THROW_IF_FAIL(m_CommandQueue->Signal(m_Fence.Get(), m_FrameResources[m_FrameIndex].FenceValue));

	THROW_IF_FAIL(m_Fence->SetEventOnCompletion(m_FrameResources[m_FrameIndex].FenceValue, m_FenceEvent));
	WaitForSingleObject(m_FenceEvent, INFINITE);

	m_FrameResources[m_FrameIndex].FenceValue++;
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
			break;
		}
	}
	// make sure a device was created successfully
	ASSERT(m_Device, "Failed to create any device.");
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
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
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
	m_SRVHeap = std::make_unique<D3DDescriptorHeap>(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 3, false);
}

void D3DGraphicsContext::CreateCommandAllocator()
{
	THROW_IF_FAIL(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_DirectCommandAllocator)));
}

void D3DGraphicsContext::CreateRenderTargetViews()
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

void D3DGraphicsContext::CreateFrameResources()
{
	for (UINT n = 0; n < s_FrameCount; n++)
	{
		// Create command allocator
		THROW_IF_FAIL(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_FrameResources[n].CommandAllocator)));
	}
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
	m_FrameResources[m_FrameIndex].FenceValue = 1;

	// Create an event handle to use for frame synchronization
	m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (m_FenceEvent == nullptr)
	{
		THROW_IF_FAIL(HRESULT_FROM_WIN32(GetLastError()));
	}
}

void D3DGraphicsContext::CreateAssets()
{
	// Create root signature
	{
		// At the moment, we only need to provide the constant buffer to the pixel shader
		// Create a root signature consisting of a descriptor table with a single CBV

		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
		if (FAILED(m_Device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
		CD3DX12_ROOT_PARAMETER1 rootParameters[1];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);

		// Allow input layout and deny unnecessary access to certain pipeline stages
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		THROW_IF_FAIL(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
		THROW_IF_FAIL(m_Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_RootSignature)));
	}

	// Create the pipeline state, which includes compiling and loading shaders
	{
		ComPtr<ID3DBlob> vertexShader;
		ComPtr<ID3DBlob> pixelShader;

#ifdef _DEBUG
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif

		// Compile shaders
		THROW_IF_FAIL(D3DCompileFromFile(L"assets/shaders/shaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
		THROW_IF_FAIL(D3DCompileFromFile(L"assets/shaders/shaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

		// Define the vertex input layout
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		// Describe and create the graphics pipeline state object (PSO)
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = m_RootSignature.Get();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		THROW_IF_FAIL(m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_PipelineState)));
	}

	// Create the command list
	THROW_IF_FAIL(m_Device->CreateCommandList(0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_DirectCommandAllocator.Get(), nullptr,
		IID_PPV_ARGS(&m_CommandList)));


	// Create the vertex buffer
	ComPtr<ID3D12Resource> vbUploadHeap;
	{
		// Define the geometry for a triangle.
		Vertex triangleVertices[] =
		{
			{ { 0.0f, 0.25f * GetAspectRatio(), 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
			{ { 0.25f, -0.25f * GetAspectRatio(), 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
			{ { -0.25f, -0.25f * GetAspectRatio(), 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
		};

		constexpr UINT vertexBufferSize = sizeof(triangleVertices);

		// Create a buffer (with default heap usage)
		CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
		auto vbDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);

		THROW_IF_FAIL(m_Device->CreateCommittedResource(
			&defaultHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&vbDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_VertexBuffer)));

		// Create an intermediate buffer (with upload heap usage)
		CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
		auto intermediateDesc = CD3DX12_RESOURCE_DESC::Buffer(GetRequiredIntermediateSize(m_VertexBuffer.Get(), 0, 1));

		THROW_IF_FAIL(m_Device->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&intermediateDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&vbUploadHeap)));

		D3D12_SUBRESOURCE_DATA vbData = {};
		vbData.pData = &triangleVertices;
		vbData.RowPitch = vertexBufferSize;
		vbData.SlicePitch = vertexBufferSize;

		UpdateSubresources(m_CommandList.Get(), m_VertexBuffer.Get(), vbUploadHeap.Get(), 0, 0, 1, &vbData);

		const auto resourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(m_VertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		m_CommandList->ResourceBarrier(1, &resourceBarrier);

		// Initialize the vertex buffer view
		m_VertexBufferView.BufferLocation = m_VertexBuffer->GetGPUVirtualAddress();
		m_VertexBufferView.StrideInBytes = sizeof(Vertex);
		m_VertexBufferView.SizeInBytes = vertexBufferSize;
	}

	// Create the index buffer
	ComPtr<ID3D12Resource> ibUploadHeap;
	{
		// Define indices
		UINT indices[] =
		{
			0, 1, 2
		};

		constexpr UINT indexBufferSize = sizeof(indices);

		// Create a buffer (with default heap usage)
		CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
		auto ibDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);

		THROW_IF_FAIL(m_Device->CreateCommittedResource(
			&defaultHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&ibDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_IndexBuffer)));

		// Create an intermediate buffer (with upload heap usage)
		CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
		auto intermediateDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);

		THROW_IF_FAIL(m_Device->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&intermediateDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&ibUploadHeap)));

		D3D12_SUBRESOURCE_DATA ibData = {};
		ibData.pData = &indices;
		ibData.RowPitch = indexBufferSize;
		ibData.SlicePitch = indexBufferSize;

		UpdateSubresources(m_CommandList.Get(), m_IndexBuffer.Get(), ibUploadHeap.Get(), 0, 0, 1, &ibData);

		const auto resourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(m_IndexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
		m_CommandList->ResourceBarrier(1, &resourceBarrier);

		// Initialize the index buffer view
		m_IndexBufferView.BufferLocation = m_IndexBuffer->GetGPUVirtualAddress();
		m_IndexBufferView.SizeInBytes = indexBufferSize;
		m_IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	}

	// Close the command list and execute it to begin the initial GPU setup
	THROW_IF_FAIL(m_CommandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
	m_CommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Need to wait as upload heaps may still be in use
	WaitForGPU();
}


void D3DGraphicsContext::MoveToNextFrame()
{
	const UINT64 currentFenceValue = m_FrameResources[m_FrameIndex].FenceValue;
	THROW_IF_FAIL(m_CommandQueue->Signal(m_Fence.Get(), currentFenceValue));

	// Update the frame index
	m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();

	// if the next frame is not ready to be rendered yet, wait until it is ready
	if (m_Fence->GetCompletedValue() < m_FrameResources[m_FrameIndex].FenceValue)
	{
		THROW_IF_FAIL(m_Fence->SetEventOnCompletion(m_FrameResources[m_FrameIndex].FenceValue, m_FenceEvent));
		WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
	}

	// Set the fence value for the next frame
	m_FrameResources[m_FrameIndex].FenceValue = currentFenceValue + 1;
}

void D3DGraphicsContext::ProcessDeferrals(UINT frameIndex) const
{
	m_RTVHeap->ProcessDeferredFree(frameIndex);
	m_SRVHeap->ProcessDeferredFree(frameIndex);
}
