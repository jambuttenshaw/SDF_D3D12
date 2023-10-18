#include "pch.h"
#include "D3D12Application.h"

#include "Windows/Win32Application.h"

#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"


D3D12Application::D3D12Application(UINT width, UINT height, const std::wstring& name)
	: BaseApplication(width, height, name)
	, m_Viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height))
	, m_ScissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height))
	, m_FenceEvent(nullptr)
{
}

void D3D12Application::OnInit()
{
	LoadPipeline();
	LoadImGui();
	LoadAssets();
}


void D3D12Application::LoadPipeline()
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

	ComPtr<IDXGIFactory6> factory;
	THROW_IF_FAIL(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

	ComPtr<IDXGIAdapter1> hardwareAdapter;
	for (UINT adapterIndex = 0;
			SUCCEEDED(factory->EnumAdapterByGpuPreference(
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
	ASSERT(m_Device);

	// Create command queue
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	THROW_IF_FAIL(m_Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_CommandQueue)));

	// Describe and create the swap chain
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = s_FrameCount;
	swapChainDesc.Width = m_Width;
	swapChainDesc.Height = m_Height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> swapChain;
	THROW_IF_FAIL(factory->CreateSwapChainForHwnd(
		m_CommandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
		Win32Application::GetHwnd(),
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain
	));

	// Currently no support for fullscreen transitions
	THROW_IF_FAIL(factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

	THROW_IF_FAIL(swapChain.As(&m_SwapChain));
	m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();

	// Create descriptor heaps
	{
		// Describe and create a render target view (RTV) descriptor heap
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.NumDescriptors = s_FrameCount;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		THROW_IF_FAIL(m_Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_RTVHeap)));

		m_RTVDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		// Create a descriptor heap for CBVs, SRVs, and UAVs
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.NumDescriptors = 3;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // Descriptor heap will also exist on the GPU
		THROW_IF_FAIL(m_Device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_SRVHeap)));

		m_SRVDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	// Create command allocator
	THROW_IF_FAIL(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_DirectCommandAllocator)));

	// Create frame resources
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_RTVHeap->GetCPUDescriptorHandleForHeapStart());

		// Create an RTV for each frame
		for (UINT n = 0; n < s_FrameCount; n++)
		{
			THROW_IF_FAIL(m_SwapChain->GetBuffer(n, IID_PPV_ARGS(&m_RenderTargets[n])));
			m_Device->CreateRenderTargetView(m_RenderTargets[n].Get(), nullptr, rtvHandle);
			rtvHandle.Offset(1, m_RTVDescriptorSize);
			
			// Create command allocator
			THROW_IF_FAIL(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_FrameResources[n].CommandAllocator)));
		}
	}

}

void D3D12Application::LoadImGui()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	ImGui::StyleColorsDark();

	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	// Setup platform and renderer back-ends
	ImGui_ImplWin32_Init(Win32Application::GetHwnd());
	// TODO: At the moment, the index of the font descriptor is hard-coded
	const CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(m_SRVHeap->GetCPUDescriptorHandleForHeapStart(), 2, m_SRVDescriptorSize);
	const CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(m_SRVHeap->GetGPUDescriptorHandleForHeapStart(), 2, m_SRVDescriptorSize);
	ImGui_ImplDX12_Init(m_Device.Get(), s_FrameCount, DXGI_FORMAT_R8G8B8A8_UNORM,
		m_SRVHeap.Get(), cpuHandle, gpuHandle);
}

void D3D12Application::LoadAssets()
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
			{ { 0.0f, 0.25f * m_AspectRatio, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
			{ { 0.25f, -0.25f * m_AspectRatio, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
			{ { -0.25f, -0.25f * m_AspectRatio, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
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

	// Create the constant buffers
	// One constant buffer is required for each frame, so that it can be read from while the next frame writes the new data
	{
		constexpr UINT constantBufferSize = sizeof(ConstantBufferType);
		static_assert(constantBufferSize % 256 == 0); // must be 256-byte aligned

		// Define heap properties and resource description for the constant buffers
		CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
		auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(static_cast<UINT64>(constantBufferSize * s_FrameCount));

		// Create the buffer
		THROW_IF_FAIL(m_Device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_ConstantBuffer)));

		// Create the buffer views
		for (UINT n = 0; n < s_FrameCount; n++)
		{
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
			cbvDesc.BufferLocation = m_ConstantBuffer->GetGPUVirtualAddress() + constantBufferSize * n;
			cbvDesc.SizeInBytes = constantBufferSize;

			CD3DX12_CPU_DESCRIPTOR_HANDLE descriptor(m_SRVHeap->GetCPUDescriptorHandleForHeapStart(), n, m_SRVDescriptorSize);

			m_Device->CreateConstantBufferView(&cbvDesc, descriptor);
		}

		// Map and initialize the constant buffer
		// This buffer is kept mapped for its entire lifetime
		CD3DX12_RANGE readRange(0, 0); // We do not intend on reading from this resource on the CPU
		THROW_IF_FAIL(m_ConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_ConstantBufferMappedAddress)));
		for (UINT n = 0; n < s_FrameCount; n++)
			memcpy(m_ConstantBufferMappedAddress + static_cast<size_t>(n * constantBufferSize), &m_ConstantBufferData, sizeof(m_ConstantBufferData));
	}

	// Close the command list and execute it to begin the initial GPU setup
	THROW_IF_FAIL(m_CommandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
	m_CommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Create synchronization objects
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

		// Wait for the command list to execute
		WaitForGPU();
	}
}


void D3D12Application::OnUpdate()
{
	// Begin new ImGui frame
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Properties");

	ImGui::SliderFloat3("Color multiplier", &m_ConstantBufferData.colorMultiplier.x, 0.0f, 1.0f);

	ImGui::End();

	// copy our const buffer data into the const buffer
	memcpy(m_ConstantBufferMappedAddress + m_FrameIndex * sizeof(m_ConstantBufferData), &m_ConstantBufferData, sizeof(m_ConstantBufferData));

	ImGui::Render();
}

void D3D12Application::OnRender()
{
	// Record all the commands we need to render the scene into the command list
	PopulateCommandList();

	// Execute the command list
	ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
	m_CommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	const ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault(nullptr, m_CommandList.Get());
	}

	// Present the frame
	THROW_IF_FAIL(m_SwapChain->Present(1, 0));

	MoveToNextFrame();
}

void D3D12Application::OnDestroy()
{
	// Ensure the GPU is no longer references resources that are about
	// to be cleaned up by the destructor
	WaitForGPU();

	// Cleanup ImGui
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	CloseHandle(m_FenceEvent);
}

void D3D12Application::PopulateCommandList() const
{
	// Command list allocators can only be reset when the associated
	// command lists have finished execution on the GPU
	THROW_IF_FAIL(m_FrameResources[m_FrameIndex].CommandAllocator->Reset());

	// Command lists can (and must) be reset after ExecuteCommandList() is called and before it is repopulated
	THROW_IF_FAIL(m_CommandList->Reset(m_FrameResources[m_FrameIndex].CommandAllocator.Get(), m_PipelineState.Get()));

	

	// Indicate the back buffer will be used as a render target
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_RenderTargets[m_FrameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_CommandList->ResourceBarrier(1, &barrier);

	const CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_RTVHeap->GetCPUDescriptorHandleForHeapStart(), m_FrameIndex, m_RTVDescriptorSize);
	m_CommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	// Clear render target
	constexpr float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	m_CommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);


	// Set necessary state
	m_CommandList->SetGraphicsRootSignature(m_RootSignature.Get());

	// Setup descriptor heaps
	ID3D12DescriptorHeap* ppHeaps[] = { m_SRVHeap.Get() };
	m_CommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	// Get gpu virtual address of constant buffer contents to use for rendering
	const CD3DX12_GPU_DESCRIPTOR_HANDLE cbvHandle(m_SRVHeap->GetGPUDescriptorHandleForHeapStart(), m_FrameIndex, m_SRVDescriptorSize);
	m_CommandList->SetGraphicsRootDescriptorTable(0, cbvHandle);

	m_CommandList->RSSetViewports(1, &m_Viewport);
	m_CommandList->RSSetScissorRects(1, &m_ScissorRect);

	// render the triangle
	m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_CommandList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
	m_CommandList->IASetIndexBuffer(&m_IndexBufferView);
	m_CommandList->DrawIndexedInstanced(3, 1, 0, 0, 0);

	// ImGui Render
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_CommandList.Get());

	// Indicate that the back buffer will now be used to present
	barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_RenderTargets[m_FrameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	m_CommandList->ResourceBarrier(1, &barrier);

	THROW_IF_FAIL(m_CommandList->Close());
}

void D3D12Application::WaitForGPU()
{
	// Signal and increment the fence
	THROW_IF_FAIL(m_CommandQueue->Signal(m_Fence.Get(), m_FrameResources[m_FrameIndex].FenceValue));

	THROW_IF_FAIL(m_Fence->SetEventOnCompletion(m_FrameResources[m_FrameIndex].FenceValue, m_FenceEvent));
	WaitForSingleObject(m_FenceEvent, INFINITE);

	m_FrameResources[m_FrameIndex].FenceValue++;
}

void D3D12Application::MoveToNextFrame()
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

void D3D12Application::OnResized()
{
	assert(false && "Not working");

	// Flush all work on GPU as the swap chain will be resized
	WaitForGPU();

	// Reset command list
	THROW_IF_FAIL(m_CommandList->Reset(m_DirectCommandAllocator.Get(), nullptr));

	// Release all resources that are about to be recreated
	for (int n = 0; n < s_FrameCount; n++)
	{
		m_RenderTargets[n].Reset();
	}

	// Resize the swap chain
	THROW_IF_FAIL(m_SwapChain->ResizeBuffers(s_FrameCount, GetWidth(), GetHeight(), DXGI_FORMAT_R8G8B8A8_UNORM, 0));

	m_FrameIndex = 0;

	// Create render target views
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_RTVHeap->GetCPUDescriptorHandleForHeapStart());

	// Create an RTV for each frame
	for (UINT n = 0; n < s_FrameCount; n++)
	{
		THROW_IF_FAIL(m_SwapChain->GetBuffer(n, IID_PPV_ARGS(&m_RenderTargets[n])));
		m_Device->CreateRenderTargetView(m_RenderTargets[n].Get(), nullptr, rtvHandle);
		rtvHandle.Offset(1, m_RTVDescriptorSize);
	}

	// Flush commands
	THROW_IF_FAIL(m_CommandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
	m_CommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	WaitForGPU();

	// Set viewport and scissor
	m_Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(GetWidth()), static_cast<float>(GetHeight()));
	m_ScissorRect = CD3DX12_RECT(0, 0, GetWidth(), GetHeight());
}
