#include "pch.h"
#include "SDFEditList.h"

#include "Core.h"
#include "Renderer/D3DGraphicsContext.h"


SDFEditList::SDFEditList(UINT maxEdits, float evaluationRange)
	: m_MaxEdits(maxEdits)
	, m_EvaluationRange(evaluationRange)
{
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

	primitiveData.Shape = static_cast<UINT>(edit.Shape);
	primitiveData.Operation = static_cast<UINT>(edit.Operation);
	primitiveData.BlendingFactor = edit.BlendingFactor;

	static_assert(sizeof(SDFShapeProperties) == sizeof(XMFLOAT4));
	memcpy(&primitiveData.ShapeParams, &edit.ShapeProperties, sizeof(XMFLOAT4));

	primitiveData.Color = edit.Color;

	return primitiveData;
}


void SDFEditBuffer::Allocate(UINT maxEdits)
{
	m_MaxEdits = maxEdits;
	m_EditsBuffer.Allocate(g_D3DGraphicsContext->GetDevice(), maxEdits, 0, L"Edit Buffer");
}

void SDFEditBuffer::Populate(const SDFEditList& editList) const
{
	ASSERT(editList.GetEditCount() <= m_MaxEdits, "Edit list buffer does not have capacity for this edit list!");
	m_EditsBuffer.CopyElements(0, editList.GetEditCount(), editList.GetEditData());
}

