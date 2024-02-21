#pragma once

#include "Core.h"
#include "Memory/MemoryAllocator.h"

#include "D3DQueue.h"
#include "HlslCompat/RaytracingHlslCompat.h"


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
	// Returns true if device is removed
	bool CheckDeviceRemovedStatus() const;

	void StartDraw() const;
	void EndDraw() const;

	void CopyRaytracingOutput(ID3D12Resource* raytracingOutput) const;

	// Updating constant buffers
	void UpdatePassCB(GameTimer* timer, Camera* camera, UINT flags, UINT heatmapQuantization, float heatmapHueRange);
	D3D12_GPU_VIRTUAL_ADDRESS GetPassCBAddress() const;

	void DeferRelease(const ComPtr<IUnknown>& resource) const;

	void Resize(UINT width, UINT height);

	// Wait for all queues to become idle
	void WaitForGPUIdle() const;

	// Vsync
	inline bool GetVSyncEnabled() const { return m_VSyncEnabled; }
	inline void SetVSyncEnabled(bool enabled) { m_VSyncEnabled = enabled; }

public:
	// Getters

	inline float GetAspectRatio() const { return static_cast<float>(m_ClientWidth) / static_cast<float>(m_ClientHeight); }

	inline static constexpr UINT GetBackBufferCount() { return s_FrameCount; }
	inline DXGI_FORMAT GetBackBufferFormat() const { return m_BackBufferFormat; }
	inline UINT GetCurrentBackBuffer() const { return m_FrameIndex; }

	inline UINT GetClientWidth() const { return m_ClientWidth; }
	inline UINT GetClientHeight() const { return m_ClientHeight; }

	// Descriptor heaps
	inline DescriptorHeap* GetSRVHeap() const { return m_SRVHeap.get(); }
	inline DescriptorHeap* GetSamplerHeap() const { return m_SamplerHeap.get(); }

	// For ImGui
	inline ID3D12DescriptorHeap* GetImGuiResourcesHeap() const { return m_ImGuiResources.GetHeap(); }
	inline D3D12_CPU_DESCRIPTOR_HANDLE GetImGuiCPUDescriptor() const { return m_ImGuiResources.GetCPUHandle(); }
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetImGuiGPUDescriptor() const { return m_ImGuiResources.GetGPUHandle(); }

	// DXGI objects
	inline bool GetTearingSupport() const { return m_TearingSupport; }
	inline IDXGISwapChain* GetSwapChain() const { return m_SwapChain.Get(); }

	// D3D objects
	inline ID3D12Device* GetDevice() const { return m_Device.Get(); }
	inline ID3D12GraphicsCommandList* GetCommandList() const { return m_CommandList.Get(); }

	// Queues
	inline D3DQueue* GetDirectCommandQueue() const { return m_DirectQueue.get(); }
	inline D3DQueue* GetComputeCommandQueue() const { return m_ComputeQueue.get(); }

	// DXR objects
	inline ID3D12Device5* GetDXRDevice() const { return m_DXRDevice.Get(); }
	inline ID3D12GraphicsCommandList4* GetDXRCommandList() const { return m_DXRCommandList.Get(); }

private:
	// Startup
	void CreateAdapter();
	void CreateDevice();
	void CreateCommandQueues();
	void CreateSwapChain();
	void CreateDescriptorHeaps();
	void CreateCommandAllocator();
	void CreateCommandList();
	void CreateRTVs();
	void CreateFrameResources();

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

	bool m_WindowedMode = true;
	bool m_TearingSupport = false;
	bool m_VSyncEnabled = false;

	// Projection properties
	float m_FOV = 0.25f * XM_PI;
	float m_NearPlane = 0.1f;
	float m_FarPlane = 100.0f;
	XMMATRIX m_ProjectionMatrix;

	// Formats
	DXGI_FORMAT m_BackBufferFormat;

	// Pipeline objects
	ComPtr<IDXGIAdapter1> m_Adapter;
	ComPtr<IDXGIFactory6> m_Factory;
	ComPtr<ID3D12Device> m_Device;

	// Device debug objects
	inline static constexpr bool s_EnableDRED = true;
	ComPtr<ID3D12InfoQueue1> m_InfoQueue;
	DWORD m_MessageCallbackCookie;

	inline static constexpr bool s_EnablePIXCaptures = false;
	HMODULE m_PIXCaptureModule = nullptr;

	ComPtr<IDXGISwapChain3> m_SwapChain;
	ComPtr<ID3D12Resource> m_RenderTargets[s_FrameCount];
	DescriptorAllocation m_RTVs;

	// Command queues
	std::unique_ptr<D3DQueue> m_DirectQueue;
	std::unique_ptr<D3DQueue> m_ComputeQueue;

	ComPtr<ID3D12CommandAllocator> m_DirectCommandAllocator;	// Used for non-frame-specific allocations (startup, resize swap chain, etc)
	ComPtr<ID3D12GraphicsCommandList> m_CommandList;

	// Frame resources
	UINT m_FrameIndex = 0;
	std::vector<std::unique_ptr<D3DFrameResources>> m_FrameResources;
	D3DFrameResources* m_CurrentFrameResources = nullptr;

	// Pass CB Data
	PassConstantBuffer m_MainPassCB;

	// Descriptor heaps
	std::unique_ptr<DescriptorHeap> m_RTVHeap;
	std::unique_ptr<DescriptorHeap> m_SRVHeap;				// SRV, UAV, and CBV heap (named SRVHeap for brevity)
	std::unique_ptr<DescriptorHeap> m_SamplerHeap;

	// DirectX Raytracing (DXR) objects
	ComPtr<ID3D12Device5> m_DXRDevice;
	ComPtr<ID3D12GraphicsCommandList4> m_DXRCommandList;
	
	// ImGui Resources
	DescriptorAllocation m_ImGuiResources;
};
