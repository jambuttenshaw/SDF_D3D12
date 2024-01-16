#pragma once


#include "Geometry.h"


/*
 * A specialization of AABB geometry that is designed to be constructed for a SDF volume
 */
class SDFGeometry : public AABBGeometry
{
public:
	SDFGeometry(UINT divisions, float halfSize);
	virtual ~SDFGeometry() = default;

	DISALLOW_COPY(SDFGeometry)
	DEFAULT_MOVE(SDFGeometry)

private:
	UINT m_Divisions = 0;	// The number of AABBs along each axis
	float m_HalfSize = 0;	// The half-extents of the entire object

};
