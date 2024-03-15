#ifndef STRUCTUREHLSLCOMPAT_H
#define STRUCTUREHLSLCOMPAT_H

#ifdef HLSL
#include "HlslCompat.h"
#else
using namespace DirectX;
#endif


struct Ray
{
	XMFLOAT3 origin;
	XMFLOAT3 direction;
};

struct AABB
{
	XMFLOAT3 TopLeft;
	XMFLOAT3 BottomRight;
};

struct Brick
{
	XMFLOAT3 TopLeft;	// Top left of this brick in eval space
	XMUINT2 SubBrickMask;		// A bit mask of sub-bricks
	UINT IndexOffset;			// Offset into the index buffer for this bricks indices
	UINT IndexCount;			// The number of indices this brick has
};


// Lighting structures
struct LightGPUData
{
	XMFLOAT3 Direction;

	float Intensity;
	XMFLOAT3 Color;

	float Padding;

};

struct MaterialGPUData
{
	XMFLOAT3 Albedo;
	float Roughness;
	float Metalness;
};



#endif