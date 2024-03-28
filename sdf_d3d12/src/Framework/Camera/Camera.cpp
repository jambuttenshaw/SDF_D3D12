#include "pch.h"
#include "Camera.h"


XMFLOAT3 Camera::GetPosition() const
{
	XMFLOAT3 pos;
	XMStoreFloat3(&pos, m_Position);
	return pos;
}

void Camera::RebuildIfDirty()
{
	if (m_Dirty)
	{
		// Recalculate properties
		const float sinY = sinf(m_Yaw);
		const float cosY = cosf(m_Yaw);
		const float sinP = sinf(m_Pitch);
		const float cosP = cosf(m_Pitch);

		m_Forward = XMVector3Normalize({ sinY * cosP, sinP, cosY * cosP });
		m_Up = { 0.0f, 1.0f, 0.0f };

		m_Right = XMVector3Normalize(XMVector3Cross(m_Up, m_Forward));
		m_Up = XMVector3Cross(m_Forward, m_Right);

		const XMVECTOR target = m_Position + m_Forward;

		m_ViewMatrix = XMMatrixLookAtLH(m_Position, target, m_Up);

		m_Dirty = false;
	}
}

void Camera::ClampYaw()
{
	if (m_Yaw > 3.14f)
		m_Yaw = fmodf(m_Yaw, 3.14f) - 3.14f;
	if (m_Yaw < -3.13f)
		m_Yaw = 3.13f - fmodf(m_Yaw, 3.13f);
}

void Camera::ClampPitch()
{
	if (m_Pitch < -1.57f)
	{
		m_Pitch = -1.57f;
	}
	if (m_Pitch > 1.57f)
		m_Pitch = 1.57f;
}
