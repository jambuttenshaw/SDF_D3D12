#ifndef RAYTRACINGHLSLCOMPAT_H
#define RAYTRACINGHLSLCOMPAT_H

#ifdef HLSL
#include "HlslCompat.h"
#else
using namespace DirectX;
#endif

#include "HlslDefines.h"


// Flags
#define RENDER_FLAG_NONE										0u
#define RENDER_FLAG_DISPLAY_BOUNDING_BOX						1u
#define RENDER_FLAG_DISPLAY_HEATMAP								2u
#define RENDER_FLAG_DISPLAY_NORMALS								4u
#define RENDER_FLAG_DISPLAY_BRICK_INDEX							8u
#define RENDER_FLAG_DISPLAY_POOL_UVW							16u
#define RENDER_FLAG_DISPLAY_ITERATION_GUARD_TERMINATIONS		32u

#define INTERSECTION_FLAG_NONE									0u
#define INTERSECTION_FLAG_NO_REMAP_NORMALS						1u
#define INTERSECTION_FLAG_ITERATION_GUARD_TERMINATION			2u


struct MyAttributes
{
	XMFLOAT3 normal;
	UINT heatmap;		// Sphere marching iteration count
	UINT flags;
};
struct RayPayload
{
	XMFLOAT4 color;
};


// Constant attributes per frame
struct PassConstantBuffer
{
	XMMATRIX View;
	XMMATRIX InvView;
	XMMATRIX Proj;
	XMMATRIX InvProj;
	XMMATRIX ViewProj;
	XMMATRIX InvViewProj;

	XMFLOAT3 WorldEyePos;

	UINT Flags;

	XMFLOAT2 RTSize;
	XMFLOAT2 InvRTSize;

	float TotalTime;
	float DeltaTime;
};


// This structure associates a raytracing AABB with a brick in the pool
struct BrickPointer
{
	// Where this brick is placed in object space
	// For transforming rays to local space of the brick
	XMFLOAT3 AABBCentre;

	// Which brick in the pool does this point to
	UINT BrickIndex;
};

// Data that is constant among all bricks in an object
struct BrickPropertiesConstantBuffer
{
	// Half the brick dimensions
	float BrickHalfSize;
	// Total number of bricks in the object
	UINT BrickCount;
};

#endif