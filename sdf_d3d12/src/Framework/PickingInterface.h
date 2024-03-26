#pragma once
#include "Renderer/Buffer/DefaultBuffer.h"
#include "Renderer/Buffer/ReadbackBuffer.h"
#include "Renderer/Buffer/UploadBuffer.h"


using namespace DirectX;


// Can be attached to the raytracer to query ray intersections
class PickingInterface
{
	struct PickingParameters
	{
		// The index of the ray that should report what it is picking, in terms of its screen-space pixel position
		XMUINT2 RayIndex;
	};

	struct PickingOutput
	{
		// The instance ID of the object that the ray hit, or INVALID_INSTANCE_ID if it did not hit anything
		UINT InstanceID;
		// The world-space position of the hit
		XMFLOAT3 HitLocation;
	};
	

public:
	PickingInterface();

	inline void SetNextPickLocation(const XMUINT2& location) { m_PickingParamsStaging.RayIndex = location; }

	void UploadPickingParams() const;
	void CopyPickingResult(ID3D12GraphicsCommandList* commandList);

private:
	// Resources required for picking

	// This will contain as many instances of params as there is frame resources, to prevent simultaneous R/W access
	UploadBuffer<PickingParameters> m_PickingParamsBuffer;
	PickingParameters m_PickingParamsStaging;

	// Raytracing will write its picking output into this buffer
	DefaultBuffer m_PickingOutputBuffer;
	// This buffer will be used to copy back the picking output so it can be read by the CPU
	ReadbackBuffer<PickingOutput> m_PickingReadbackBuffer;
};
