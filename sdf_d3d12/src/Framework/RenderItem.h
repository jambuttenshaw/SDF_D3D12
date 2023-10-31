#pragma once
#include "SDF/SDFTypes.h"

using namespace DirectX;


class RenderItem
{
public:
	RenderItem();

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

	void SetSDFPrimitiveData(const SDFPrimitive& primitiveData);
	inline const SDFPrimitive& GetSDFPrimitiveData() const { return m_PrimitiveData; }

	inline bool IsDirty() const { return m_NumFramesDirty > 0; }
	inline void DecrementDirty() { m_NumFramesDirty--; }
	void SetDirty();

	inline UINT GetObjectIndex() const { return m_ObjectIndex; }


	bool DrawGui();


private:
	void BuildWorldMatrix();

private:
	XMVECTOR m_Translation;
	float m_Yaw = 0.0f, m_Pitch = 0.0f, m_Roll = 0.0f;
	float m_Scale = 1.0f;

	XMMATRIX m_WorldMat;

	// SDF data associated with this object
	SDFPrimitive m_PrimitiveData;

	// Dirty flag to indicate the object data has been changed 
	// and we need to update the constant buffer
	UINT m_NumFramesDirty;

	// Index into the constant buffer of per object data
	UINT m_ObjectIndex = -1;
};
