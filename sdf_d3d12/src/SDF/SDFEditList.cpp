#include "pch.h"
#include "SDFEditList.h"

#include "Renderer/D3DGraphicsContext.h"


SDFEditList::SDFEditList(UINT maxEdits)
	: m_MaxEdits(maxEdits)
{
	m_EditsStaging.reserve(maxEdits);
	m_EditsBuffer.Allocate(g_D3DGraphicsContext->GetDevice(), m_MaxEdits, 0, L"Edit Buffer");

	// Create SRV
	m_EditsBufferSRV = g_D3DGraphicsContext->GetSRVHeap()->Allocate(1);
	ASSERT(m_EditsBufferSRV.IsValid(), "Descirptor allocation failed.");

	// Build SRV desc
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = m_MaxEdits;
	srvDesc.Buffer.StructureByteStride = m_EditsBuffer.GetElementStride();
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	g_D3DGraphicsContext->GetDevice()->CreateShaderResourceView(m_EditsBuffer.GetResource(), &srvDesc, m_EditsBufferSRV.GetCPUHandle());
}

SDFEditList::~SDFEditList()
{
	m_EditsBufferSRV.Free();
}


void SDFEditList::Reset()
{
	m_EditsStaging.clear();
}

bool SDFEditList::AddEdit(const SDFEdit& edit)
{
	if (m_EditsStaging.size() >= m_MaxEdits)
	{
		LOG_WARN("Cannot add edit: Edit buffer full!")
		return false;
	}

	m_EditsStaging.emplace_back(BuildEditData(edit));
	return true;
}

void SDFEditList::CopyStagingToGPU() const
{
	m_EditsBuffer.CopyElements(0, static_cast<UINT>(m_EditsStaging.size()), m_EditsStaging.data());
}


SDFEditData SDFEditList::BuildEditData(const SDFEdit& edit)
{
	SDFEditData primitiveData;
	primitiveData.InvWorldMat = XMMatrixTranspose(XMMatrixInverse(nullptr, edit.PrimitiveTransform.GetWorldMatrix()));
	primitiveData.Scale = edit.PrimitiveTransform.GetScale();

	primitiveData.Shape = static_cast<UINT>(edit.Shape);
	primitiveData.Operation = static_cast<UINT>(edit.Operation);
	primitiveData.BlendingFactor = edit.BlendingFactor;

	static_assert(sizeof(SDFShapeProperties) == sizeof(XMFLOAT4));
	memcpy(&primitiveData.ShapeParams, &edit.ShapeProperties, sizeof(XMFLOAT4));

	primitiveData.Color = edit.Color;

	return primitiveData;
}
