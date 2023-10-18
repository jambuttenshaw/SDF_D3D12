#pragma once

#include "Memory/D3DMemoryAllocator.h"

using Microsoft::WRL::ComPtr;


class D3DConstantBuffer
{
public:
	D3DConstantBuffer(void* initialData, UINT size);
	~D3DConstantBuffer();

	D3D12_GPU_DESCRIPTOR_HANDLE GetCBV() const;
	void CopyData(void* data) const;

protected:
	ComPtr<ID3D12Resource> m_Buffer;
	D3DDescriptorAllocation m_CBVs;

	UINT m_BufferSize;
	UINT8* m_MappedAddress;
};
