#include "pch.h"
#include "PickingQueryInterface.h"

#include "Renderer/D3DGraphicsContext.h"


PickingQueryInterface::PickingQueryInterface()
{
	m_PickingParamsStaging.rayIndex = { INVALID_PICK_INDEX, INVALID_PICK_INDEX };

	m_LastPick.hitLocation = { D3D12_FLOAT32_MAX, D3D12_FLOAT32_MAX, D3D12_FLOAT32_MAX };
	m_LastPick.instanceID = INVALID_INSTANCE_ID;

	const auto device = g_D3DGraphicsContext->GetDevice();

	m_PickingParamsBuffer.Allocate(device, D3DGraphicsContext::GetBackBufferCount(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, L"Picking Params");
	m_PickingOutputBuffer.Allocate(device, sizeof(PickingQueryPayload), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"Picking Output");
	m_PickingReadbackBuffer.Allocate(device, D3DGraphicsContext::GetBackBufferCount(), 0, L"Picking Readback");
}


void PickingQueryInterface::UploadPickingParams() const
{
	m_PickingParamsBuffer.CopyElement(g_D3DGraphicsContext->GetCurrentBackBuffer(), m_PickingParamsStaging);
}

void PickingQueryInterface::CopyPickingResult(ID3D12GraphicsCommandList* commandList) const
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_PickingOutputBuffer.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->ResourceBarrier(1, &barrier);

	const UINT64 destOffset = g_D3DGraphicsContext->GetCurrentBackBuffer() * sizeof(PickingQueryPayload);
	commandList->CopyBufferRegion(m_PickingReadbackBuffer.GetResource(), destOffset, m_PickingOutputBuffer.GetResource(), 0, sizeof(PickingQueryPayload));

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_PickingOutputBuffer.GetResource(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ResourceBarrier(1, &barrier);
}


void PickingQueryInterface::ReadLastPick()
{
	m_LastPick = m_PickingReadbackBuffer.ReadElement(g_D3DGraphicsContext->GetCurrentBackBuffer());
}


D3D12_GPU_VIRTUAL_ADDRESS PickingQueryInterface::GetPickingParamsBuffer() const
{
	return m_PickingParamsBuffer.GetAddressOfElement(g_D3DGraphicsContext->GetCurrentBackBuffer());
}


