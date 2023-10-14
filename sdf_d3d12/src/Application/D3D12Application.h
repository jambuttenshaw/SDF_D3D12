#pragma once

#include "BaseApplication.h"


using Microsoft::WRL::ComPtr;


class D3D12Application : public BaseApplication
{
public:
	D3D12Application(UINT width, UINT height, const std::wstring& name);
	virtual ~D3D12Application() override = default;

	virtual void OnInit() override;
	virtual void OnUpdate() override;
	virtual void OnRender() override;
	virtual void OnDestroy() override;

private:
	void LoadPipeline();
	void LoadAssets();

	void PopulateCommandList();
	void WaitForPreviousFrame();

private:
	static constexpr UINT s_FrameCount = 2;

	// Pipeline objects
	ComPtr<IDXGISwapChain3> m_SwapChain;
	ComPtr<ID3D12Device> m_Device;
	ComPtr<ID3D12Resource> m_RenderTargets[s_FrameCount];

	ComPtr<ID3D12CommandAllocator> m_CommandAllocator;
	ComPtr<ID3D12CommandQueue> m_CommandQueue;

	ComPtr<ID3D12DescriptorHeap> m_RTVHeap;
	UINT m_RTVDescriptorSize = 0;

	ComPtr<ID3D12PipelineState> m_PipelineState;
	ComPtr<ID3D12GraphicsCommandList> m_CommandList;

	// Synchronization objects
	UINT m_FrameIndex = 0;
	HANDLE m_FenceEvent;
	ComPtr<ID3D12Fence> m_Fence;
	UINT64 m_FenceValue;
};
