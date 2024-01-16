#pragma once

#include "SDFTypes.h"
#include "Renderer/Memory/D3DMemoryAllocator.h"


class RenderItem;
using Microsoft::WRL::ComPtr;

/**
 * A 3D volume texture that contains the SDF data of an object
 * TODO: Think of a better name for this!
 */
class SDFObject
{
public:
	SDFObject(UINT resolution);
	~SDFObject();

	inline ID3D12Resource* GetResource() const { return m_Resource.Get(); }

	inline UINT GetResolution() const { return m_Resolution; }

	inline D3D12_GPU_DESCRIPTOR_HANDLE GetSRV() const { return m_ResourceViews.GetGPUHandle(0); };
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetUAV() const{ return m_ResourceViews.GetGPUHandle(1); }

	// Manipulate primitives list
	void AddPrimitive(SDFPrimitive&& primitive);

	inline size_t GetPrimitiveCount() const { return m_Primitives.size(); }
	inline const SDFPrimitive& GetPrimitive(size_t index) const { return m_Primitives.at(index); }

	// Affect the render item that will draw the bounding box of this object
	RenderItem* GetRenderItem() const { return m_RenderItem; }

private:
	UINT m_Resolution = 0;

	ComPtr<ID3D12Resource> m_Resource;
	D3DDescriptorAllocation m_ResourceViews;	// index 0 = SRV
												// index 1 = UAV

	// The primitives that make up this object
	// The baked SDF texture is constructed by rendering these primitives in order
	std::vector<SDFPrimitive> m_Primitives;

	RenderItem* m_RenderItem = nullptr;
};
