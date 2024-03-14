#pragma once
#include "Core.h"
#include "Renderer/Memory/MemoryAllocator.h"


using Microsoft::WRL::ComPtr;


class Texture
{
public:
	Texture(const D3D12_RESOURCE_DESC* const desc, D3D12_RESOURCE_STATES initialState);
	~Texture();

	DISALLOW_COPY(Texture);
	DEFAULT_MOVE(Texture);

	inline ID3D12Resource* GetResource() const { return m_Resource.Get(); }
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetSRV() const { return m_SRV.GetGPUHandle(); }

private:
	ComPtr<ID3D12Resource> m_Resource;
	DescriptorAllocation m_SRV;
};
