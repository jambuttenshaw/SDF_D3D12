#pragma once

#include "Framework/Transform.h"
#include "HlslCompat/RaytracingHlslCompat.h"


class SDFGeometryInstance
{
public:
	SDFGeometryInstance(UINT instanceID, size_t blasIndex)
		: m_InstanceID(instanceID)
		, m_BLASIndex(blasIndex)
	{}


	inline UINT GetInstanceID() const { return m_InstanceID; }

	inline const Transform& GetTransform() const { return m_Transform; }
	inline Transform& GetTransform() { return m_Transform; }


	// Acceleration structure
	inline bool IsDirty() const { return m_Dirty; }
	inline size_t GetAccelerationStructureIndex() const { return m_BLASIndex; }

	inline void ResetDirty() { m_Dirty = false; }

private:
	UINT m_InstanceID = INVALID_INSTANCE_ID;
	// Have any fields of this object been changed that will
	// require its instance description in the TLAS to be updated
	bool m_Dirty = true;

	Transform m_Transform;

	// Which index of BLAS is this geometry an instance of
	size_t m_BLASIndex;
};
