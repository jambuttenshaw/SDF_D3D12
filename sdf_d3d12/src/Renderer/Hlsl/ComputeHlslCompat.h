#ifndef COMPUTEHLSLCOMPAT_H
#define COMPUTEHLSLCOMPAT_H

#ifdef HLSL
#include "HlslCompat.h"
#else
using namespace DirectX;
#endif


struct BakeDataConstantBuffer
{
	UINT PrimitiveCount;
	UINT VolumeStride;
};


struct AABBBuilderParametersConstantBuffer
{
	UINT Divisions;				// The number of bounding boxes along each side.
								// This is the same for each dimensions as SDF volumes are cubes

	UINT VolumeStride;

	float AABBDimensions;		// All AABBs will be cubic and uniform in size
								// The size can be computed from the number of divisions

	float UVWIncrement;			// The UVW increment between each AABB ( = 1.0f / Divisions) 
};

#endif