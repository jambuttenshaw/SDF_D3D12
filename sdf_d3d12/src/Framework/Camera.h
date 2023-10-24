#pragma once

using namespace DirectX;


class Camera
{
public:
	Camera()
	{
		m_ViewMatrix = XMMatrixIdentity();
	}

	XMFLOAT3 GetPosition() const;
	
	inline float GetYaw() const { return m_Yaw; }
	inline float GetPitch() const { return m_Pitch; }

	inline void SetPosition(const XMFLOAT3& position) { m_Position = XMLoadFloat3(&position); m_Dirty = true; }
	inline void SetYaw(float yaw) { m_Yaw = yaw; m_Dirty = true; }
	inline void SetPitch(float pitch) { m_Pitch = pitch; m_Dirty = true; }

	inline void Translate(const XMFLOAT3& translation) { m_Position += XMLoadFloat3(&translation); m_Dirty = true; }
	inline void RotateYaw(float deltaYaw) { m_Yaw += deltaYaw; m_Dirty = true; }
	inline void RotatePitch(float deltaPitch) { m_Pitch += deltaPitch; m_Dirty = true; }

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
	void RebuildIfDirty();

private:
	XMVECTOR m_Position{ 0.0f, 0.0f, 0.0f };

	float m_Yaw = 0.0f;
	float m_Pitch = 0.0f;

	XMMATRIX m_ViewMatrix;
	XMFLOAT3 m_Forward{ 0.0f, 0.0f, 1.0f };
	XMFLOAT3 m_Up{ 0.0f, 0.0f, 1.0f };
	bool m_Dirty = true;	// Flags when matrix requires reconstruction
};
