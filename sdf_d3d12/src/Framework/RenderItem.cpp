#include "pch.h"
#include "RenderItem.h"

#include "imgui.h"
#include "Renderer/D3DGraphicsContext.h"


RenderItem::RenderItem()
{
	m_NumFramesDirty = D3DGraphicsContext::GetBackBufferCount();
	m_ObjectIndex = g_D3DGraphicsContext->GetNextObjectIndex();
}

void RenderItem::SetDirty()
{
	m_NumFramesDirty = D3DGraphicsContext::GetBackBufferCount();
}

bool RenderItem::DrawGui()
{
	bool changed = false;

	changed |= m_Transform.DrawGui();

	if (changed)
		SetDirty();

	return changed;
}
