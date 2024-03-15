#include "pch.h"
#include "Texture.h"

#include "Renderer/D3DGraphicsContext.h"


Texture::Texture(const D3D12_RESOURCE_DESC* const desc, D3D12_RESOURCE_STATES initialState)
	: m_Format(desc->Format)
{
	const auto defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	THROW_IF_FAIL(g_D3DGraphicsContext->GetDevice()->CreateCommittedResource(
		&defaultHeap,
		D3D12_HEAP_FLAG_NONE,
		desc,
		initialState,
		nullptr,
		IID_PPV_ARGS(&m_Resource)));
}
