#pragma once

#include "D3DBuffer.h"
#include "Memory/D3DMemoryAllocator.h"
#include "D3DCBTypes.h"

#include "D3DPipeline.h"
#include "D3DShaderTable.h"


using Microsoft::WRL::ComPtr;
using namespace DirectX;

class RenderItem;
class GameTimer;
class Camera;

class D3DFrameResources;

class D3DGraphicsContext;
extern D3DGraphicsContext* g_D3DGraphicsContext;


namespace GlobalRootSignatureParams
{
	enum Value
	{
		OutputViewSlot = 0,
		AccelerationStructureSlot,
		Count
	};
}

namespace LocalRootSignatureParams
{
	enum Value
	{
		RayGenConstantSlot = 0,
		Count
	};
}


class D3DGraphicsContext
{
public:
	D3DGraphicsContext(HWND window, UINT width, UINT height);
	~D3DGraphicsContext();

	// Disable copying & moving
	D3DGraphicsContext(const D3DGraphicsContext&) = delete;
	D3DGraphicsContext& operator= (const D3DGraphicsContext&) = delete;
	D3DGraphicsContext(D3DGraphicsContext&&) = delete;
	D3DGraphicsContext& operator= (D3DGraphicsContext&&) = delete;


	void Present();

	void StartDraw() const;
	void EndDraw() const;

	void DrawItems(D3DComputePipeline* pipeline) const;
	void DrawVolume(D3DComputePipeline* pipeline, D3D12_GPU_DESCRIPTOR_HANDLE volumeSRV) const;
	void DrawRaytracing() const;

	// Render Items

	inline UINT GetNextObjectIndex() { ASSERT(m_NextRenderItemIndex < s_MaxObjectCount, "Too many render items!"); return m_NextRenderItemIndex++; }
	RenderItem* CreateRenderItem();

	// Updating constant buffers
	void UpdateObjectCBs() const;
	void UpdatePassCB(GameTimer* timer, Camera* camera, const RayMarchPropertiesType& rayMarchProperties);

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

	// DXR
	void CreateRaytracingInterface();
	void CreateRaytracingRootSignatures();
	void CreateRaytracingPipelineStateObject();
	void CreateRaytracingOutputTexture();

	void BuildGeometry();
	void BuildAccelerationStructures();
	void BuildShaderTables();

	// Raytracing helper functions
	bool CheckRaytracingSupport() const;
	void SerializeAndCreateRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* rootSig) const;

	void CreateViewport();
	void CreateScissorRect();

	void CreateFence();

	void CreateProjectionMatrix();

	void CreateAssets();		// Temporary - eventually all assets will be created elsewhere


	void MoveToNextFrame();
	void ProcessDeferrals(UINT frameIndex) const;
	// NOTE: this is only safe to do so when ALL WORK HAS BEEN COMPLETED ON THE GPU!!!
	void ProcessAllDeferrals() const;

private:
	static constexpr UINT s_FrameCount = 2;
	static constexpr UINT s_MaxObjectCount = 16;
	static constexpr UINT s_NumShaderThreads = 8;

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
	D3DDescriptorAllocation m_RTVs;

	ComPtr<ID3D12Resource> m_DepthStencilBuffer;
	D3DDescriptorAllocation m_DSV;

	ComPtr<ID3D12CommandAllocator> m_DirectCommandAllocator;	// Used for non-frame-specific allocations (startup, resize swap chain, etc)

	ComPtr<ID3D12CommandQueue> m_CommandQueue;
	ComPtr<ID3D12GraphicsCommandList> m_CommandList;


	// Raytracing pipeline objects
	ComPtr<ID3D12Device5> m_DXRDevice;
	ComPtr<ID3D12GraphicsCommandList4> m_DXRCommandList;
	ComPtr<ID3D12StateObject> m_DXRStateObject;

	// Root signatures
	ComPtr<ID3D12RootSignature> m_RayTracingGlobalRootSignature;

	// Geometry
	typedef UINT16 Index;
	struct Vertex { float v1, v2, v3; };
	ComPtr<ID3D12Resource> m_IndexBuffer;
	ComPtr<ID3D12Resource> m_VertexBuffer;

	// Acceleration structure
	ComPtr<ID3D12Resource> m_AccelerationStructure;
	ComPtr<ID3D12Resource> m_BottomLevelAccelerationStructure;
	ComPtr<ID3D12Resource> m_TopLevelAccelerationStructure;

	// Shader tables
	inline static const wchar_t* c_HitGroupName = L"HitGroup";
	inline static const wchar_t* c_RaygenShaderName = L"RaygenShader";
	inline static const wchar_t* c_ClosestHitShaderName = L"ClosestHitShader";
	inline static const wchar_t* c_MissShaderName = L"MissShader";
	std::unique_ptr<D3DShaderTable> m_RayGenShaderTable;
	std::unique_ptr<D3DShaderTable> m_HitGroupShaderTable;
	std::unique_ptr<D3DShaderTable> m_MissShaderTable;


	// Frame resources
	std::vector<std::unique_ptr<D3DFrameResources>> m_FrameResources;
	D3DFrameResources* m_CurrentFrameResources = nullptr;

	// Pass CB Data
	PassCBType m_MainPassCB;

	// Descriptor heaps
	std::unique_ptr<D3DDescriptorHeap> m_RTVHeap;
	std::unique_ptr<D3DDescriptorHeap> m_DSVHeap;
	std::unique_ptr<D3DDescriptorHeap> m_SRVHeap;				// SRV, UAV, and CBV heap (named SRVHeap for brevity)
	std::unique_ptr<D3DDescriptorHeap> m_SamplerHeap;

	// Synchronization objects
	UINT m_FrameIndex = 0;
	HANDLE m_FenceEvent = nullptr;
	ComPtr<ID3D12Fence> m_Fence;

	CD3DX12_VIEWPORT m_Viewport{ };
	CD3DX12_RECT m_ScissorRect{ };

	// Pipeline assets
	std::unique_ptr<D3DGraphicsPipeline> m_GraphicsPipeline;

	// ImGui Resources
	D3DDescriptorAllocation m_ImGuiResources;

	// Application resources
	ComPtr<ID3D12Resource> m_RayTracingOutputTexture;				// The texture to output raytracing to
	D3DDescriptorAllocation m_RayTracingOutputTextureViews;			// UAV and SRV


	// Render Items
	UINT m_NextRenderItemIndex = 0;
	std::vector<RenderItem> m_RenderItems;
};
