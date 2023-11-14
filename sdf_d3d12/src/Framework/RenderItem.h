#pragma once
#include "Transform.h"
#include "SDF/SDFTypes.h"

using namespace DirectX;

/**
 * RenderItem can be seen as a bounding box for a SDF texture
 * The RenderItem will be sphere-traced, and then its contents ray-marched
 */
class RenderItem
{
public:
	RenderItem();

	inline const Transform& GetTransform() const { return m_Transform; }
	inline Transform& GetTransform() { SetDirty(); return m_Transform; }

	inline const XMFLOAT3& GetBoundsExtents() const { return m_BoundsExtents; }
	inline void SetBoundsExtents(const XMFLOAT3& extents) { m_BoundsExtents = extents; }

	inline bool IsDirty() const { return m_NumFramesDirty > 0; }
	inline void DecrementDirty() { m_NumFramesDirty--; }
	void SetDirty();

	inline UINT GetObjectIndex() const { return m_ObjectIndex; }


	bool DrawGui();

private:
	Transform m_Transform;

	// The extents of the bounding box this item represents
	XMFLOAT3 m_BoundsExtents = { 1.0f, 1.0f, 1.0f };

	// Dirty flag to indicate the object data has been changed 
	// and we need to update the constant buffer
	UINT m_NumFramesDirty;

	// Index into the constant buffer of per object data
	UINT m_ObjectIndex = -1;
};
