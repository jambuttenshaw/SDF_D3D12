#pragma once

using namespace DirectX;


class RenderItem
{
public:
	RenderItem();

	void SetWorldMatrix(const XMMATRIX& mat);
	inline const XMMATRIX& GetWorldMatrix() const { return m_WorldMat; };

	inline bool IsDirty() const { return m_NumFramesDirty > 0; }
	inline void DecrementDirty() { m_NumFramesDirty--; }
	void SetDirty();

	inline UINT GetObjectIndex() const { return m_ObjectIndex; }

private:
	XMMATRIX m_WorldMat;

	// Dirty flag to indicate the object data has been changed 
	// and we need to update the constant buffer
	UINT m_NumFramesDirty;

	// Index into the constant buffer of per object data
	UINT m_ObjectIndex = -1;


	// Geometry associated with this object

	// Draw Indexed parameters
};
