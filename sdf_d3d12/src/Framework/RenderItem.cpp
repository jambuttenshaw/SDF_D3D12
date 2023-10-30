#include "pch.h"
#include "RenderItem.h"

#include "Renderer/D3DGraphicsContext.h"


RenderItem::RenderItem()
{
	m_Translation = XMVectorZero();
	m_Rotation = XMQuaternionIdentity();
	m_WorldMat = XMMatrixIdentity();

	m_NumFramesDirty = D3DGraphicsContext::GetBackBufferCount();
	m_ObjectIndex = g_D3DGraphicsContext->GetNextObjectIndex();
}


void RenderItem::SetTranslation(const XMVECTOR& translation)
{
	m_Translation = translation;
	BuildWorldMatrix();
}

void RenderItem::SetRotation(const XMVECTOR& rotation)
{
	m_Rotation = rotation;
	BuildWorldMatrix();
}

void RenderItem::SetScale(float scale)
{
	m_Scale = scale;
	SetDirty();
}


void RenderItem::SetSDFPrimitiveData(const SDFPrimitive& primitiveData)
{
	m_PrimitiveData = primitiveData;
	SetDirty();
}

void RenderItem::SetDirty()
{
	m_NumFramesDirty = D3DGraphicsContext::GetBackBufferCount();
}


void RenderItem::BuildWorldMatrix()
{
	m_WorldMat = XMMatrixRotationQuaternion(m_Rotation) * XMMatrixTranslationFromVector(m_Translation);
	SetDirty();
}
