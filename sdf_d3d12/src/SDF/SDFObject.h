#pragma once

#include "Renderer/Memory/D3DMemoryAllocator.h"
#include "Renderer/Raytracing/AABBGeometry.h"
#include "Renderer/Hlsl/RaytracingHlslCompat.h"
#include "Renderer/Buffer/D3DStructuredBuffer.h"


using Microsoft::WRL::ComPtr;

/**
 * A 3D volume texture that contains the SDF data of an object
 */
class SDFObject : public BaseAABBGeometry
{
public:
	SDFObject(UINT volumeResolution, UINT volumeStride, UINT aabbDivisions);
	virtual ~SDFObject();

	DISALLOW_COPY(SDFObject)
	DEFAULT_MOVE(SDFObject)


	// Volume
	inline ID3D12Resource* GetVolumeResource() const { return m_VolumeResource.Get(); }

	inline UINT GetVolumeResolution() const { return m_Resolution; }
	inline UINT GetVolumeStride() const { return m_VolumeStride; }

	inline D3D12_GPU_DESCRIPTOR_HANDLE GetVolumeSRV() const { return m_VolumeResourceViews.GetGPUHandle(0); };
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetVolumeUAV() const{ return m_VolumeResourceViews.GetGPUHandle(1); }


	// Geometry Interface
	inline UINT GetDivisions() const { return m_Divisions; }
	inline virtual UINT GetAABBCount() const override { return m_AABBCount; }
	inline virtual UINT GetMaxAABBCount() const { return m_MaxAABBCount; }

	inline virtual D3D12_GPU_VIRTUAL_ADDRESS GetAABBBufferAddress() const override { return m_AABBBuffer.GetAddress(); }
	inline virtual D3D12_GPU_VIRTUAL_ADDRESS GetPrimitiveDataBufferAddress() const override { return m_PrimitiveDataBuffer.GetAddress(); }

	inline D3D12_GPU_DESCRIPTOR_HANDLE GetAABBBufferUAV() const { return m_AABBBuffer.GetUAV(); }
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetPrimitiveDataBufferUAV() const { return m_PrimitiveDataBuffer.GetUAV(); }

	// Used to update the aabb count after the AABB builder compute shader has executed
	// Should only be called from the SDF factory
	inline void SetAABBCount(UINT count) { m_AABBCount = count; }

private:
	// Volume
	UINT m_Resolution = 0;
	UINT m_VolumeStride = 0; // Volume stride is how distances are mapped in terms of the volume resolution
							 // e.g. Volume Stride = 8 would mean a distance value of 1 in the volume would map to 8 voxels

	ComPtr<ID3D12Resource> m_VolumeResource;
	D3DDescriptorAllocation m_VolumeResourceViews;	// index 0 = SRV
													// index 1 = UAV

	// Geometry
	D3DRWStructuredBuffer<D3D12_RAYTRACING_AABB> m_AABBBuffer;
	D3DRWStructuredBuffer<AABBPrimitiveData> m_PrimitiveDataBuffer;

	UINT m_Divisions = 0;	 // The maximum number of AABBs along each axis

	UINT m_MaxAABBCount = 0; // The maximum possible value of AABB count
	UINT m_AABBCount = 0;	 // The number of AABBs that actually make up this object
};
