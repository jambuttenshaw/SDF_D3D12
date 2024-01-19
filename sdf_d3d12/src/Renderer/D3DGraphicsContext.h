#pragma once

#include "Memory/MemoryAllocator.h"

#include "D3DPipeline.h"
#include "Hlsl/RaytracingHlslCompat.h"


using Microsoft::WRL::ComPtr;
using namespace DirectX;

class RenderItem;
class GameTimer;
class Camera;

class Scene;

class D3DFrameResources;

// Make the graphics context globally accessible
class D3DGraphicsContext;
extern D3DGraphicsContext* g_D3DGraphicsContext;


class D3DGraphicsContext
{
public:
	D3DGraphicsContext(HWND window, UINT width, UINT height);
	~D3DGraphicsContext();

	// Disable copying & moving
	DISALLOW_COPY(D3DGraphicsContext)
	DISALLOW_MOVE(D3DGraphicsContext)


	void Present();

	void StartDraw() const;
	void EndDraw() const;

	void CopyRaytracingOutput(D3D12_GPU_DESCRIPTOR_HANDLE rtOutputHandle) const;

	// Updating constant buffers
	void UpdatePassCB(GameTimer* timer, Camera* camera, UINT flags);
	D3D12_GPU_VIRTUAL_ADDRESS GetPassCBAddress() const;

	void Resize(UINT width, UINT height);

	void Flush() const;
	void WaitForGPU() const;

public:
	// Getters

	inline float GetAspectRatio() const { return static_cast<float>(m_ClientWidth) / static_cast<float>(m_ClientHeight); }

	inline static UINT GetBackBufferCount() { return s_FrameCount; }
	inline DXGI_FORMAT GetBackBufferFormat() const { return m_BackBufferFormat; }
	inline DXGI_FORMAT GetDepthStencilFormat() const { return m_DepthStencilFormat; }
	inline UINT GetCurrentBackBuffer() const { return m_FrameIndex; }

	inline UINT GetClientWidth() const { return m_ClientWidth; }
	inline UINT GetClientHeight() const { return m_ClientHeight; }

	// Descriptor heaps
	inline DescriptorHeap* GetSRVHeap() const { return m_SRVHeap.get(); }

	// For ImGui
	inline ID3D12DescriptorHeap* GetImGuiResourcesHeap() const { return m_ImGuiResources.GetHeap(); }
	inline D3D12_CPU_DESCRIPTOR_HANDLE GetImGuiCPUDescriptor() const { return m_ImGuiResources.GetCPUHandle(); }
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetImGuiGPUDescriptor() const { return m_ImGuiResources.GetGPUHandle(); }

	// D3D objects
	inline ID3D12Device* GetDevice() const { return m_Device.Get(); }
	inline ID3D12GraphicsCommandList* GetCommandList() const { return m_CommandList.Get(); }


	// DXR objects
	inline ID3D12Device5* GetDXRDevice() const { return m_DXRDevice.Get(); }
	inline ID3D12GraphicsCommandList4* GetDXRCommandList() const { return m_DXRCommandList.Get(); }

private:
	// Startup
	void CreateAdapter();
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

	// Raytracing
	bool CheckRaytracingSupport() const;
	void CreateRaytracingInterfaces();

	void CreateProjectionMatrix();

	void MoveToNextFrame();
	void ProcessDeferrals(UINT frameIndex) const;
	// NOTE: this is only safe to do so when ALL WORK HAS BEEN COMPLETED ON THE GPU!!!
	void ProcessAllDeferrals() const;

private:
	static constexpr UINT s_FrameCount = 2;

	// Context properties
	HWND m_WindowHandle;
	UINT m_ClientWidth;
	UINT m_ClientHeight;

	// Projection properties
	float m_FOV = 0.25f * XM_PI;
	float m_NearPlane = 0.1f;
	float m_FarPlane = 100.0f;
	XMMATRIX m_ProjectionMatrix;

	// Formats
	DXGI_FORMAT m_BackBufferFormat;
	DXGI_FORMAT m_DepthStencilFormat;

	// Pipeline objects
	ComPtr<IDXGIAdapter1> m_Adapter;
	ComPtr<IDXGIFactory6> m_Factory;
	ComPtr<ID3D12Device> m_Device;

	ComPtr<IDXGISwapChain3> m_SwapChain;
	ComPtr<ID3D12Resource> m_RenderTargets[s_FrameCount];
	DescriptorAllocation m_RTVs;

	ComPtr<ID3D12Resource> m_DepthStencilBuffer;
	DescriptorAllocation m_DSV;

	ComPtr<ID3D12CommandAllocator> m_DirectCommandAllocator;	// Used for non-frame-specific allocations (startup, resize swap chain, etc)

	ComPtr<ID3D12CommandQueue> m_CommandQueue;
	ComPtr<ID3D12GraphicsCommandList> m_CommandList;

	// Frame resources
	std::vector<std::unique_ptr<D3DFrameResources>> m_FrameResources;
	D3DFrameResources* m_CurrentFrameResources = nullptr;

	// Pass CB Data
	PassConstantBuffer m_MainPassCB;

	// Descriptor heaps
	std::unique_ptr<DescriptorHeap> m_RTVHeap;
	std::unique_ptr<DescriptorHeap> m_DSVHeap;
	std::unique_ptr<DescriptorHeap> m_SRVHeap;				// SRV, UAV, and CBV heap (named SRVHeap for brevity)
	std::unique_ptr<DescriptorHeap> m_SamplerHeap;

	// Synchronization objects
	UINT m_FrameIndex = 0;
	HANDLE m_FenceEvent = nullptr;
	ComPtr<ID3D12Fence> m_Fence;

	CD3DX12_VIEWPORT m_Viewport{ };
	CD3DX12_RECT m_ScissorRect{ };


	// DirectX Raytracing (DXR) objects
	ComPtr<ID3D12Device5> m_DXRDevice;
	ComPtr<ID3D12GraphicsCommandList4> m_DXRCommandList;
	

	// Pipeline assets
	std::unique_ptr<D3DGraphicsPipeline> m_GraphicsPipeline;	// A raster pass is used to copy the raytracing output to the back buffer

	// ImGui Resources
	DescriptorAllocation m_ImGuiResources;
};
