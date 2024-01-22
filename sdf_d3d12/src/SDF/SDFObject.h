#pragma once

#include "Renderer/Memory/MemoryAllocator.h"
#include "Renderer/Raytracing/AABBGeometry.h"
#include "Renderer/Hlsl/RaytracingHlslCompat.h"
#include "Renderer/Buffer/StructuredBuffer.h"


using Microsoft::WRL::ComPtr;

/**
 * A 3D volume texture that contains the SDF data of an object
 */
class SDFObject : public BaseAABBGeometry
{
public:
	SDFObject(UINT volumeResolution);
	virtual ~SDFObject();

	DISALLOW_COPY(SDFObject)
	DEFAULT_MOVE(SDFObject)

	// Volume
	inline ID3D12Resource* GetVolumeResource() const { return m_VolumeResource.Get(); }
	inline UINT GetVolumeResolution() const { return m_VolumeResolution; }

	// Geometry Interface
	inline virtual UINT GetAABBCount() const override { return m_AABBCount; }
	inline virtual UINT GetMaxAABBCount() const { return m_MaxAABBCount; }

	inline virtual D3D12_GPU_VIRTUAL_ADDRESS GetAABBBufferAddress() const override { return m_AABBBuffer.GetAddress(); }
	inline virtual D3D12_GPU_VIRTUAL_ADDRESS GetPrimitiveDataBufferAddress() const override { return m_PrimitiveDataBuffer.GetAddress(); }

	// Get Resource Views
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetVolumeSRV() const { return m_ResourceViews.GetGPUHandle(0); };
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetVolumeUAV() const { return m_ResourceViews.GetGPUHandle(1); }
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetAABBBufferUAV() const { return m_ResourceViews.GetGPUHandle(2); }
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetPrimitiveDataBufferUAV() const { return m_ResourceViews.GetGPUHandle(3); }
	
	// Used to update the aabb count after the AABB builder compute shader has executed
	// Should only be called from the SDF factory
	inline void SetAABBCount(UINT count) { m_AABBCount = count; }

private:
	// Volume
	UINT m_VolumeResolution = 0;
	ComPtr<ID3D12Resource> m_VolumeResource;
	
	// Geometry
	StructuredBuffer<D3D12_RAYTRACING_AABB> m_AABBBuffer;
	StructuredBuffer<AABBPrimitiveData> m_PrimitiveDataBuffer;

	DescriptorAllocation m_ResourceViews;	// index 0 = volume SRV
											// index 1 = volume UAV
											// index 2 = aabb buffer uav
											// index 3 = aabb primitive data buffer uav

	UINT m_MaxAABBCount = 0; // The maximum possible value of AABB count
	UINT m_AABBCount = 0;	 // The number of AABBs that actually make up this object
};
