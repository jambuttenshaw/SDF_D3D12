#include "pch.h"
#include "RenderItem.h"

#include "imgui.h"
#include "Renderer/D3DGraphicsContext.h"


RenderItem::RenderItem()
{
	m_Translation = XMVectorZero();
	m_WorldMat = XMMatrixIdentity();

	m_NumFramesDirty = D3DGraphicsContext::GetBackBufferCount();
	m_ObjectIndex = g_D3DGraphicsContext->GetNextObjectIndex();
}


void RenderItem::SetTranslation(const XMVECTOR& translation)
{
	m_Translation = translation;
	BuildWorldMatrix();
}

void RenderItem::SetYaw(float yaw)
{
	m_Yaw = yaw;
	BuildWorldMatrix();
}

void RenderItem::SetPitch(float pitch)
{
	m_Pitch = pitch;
	BuildWorldMatrix();
}

void RenderItem::SetRoll(float roll)
{
	m_Roll = roll;
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
	m_WorldMat = XMMatrixRotationRollPitchYaw(m_Pitch, m_Yaw, m_Roll) * XMMatrixTranslationFromVector(m_Translation);
	SetDirty();
}


bool RenderItem::DrawGui()
{
	bool changed = false;

	{
		// Transform Gui

		// Translation
		XMFLOAT3 pos;
		XMStoreFloat3(&pos, m_Translation);
		if (ImGui::DragFloat3("Translation", &pos.x, 0.01f))
		{
			changed = true;
			m_Translation = XMLoadFloat3(&pos);
			BuildWorldMatrix();
		}

		// Rotation
		{
			bool rotChanged = false;

			rotChanged |= ImGui::SliderAngle("Pitch", &m_Pitch);
			rotChanged |= ImGui::SliderAngle("Yaw", &m_Yaw);
			rotChanged |= ImGui::SliderAngle("Roll", &m_Roll);

			if (rotChanged)
			{
				changed = true;
				BuildWorldMatrix();
			}
		}

		// Scale
		changed |= ImGui::DragFloat("Scale", &m_Scale, 0.01f);
	}

	ImGui::Separator();

	// SDF primitive data gui
	changed |= m_PrimitiveData.DrawGui();

	if (changed)
		SetDirty();

	return changed;
}
