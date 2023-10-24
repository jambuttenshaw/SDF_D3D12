#pragma once

using namespace DirectX;


class Camera
{
public:
	Camera()
	{
		m_ViewMatrix = XMMatrixIdentity();
	}

	inline const XMFLOAT3& GetPosition() const { return m_Position; }
	inline float GetYaw() const { return m_Yaw; }
	inline float GetPitch() const { return m_Pitch; }

	inline void SetPosition(const XMFLOAT3& position) { m_Position = position; m_Dirty = true; }
	inline void SetYaw(float yaw) { m_Yaw = yaw; m_Dirty = true; }
	inline void SetPitch(float pitch) { m_Pitch = pitch; m_Dirty = true; }

	const XMFLOAT3& GetForward()
	{
		RebuildIfDirty();
		return m_Forward;
	}
	const XMFLOAT3& GetUp()
	{
		RebuildIfDirty();
		return m_Up;
	}
	const XMMATRIX& GetViewMatrix()
	{
		RebuildIfDirty();
		return m_ViewMatrix;
	}

private:
	void RebuildIfDirty()
	{
		if (m_Dirty)
		{
			// Recalculate properties
			const XMVECTOR pos = XMLoadFloat3(&m_Position);

			const float sinY = sinf(m_Yaw);
			const float cosY = cosf(m_Yaw);
			const float sinP = sinf(m_Pitch);
			const float cosP = cosf(m_Pitch);

			const XMFLOAT3 dirF3{ sinY * cosP, sinP, cosY * cosP};
			const XMVECTOR direction = XMVector3Normalize(XMLoadFloat3(&dirF3));

			const XMFLOAT3 upF3{ -sinY * sinP, cosP, -sinY * sinP };
			const XMVECTOR up = XMVector3Normalize(XMLoadFloat3(&upF3));

			const XMVECTOR target = pos + direction;

			XMStoreFloat3(&m_Forward, direction);
			XMStoreFloat3(&m_Up, up);
			m_ViewMatrix = XMMatrixLookAtLH(pos, target, up);

			m_Dirty = false;
		}
	}

private:
	XMFLOAT3 m_Position{ 0.0f, 0.0f, 0.0f };

	float m_Yaw = 0.0f;
	float m_Pitch = 0.0f;

	XMMATRIX m_ViewMatrix;
	XMFLOAT3 m_Forward{ 0.0f, 0.0f, 1.0f };
	XMFLOAT3 m_Up{ 0.0f, 0.0f, 1.0f };
	bool m_Dirty = true;	// Flags when matrix requires reconstruction
};
