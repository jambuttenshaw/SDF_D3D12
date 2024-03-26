#include "pch.h"
#include "PickingInterface.h"

#include "Renderer/D3DGraphicsContext.h"


PickingInterface::PickingInterface()
{
	m_PickingParamsStaging.RayIndex = { 0, 0 };

	const auto device = g_D3DGraphicsContext->GetDevice();

	m_PickingParamsBuffer.Allocate(device, D3DGraphicsContext::GetBackBufferCount(), 0, L"Picking Params");
	m_PickingOutputBuffer.Allocate(device, sizeof(PickingOutput), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"Picking Output");
	m_PickingReadbackBuffer.Allocate(device, D3DGraphicsContext::GetBackBufferCount(), 0, L"Picking Readback");
}


void PickingInterface::UploadPickingParams() const
{
	m_PickingParamsBuffer.CopyElement(g_D3DGraphicsContext->GetCurrentBackBuffer(), m_PickingParamsStaging);
}

void PickingInterface::CopyPickingResult(ID3D12GraphicsCommandList* commandList)
{
	
}
