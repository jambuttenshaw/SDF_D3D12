#pragma once

using namespace DirectX;


class Transform
{
public:
	Transform(bool scaleMatrix = false);
	Transform(const XMFLOAT3& translation, bool scaleMatrix = false);
	Transform(float x, float y, float z, bool scaleMatrix = false);

	inline const XMVECTOR& GetTranslation() const { return m_Translation; }

	inline float GetYaw() const { return m_Yaw; }
	inline float GetPitch() const { return m_Pitch; }
	inline float GetRoll() const { return m_Roll; }

	inline float GetScale() const { return m_Scale; }

	inline const XMMATRIX& GetWorldMatrix() const { return m_WorldMat; };

	void SetTranslation(const XMVECTOR& translation);
	void SetYaw(float yaw);
	void SetPitch(float pitch);
	void SetRoll(float roll);
	void SetScale(float scale);

	bool DrawGui();

private:
	void BuildWorldMatrix();

private:
	XMVECTOR m_Translation;
	float m_Yaw = 0.0f, m_Pitch = 0.0f, m_Roll = 0.0f;
	float m_Scale = 1.0f;

	bool m_ScaleMatrix = false; // Should scale be put into the matrix?
	XMMATRIX m_WorldMat;
};
