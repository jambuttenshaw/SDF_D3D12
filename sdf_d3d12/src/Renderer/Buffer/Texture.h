#pragma once
#include "Core.h"
#include "Renderer/Memory/MemoryAllocator.h"


using Microsoft::WRL::ComPtr;


class Texture
{
public:
	Texture(const D3D12_RESOURCE_DESC* const desc, D3D12_RESOURCE_STATES initialState);
	~Texture() = default;

	DISALLOW_COPY(Texture);
	DEFAULT_MOVE(Texture);

	inline ID3D12Resource* GetResource() const { return m_Resource.Get(); }
	inline DXGI_FORMAT GetFormat() const { return m_Format; }

private:
	ComPtr<ID3D12Resource> m_Resource;
	DXGI_FORMAT m_Format = DXGI_FORMAT_UNKNOWN;
};
