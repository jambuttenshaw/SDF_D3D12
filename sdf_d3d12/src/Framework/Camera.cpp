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

		const XMVECTOR direction = XMVector3Normalize({ sinY * cosP, sinP, cosY * cosP });
		const XMVECTOR up = XMVector3Normalize({ 0.0f, 1.0f, 0.0f });
		if (XMVector3Equal(up, direction))
			XMVectorSwizzle(up, 0, 2, 1, 3);

		const XMVECTOR target = m_Position + direction;

		XMStoreFloat3(&m_Forward, direction);
		XMStoreFloat3(&m_Up, up);
		m_ViewMatrix = XMMatrixLookAtLH(m_Position, target, up);

		m_Dirty = false;
	}
}

void Camera::ClampYaw()
{
	if (m_Yaw > 3.1416f)
		m_Yaw = fmodf(m_Yaw, 3.1416f) - 3.1416f;
	if (m_Yaw < -3.1416f)
		m_Yaw = 3.1416f - fmodf(m_Yaw, 3.1416f);
}

void Camera::ClampPitch()
{
	if (m_Pitch < -1.5708f)
	{
		m_Pitch = -1.5708f;
	}
	if (m_Pitch > 1.5708f)
		m_Pitch = 1.5708f;
}
