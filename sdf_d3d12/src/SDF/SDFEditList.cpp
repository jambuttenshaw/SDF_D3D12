#include "pch.h"
#include "SDFEditList.h"

#include "Core.h"
#include "Renderer/D3DGraphicsContext.h"


SDFEditList::SDFEditList(UINT maxEdits)
	: m_EditsBuffers(D3DGraphicsContext::GetBackBufferCount())
	, m_MaxEdits(maxEdits)
{
	m_EditsStaging.resize(m_MaxEdits);
	for (auto& buffer : m_EditsBuffers)
		buffer.Allocate(g_D3DGraphicsContext->GetDevice(), m_MaxEdits, 0, L"Edit Buffer");
}

void SDFEditList::Reset()
{
	m_EditCount = 0;
}

bool SDFEditList::AddEdit(const SDFEdit& edit)
{
	if (m_EditCount >= m_MaxEdits)
	{
		LOG_WARN("Cannot add edit: Edit buffer full!")
		return false;
	}

	m_EditsStaging.at(m_EditCount++) = BuildEditData(edit);
	return true;
}

void SDFEditList::CopyStagingToGPU() const
{
	m_EditsBuffers[g_D3DGraphicsContext->GetCurrentBackBuffer()].CopyElements(0, m_EditCount, m_EditsStaging.data());
}

D3D12_GPU_VIRTUAL_ADDRESS SDFEditList::GetEditBufferAddress() const
{
	return m_EditsBuffers[g_D3DGraphicsContext->GetCurrentBackBuffer()].GetAddressOfElement(0);
}


void SDFEditList::PrintEditList() const
{
	for (auto& edit : m_EditsStaging)
	{
		LOG_INFO("Shape: {} Scale: {}", edit.Shape, edit.Scale);
	}
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
