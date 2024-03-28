#include "pch.h"
#include "Transform.h"

#include "imgui.h"


Transform::Transform(bool scaleMatrix)
	: m_ScaleMatrix(scaleMatrix)
{
	m_Translation = XMVectorZero();
	m_WorldMat = XMMatrixIdentity();
}

Transform::Transform(const XMFLOAT3& translation, bool scaleMatrix)
	: Transform(scaleMatrix)
{
	m_Translation = XMLoadFloat3(&translation);
	BuildWorldMatrix();
}

Transform::Transform(float x, float y, float z, bool scaleMatrix)
	: Transform({ x, y, z }, scaleMatrix)
{
}


void Transform::SetTranslation(const XMVECTOR& translation)
{
	m_Translation = translation;
	BuildWorldMatrix();
}

void Transform::SetTranslation(const XMFLOAT3& translation)
{
	m_Translation = XMLoadFloat3(&translation);
	BuildWorldMatrix();
}

void Transform::SetYaw(float yaw)
{
	m_Yaw = yaw;
	BuildWorldMatrix();
}

void Transform::SetPitch(float pitch)
{
	m_Pitch = pitch;
	BuildWorldMatrix();
}

void Transform::SetRoll(float roll)
{
	m_Roll = roll;
	BuildWorldMatrix();
}

void Transform::SetScale(float scale)
{
	m_Scale = scale;
}

bool Transform::DrawGui()
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

		// Reset button
		if (ImGui::Button("Reset"))
		{
			m_Translation = { 0.0f, 0.0f, 0.0f };
			m_Pitch = 0.0f;
			m_Yaw = 0.0f;
			m_Roll = 0.0f;
			m_Scale = 1.0f;
			BuildWorldMatrix();
		}
	}
	return changed;
}


void Transform::BuildWorldMatrix()
{
	if (m_ScaleMatrix)
		m_WorldMat = XMMatrixScaling(m_Scale, m_Scale, m_Scale)
				   * XMMatrixRotationRollPitchYaw(m_Pitch, m_Yaw, m_Roll)
				   * XMMatrixTranslationFromVector(m_Translation);
	else
		m_WorldMat = XMMatrixRotationRollPitchYaw(m_Pitch, m_Yaw, m_Roll)
				   * XMMatrixTranslationFromVector(m_Translation);
}
