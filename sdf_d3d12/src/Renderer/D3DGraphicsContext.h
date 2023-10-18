#pragma once

#include "Memory/D3DMemoryAllocator.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

class D3DGraphicsContext;
extern D3DGraphicsContext* g_D3DGraphicsContext;


class D3DGraphicsContext
{
public:            
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

public:
	D3DGraphicsContext(HWND window, UINT width, UINT height);
	~D3DGraphicsContext();

	void Present();

	void StartDraw() const;
	void EndDraw() const;

	// Temporary, eventually commands will be submitted by the application
	void PopulateCommandList(D3D12_GPU_DESCRIPTOR_HANDLE cbv) const;

	void Flush();
	void WaitForGPU();

public:
	// Getters

	inline float GetAspectRatio() const { return static_cast<float>(m_ClientWidth) / static_cast<float>(m_ClientHeight); }

	inline static UINT GetBackBufferCount() { return s_FrameCount; }
	inline DXGI_FORMAT GetBackBufferFormat() const { return m_BackBufferFormat; }
	inline UINT GetCurrentBackBuffer() const { return m_FrameIndex; }

	// Descriptor heaps
	inline D3DDescriptorHeap* GetSRVHeap() const { return m_SRVHeap.get(); }

	// For ImGui
	inline ID3D12DescriptorHeap* GetImGuiResourcesHeap() const { return m_ImGuiResources.GetHeap(); }
	inline D3D12_CPU_DESCRIPTOR_HANDLE GetImGuiCPUDescriptor() const { return m_ImGuiResources.GetCPUHandle(); }
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetImGuiGPUDescriptor() const { return m_ImGuiResources.GetGPUHandle(); }

	// D3D objects
	inline ID3D12Device* GetDevice() const { return m_Device.Get(); }
	inline ID3D12GraphicsCommandList* GetCommandList() const { return m_CommandList.Get(); }

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

	// Temporary, eventually buffers and their data will be created elsewhere
	void CreateAssets();


	void MoveToNextFrame();
	void ProcessDeferrals(UINT frameIndex) const;

private:
	static constexpr UINT s_FrameCount = 2;

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
	D3DDescriptorAllocation m_RTVs;

	ComPtr<ID3D12CommandAllocator> m_DirectCommandAllocator;
	ComPtr<ID3D12CommandQueue> m_CommandQueue;
	ComPtr<ID3D12GraphicsCommandList> m_CommandList;

	// Frame resources
	FrameResources m_FrameResources[s_FrameCount];

	// Descriptor heaps
	std::unique_ptr<D3DDescriptorHeap> m_RTVHeap;
	std::unique_ptr<D3DDescriptorHeap> m_SRVHeap;

	// Synchronization objects
	UINT m_FrameIndex = 0;
	HANDLE m_FenceEvent = nullptr;
	ComPtr<ID3D12Fence> m_Fence;

	CD3DX12_VIEWPORT m_Viewport{ };
	CD3DX12_RECT m_ScissorRect{ };

	// Pipeline assets
	ComPtr<ID3D12RootSignature> m_RootSignature;
	ComPtr<ID3D12PipelineState> m_PipelineState;

	// ImGui Resources
	D3DDescriptorAllocation m_ImGuiResources;

	// App resources
	ComPtr<ID3D12Resource> m_VertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView = {};

	ComPtr<ID3D12Resource> m_IndexBuffer;
	D3D12_INDEX_BUFFER_VIEW m_IndexBufferView = {};
};