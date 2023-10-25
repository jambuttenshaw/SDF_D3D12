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

	inline void SetPosition(const XMVECTOR& position) { m_Position = position; m_Dirty = true; }
	inline void SetPosition(const XMFLOAT3& position) { m_Position = XMLoadFloat3(&position); m_Dirty = true; }

	void SetYaw(float yaw) { m_Yaw = yaw; ClampYaw(); m_Dirty = true; }
	void SetPitch(float pitch) { m_Pitch = pitch; ClampPitch(); m_Dirty = true; }

	inline void Translate(const XMVECTOR& translation) { m_Position += translation; m_Dirty = true; }
	inline void Translate(const XMFLOAT3& translation) { m_Position += XMLoadFloat3(&translation); m_Dirty = true; }

	void RotateYaw(float deltaYaw) { m_Yaw += deltaYaw; ClampYaw(); m_Dirty = true; }
	void RotatePitch(float deltaPitch) { m_Pitch += deltaPitch; ClampPitch(); m_Dirty = true; }

	const XMVECTOR& GetForward()
	{
		RebuildIfDirty();
		return m_Forward;
	}
	const XMVECTOR& GetUp()
	{
		RebuildIfDirty();
		return m_Up;
	}
	const XMVECTOR& GetRight()
	{
		RebuildIfDirty();
		return m_Right;
	}
	const XMMATRIX& GetViewMatrix()
	{
		RebuildIfDirty();
		return m_ViewMatrix;
	}

private:
	void RebuildIfDirty();
	void ClampYaw();
	void ClampPitch();

private:
	XMVECTOR m_Position{ 0.0f, 0.0f, 0.0f };

	float m_Yaw = 0.0f;
	float m_Pitch = 0.0f;

	XMMATRIX m_ViewMatrix;

	XMVECTOR m_Forward{ 0.0f, 0.0f, 1.0f };
	XMVECTOR m_Right{ 1.0f, 0.0f, 0.0f };
	XMVECTOR m_Up{ 0.0f, 1.0f, 0.0f };

	bool m_Dirty = true;	// Flags when matrix requires reconstruction
};
