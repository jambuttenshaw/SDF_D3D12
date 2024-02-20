#include "pch.h"
#include "SDFEditList.h"

#include "Core.h"
#include "Renderer/D3DGraphicsContext.h"


SDFEditList::SDFEditList(UINT maxEdits, float evaluationRange)
	: m_MaxEdits(maxEdits)
	, m_EvaluationRange(evaluationRange)
{
	ASSERT(m_MaxEdits <= s_EditLimit, "Edit lists are limited to 1024 edits.");

	m_Edits.resize(m_MaxEdits);
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

	m_Edits.at(m_EditCount++) = BuildEditData(edit);
	return true;
}

SDFEditData SDFEditList::BuildEditData(const SDFEdit& edit)
{
	SDFEditData primitiveData;
	primitiveData.InvWorldMat = XMMatrixTranspose(XMMatrixInverse(nullptr, edit.PrimitiveTransform.GetWorldMatrix()));
	primitiveData.Scale = edit.PrimitiveTransform.GetScale();

	primitiveData.PrimitivesAndDependencies = (edit.Shape & 0x000000FF) | ((edit.Operation << 8) & 0x0000FF00);
	primitiveData.BlendingRange = edit.BlendingRange;

	static_assert(sizeof(SDFShapeProperties) == sizeof(XMFLOAT4));
	memcpy(&primitiveData.ShapeParams, &edit.ShapeProperties, sizeof(XMFLOAT4));

	return primitiveData;
}


void SDFEditBuffer::Allocate(UINT maxEdits)
{
	m_MaxEdits = maxEdits;
	m_EditsUpload.Allocate(g_D3DGraphicsContext->GetDevice(), maxEdits, 0, L"Edit Upload");
	m_EditsBuffer.Allocate(g_D3DGraphicsContext->GetDevice(), maxEdits, D3D12_RESOURCE_STATE_COMMON, true, L"Edit Buffer");
}

void SDFEditBuffer::Populate(const SDFEditList& editList) const
{
	ASSERT(editList.GetEditCount() <= m_MaxEdits, "Edit list buffer does not have capacity for this edit list!");
	m_EditsUpload.CopyElements(0, editList.GetEditCount(), editList.GetEditData());
}

void SDFEditBuffer::CopyFromUpload(ID3D12GraphicsCommandList* commandList) const
{
	// Copy upload data into default heap
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_EditsBuffer.GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
	commandList->ResourceBarrier(1, &barrier);

	commandList->CopyBufferRegion(m_EditsBuffer.GetResource(), 0, m_EditsUpload.GetResource(), 0, m_EditsUpload.GetElementCount() * m_EditsUpload.GetElementStride());

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_EditsBuffer.GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ResourceBarrier(1, &barrier);
}
