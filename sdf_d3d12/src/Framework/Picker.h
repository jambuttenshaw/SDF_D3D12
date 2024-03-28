#pragma once
#include "HlslCompat/RaytracingHlslCompat.h"
#include "Renderer/Buffer/DefaultBuffer.h"
#include "Renderer/Buffer/ReadbackBuffer.h"
#include "Renderer/Buffer/UploadBuffer.h"


using namespace DirectX;


// Can be attached to the raytracer to query ray intersections
class Picker
{
public:
	Picker();

	inline void SetNextPickLocation(const XMUINT2& location) { m_PickingParamsStaging.rayIndex = location; }

	void UploadPickingParams() const;
	void CopyPickingResult(ID3D12GraphicsCommandList* commandList) const;

	void ReadLastPick();
	const PickingQueryPayload& GetLastPick() const { return m_LastPick; }

	// Getters
	D3D12_GPU_VIRTUAL_ADDRESS GetPickingParamsBuffer() const;
	D3D12_GPU_VIRTUAL_ADDRESS GetPickingOutputBuffer() const { return m_PickingOutputBuffer.GetAddress(); }

private:
	// Resources required for picking

	// This will contain as many instances of params as there is frame resources, to prevent simultaneous R/W access
	UploadBuffer<PickingQueryParameters> m_PickingParamsBuffer;
	PickingQueryParameters m_PickingParamsStaging;

	// Raytracing will write its picking output into this buffer
	DefaultBuffer m_PickingOutputBuffer;
	// This buffer will be used to copy back the picking output so it can be read by the CPU
	ReadbackBuffer<PickingQueryPayload> m_PickingReadbackBuffer;
	// The result of the last pick retrieved
	PickingQueryPayload m_LastPick;
};
