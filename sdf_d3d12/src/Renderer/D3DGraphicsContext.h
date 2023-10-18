#pragma once

using Microsoft::WRL::ComPtr;
using namespace DirectX;


class D3DGraphicsContext
{
public:
	D3DGraphicsContext(HWND window, UINT width, UINT height);
	~D3DGraphicsContext();

	void Present();

	void StartDraw() const;
	void EndDraw() const;

	// Temporary, eventually commands will be submitted by the application
	void PopulateCommandList() const;

	void Flush();
	void WaitForGPU();
	void MoveToNextFrame();

public:
	// Getters

	inline float GetAspectRatio() const { return static_cast<float>(m_ClientWidth) / static_cast<float>(m_ClientHeight); }

private:
	// Startup
	void CreateDevice();
	void CreateCommandQueue();
	void CreateSwapChain();
	void CreateDescriptorHeaps();
	void CreateCommandAllocator();
	void CreateRenderTargetViews();
	void CreateFrameResources();

	void CreateViewport();
	void CreateScissorRect();

	void CreateFence();
	void LoadImGui() const;

	// Temporary, eventually buffers and their data will be created elsewhere
	void CreateAssets();

private:
	static constexpr UINT s_FrameCount = 2;

	struct FrameResources
	{
		ComPtr<ID3D12CommandAllocator> CommandAllocator;
		UINT64 FenceValue = 0;
	};

	struct Vertex
	{
		XMFLOAT3 position;
		XMFLOAT4 color;
	};

	struct ConstantBufferType
	{
		XMFLOAT4 colorMultiplier{ 1.0f, 1.0f, 1.0f, 1.0f };
		float padding[60];
	};

	// Context properties
	HWND m_WindowHandle;
	UINT m_ClientWidth;
	UINT m_ClientHeight;

	// Formats
	DXGI_FORMAT m_BackBufferFormat;

	// Pipeline objects
	ComPtr<IDXGIFactory6> m_Factory;
	ComPtr<ID3D12Device> m_Device;

	ComPtr<IDXGISwapChain3> m_SwapChain;
	ComPtr<ID3D12Resource> m_RenderTargets[s_FrameCount];

	ComPtr<ID3D12CommandAllocator> m_DirectCommandAllocator;
	ComPtr<ID3D12CommandQueue> m_CommandQueue;
	ComPtr<ID3D12GraphicsCommandList> m_CommandList;

	// Frame resources
	FrameResources m_FrameResources[s_FrameCount];

	// Descriptor heaps
	ComPtr<ID3D12DescriptorHeap> m_RTVHeap;
	UINT m_RTVDescriptorSize = 0;
	ComPtr<ID3D12DescriptorHeap> m_SRVHeap;
	UINT m_SRVDescriptorSize = 0;

	// Synchronization objects
	UINT m_FrameIndex = 0;
	HANDLE m_FenceEvent = nullptr;
	ComPtr<ID3D12Fence> m_Fence;

	CD3DX12_VIEWPORT m_Viewport{ };
	CD3DX12_RECT m_ScissorRect{ };

	// Pipeline assets
	ComPtr<ID3D12RootSignature> m_RootSignature;
	ComPtr<ID3D12PipelineState> m_PipelineState;

	// App resources
	ComPtr<ID3D12Resource> m_VertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView = {};

	ComPtr<ID3D12Resource> m_IndexBuffer;
	D3D12_INDEX_BUFFER_VIEW m_IndexBufferView = {};

	// Constant buffer:
	// The memory on the GPU in which the constant buffer data is kept
	ComPtr<ID3D12Resource> m_ConstantBuffer;
	// This is the current constant buffer data that will be copied to the GPU each frame
	ConstantBufferType m_ConstantBufferData = {};
	// This is the GPU virtual address for each constant buffer resource on the GPU
	UINT8* m_ConstantBufferMappedAddress = nullptr;
	// Indices into the descriptor heap for the CBV for each frame
	UINT32 m_CBVs[s_FrameCount] = { 0, 0 };

	inline static D3DGraphicsContext* g_D3DGraphicsContext = nullptr;
};
