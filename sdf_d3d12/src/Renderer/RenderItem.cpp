#include "pch.h"
#include "RenderItem.h"

#include "D3DGraphicsContext.h"


RenderItem::RenderItem()
{
	m_WorldMat = XMMatrixIdentity();
	m_NumFramesDirty = D3DGraphicsContext::GetBackBufferCount();
	m_ObjectIndex = g_D3DGraphicsContext->GetNextObjectIndex();
}

void RenderItem::SetWorldMatrix(const XMMATRIX& mat)
{
	m_WorldMat = mat;
	SetDirty();
}

void RenderItem::SetDirty()
{
	m_NumFramesDirty = D3DGraphicsContext::GetBackBufferCount();
}
