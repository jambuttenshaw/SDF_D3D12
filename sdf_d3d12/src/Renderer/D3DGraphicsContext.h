#pragma once

#include "Memory/D3DMemoryAllocator.h"
#include "D3DCBTypes.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

class RenderItem;
class D3DFrameResources;

class D3DGraphicsContext;
extern D3DGraphicsContext* g_D3DGraphicsContext;


class D3DGraphicsContext
{
public:
	struct Vertex
	{
		XMFLOAT3 position;
		XMFLOAT4 color;
	};

public:
	D3DGraphicsContext(HWND window, UINT width, UINT height);
	~D3DGraphicsContext();

	// Disable copying
	D3DGraphicsContext(const D3DGraphicsContext&) = delete;
	D3DGraphicsContext& operator= (const D3DGraphicsContext&) = delete;

	// Disable moving
	D3DGraphicsContext(D3DGraphicsContext&&) = delete;
	D3DGraphicsContext& operator= (D3DGraphicsContext&&) = delete;


	void Present();

	void StartDraw() const;
	void EndDraw() const;

	// Temporary, eventually commands will be submitted by the application
	void DrawItems() const;

	// Render Items
	inline UINT GetNextObjectIndex() { ASSERT(m_NextRenderItemIndex < s_MaxObjectCount, "Too many render items!"); return m_NextRenderItemIndex++; }

	// Updating constant buffers
	void UpdateObjectCBs() const;
	void UpdatePassCB();

	void Flush() const;
	void WaitForGPU() const;

public:
	// Getters

	inline float GetAspectRatio() const { return static_cast<float>(m_ClientWidth) / static_cast<float>(m_ClientHeight); }

	inline static UINT GetBackBufferCount() { return s_FrameCount; }
	inline DXGI_FORMAT GetBackBufferFormat() const { return m_BackBufferFormat; }
	inline UINT GetCurrentBackBuffer() const { return m_FrameIndex; }

	inline static UINT GetMaxObjectCount() { return s_MaxObjectCount; }

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
	void CreateCommandList();
	void CreateRTVs();
	void CreateDepthStencilBuffer();
	void CreateFrameResources();

	void CreateViewport();
	void CreateScissorRect();

	void CreateFence();

	void CreateProjectionMatrix();

	// Temporary, eventually buffers and their data will be created elsewhere
	void CreateAssets();


	void MoveToNextFrame();
	void ProcessDeferrals(UINT frameIndex) const;

private:
	static constexpr UINT s_FrameCount = 2;
	static constexpr UINT s_MaxObjectCount = 2;

	// Context properties
	HWND m_WindowHandle;
	UINT m_ClientWidth;
	UINT m_ClientHeight;

	// Projection properties
	float m_FOV = 0.25f * XM_PI;
	float m_NearPlane = 0.1f;
	float m_FarPlane = 100.0f;
	XMMATRIX m_ProjectionMatrix;

	// View Info
	XMFLOAT3 m_EyePos{ 0.0f, 1.0f, -5.0f };
	XMFLOAT3 m_EyeDirection{ 0.0f, 0.0f, 1.0f };
	XMFLOAT3 m_EyeUp{ 0.0f, 1.0f, 0.0f };

	// Formats
	DXGI_FORMAT m_BackBufferFormat;
	DXGI_FORMAT m_DepthStencilFormat;

	// Pipeline objects
	ComPtr<IDXGIFactory6> m_Factory;
	ComPtr<ID3D12Device> m_Device;

	ComPtr<IDXGISwapChain3> m_SwapChain;
	ComPtr<ID3D12Resource> m_RenderTargets[s_FrameCount];
	D3DDescriptorAllocation m_RTVs;

	ComPtr<ID3D12Resource> m_DepthStencilBuffer;
	D3DDescriptorAllocation m_DSV;

	ComPtr<ID3D12CommandAllocator> m_DirectCommandAllocator;	// Used for non-frame-specific allocations (startup, resize swap chain, etc)
	ComPtr<ID3D12CommandQueue> m_CommandQueue;
	ComPtr<ID3D12GraphicsCommandList> m_CommandList;

	// Frame resources
	std::vector<std::unique_ptr<D3DFrameResources>> m_FrameResources;
	D3DFrameResources* m_CurrentFrameResources = nullptr;

	// Pass CB Data
	PassCBType m_MainPassCB;

	// Descriptor heaps
	std::unique_ptr<D3DDescriptorHeap> m_RTVHeap;
	std::unique_ptr<D3DDescriptorHeap> m_DSVHeap;
	std::unique_ptr<D3DDescriptorHeap> m_SRVHeap;				// SRV, UAV, and CBV heap (named SRVHeap for brevity)

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

	// Render Items
	UINT m_NextRenderItemIndex = 0;
	std::vector<RenderItem> m_RenderItems;

	// Temporary
	// TODO: Implement a robust and re-usable deferred release system
	std::vector<ComPtr<IUnknown>> m_DeferredReleases;
};
