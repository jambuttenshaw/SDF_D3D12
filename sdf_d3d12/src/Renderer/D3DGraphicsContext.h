#pragma once

#include "Memory/D3DMemoryAllocator.h"

#include "D3DBuffer.h"
#include "D3DPipeline.h"
#include "D3DShaderTable.h"
#include "Hlsl/RaytracingHlslCompat.h"


using Microsoft::WRL::ComPtr;
using namespace DirectX;

class RenderItem;
class GameTimer;
class Camera;

class D3DFrameResources;

class D3DGraphicsContext;
extern D3DGraphicsContext* g_D3DGraphicsContext;


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

	void DrawRaytracing() const;

	// Updating constant buffers
	void UpdatePassCB(GameTimer* timer, Camera* camera);
	void UpdateAABBPrimitiveAttributes();

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


	// Descriptor heaps
	inline D3DDescriptorHeap* GetSRVHeap() const { return m_SRVHeap.get(); }

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


public:

	// Scene building
	// Construct the objects in the scene before building the acceleration structure and the shader table

	void AddObjectToScene(class SDFObject* object);

	void BuildGeometry();
	void BuildAccelerationStructures();
	void BuildShaderTables();


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
	void SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* rootSig) const;
	void CreateRootSignatures();
	void CreateRaytracingPipelineStateObject();
	void CreateRaytracingOutputResource();

	void CreateProjectionMatrix();

	void CreateAssets();		// Temporary - eventually all assets will be created elsewhere


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
	PassConstantBuffer m_MainPassCB;

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

	

	// DirectX Raytracing (DXR) attributes
	ComPtr<ID3D12Device5> m_DXRDevice;
	ComPtr<ID3D12GraphicsCommandList4> m_DXRCommandList;
	ComPtr<ID3D12StateObject> m_DXRStateObject;

	// Root signatures
	ComPtr<ID3D12RootSignature> m_RaytracingGlobalRootSignature;
	ComPtr<ID3D12RootSignature> m_RaytracingLocalRootSignature;

	// Geometry
	std::vector<D3D12_RAYTRACING_AABB> m_AABBs;
	std::unique_ptr<D3DUploadBuffer<D3D12_RAYTRACING_AABB>> m_AABBBuffer;

	// Acceleration structure
	std::unique_ptr<D3DUAVBuffer> m_BottomLevelAccelerationStructure;
	std::unique_ptr<D3DUAVBuffer> m_TopLevelAccelerationStructure;

	// Raytracing output
	ComPtr<ID3D12Resource> m_RaytracingOutput;
	D3DDescriptorAllocation m_RaytracingOutputDescriptor;

	// Shader tables
	static const wchar_t* c_HitGroupName;
	static const wchar_t* c_RaygenShaderName;
	static const wchar_t* c_IntersectionShaderName;
	static const wchar_t* c_ClosestHitShaderName;
	static const wchar_t* c_MissShaderName;
	std::unique_ptr<D3DShaderTable> m_MissShaderTable;
	std::unique_ptr<D3DShaderTable> m_HitGroupShaderTable;
	std::unique_ptr<D3DShaderTable> m_RayGenShaderTable;


	// Pipeline assets
	std::unique_ptr<D3DGraphicsPipeline> m_GraphicsPipeline;

	// Primitives
	std::unique_ptr<D3DUploadBuffer<PrimitiveInstancePerFrameBuffer>> m_PrimitiveAttributes;


	// Scene
	std::vector<SDFObject*> m_SceneObjects;

	// ImGui Resources
	D3DDescriptorAllocation m_ImGuiResources;
};
