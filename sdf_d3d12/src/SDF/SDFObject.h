#pragma once

#include "SDFTypes.h"
#include "Renderer/Memory/D3DMemoryAllocator.h"


using Microsoft::WRL::ComPtr;

/**
 * A 3D volume texture that contains the SDF data of an object
 * TODO: Think of a better name for this!
 */
class SDFObject
{
public:
	SDFObject(UINT width, UINT height, UINT depth);
	~SDFObject();

	inline ID3D12Resource* GetResource() const { return m_Resource.Get(); }

	inline UINT GetWidth() const { return m_Width; }
	inline UINT GetHeight() const { return m_Height; }
	inline UINT GetDepth() const { return m_Depth; }

	inline D3D12_GPU_DESCRIPTOR_HANDLE GetSRV() const { return m_ResourceViews.GetGPUHandle(0); };
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetUAV() const{ return m_ResourceViews.GetGPUHandle(1); }

	// Manipulate primitives list
	void AddPrimitive(SDFPrimitive&& primitive);

	inline size_t GetPrimitiveCount() const { return m_Primitives.size(); }
	inline const SDFPrimitive& GetPrimitive(size_t index) const { return m_Primitives.at(index); }

private:
	UINT m_Width = 0,
		 m_Height = 0,
		 m_Depth = 0;

	ComPtr<ID3D12Resource> m_Resource;
	D3DDescriptorAllocation m_ResourceViews;	// index 0 = SRV
												// index 1 = UAV

	// The primitives that make up this object
	// The baked SDF texture is constructed by rendering these primitives in order
	std::vector<SDFPrimitive> m_Primitives;
};
