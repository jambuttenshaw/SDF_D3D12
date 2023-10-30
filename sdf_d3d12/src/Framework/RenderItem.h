#pragma once
#include "SDF/SDFTypes.h"

using namespace DirectX;


class RenderItem
{
public:
	RenderItem();

	inline const XMVECTOR& GetTranslation() const { return m_Translation; }
	inline const XMVECTOR& GetRotation() const { return m_Rotation; }
	inline float GetScale() const { return m_Scale; }
	inline const XMMATRIX& GetWorldMatrix() const { return m_WorldMat; };

	void SetTranslation(const XMVECTOR& translation);
	void SetRotation(const XMVECTOR& rotation);
	void SetScale(float scale);

	void SetSDFPrimitiveData(const SDFPrimitive& primitiveData);
	inline const SDFPrimitive& GetSDFPrimitiveData() const { return m_PrimitiveData; }

	inline bool IsDirty() const { return m_NumFramesDirty > 0; }
	inline void DecrementDirty() { m_NumFramesDirty--; }
	void SetDirty();

	inline UINT GetObjectIndex() const { return m_ObjectIndex; }

private:
	void BuildWorldMatrix();

private:
	XMVECTOR m_Translation;
	XMVECTOR m_Rotation;	// as quaternion
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
