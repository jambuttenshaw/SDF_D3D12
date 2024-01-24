#ifndef RAYTRACINGHLSLCOMPAT_H
#define RAYTRACINGHLSLCOMPAT_H

#ifdef HLSL
#include "HlslCompat.h"
#else
using namespace DirectX;
#endif


// Flags
#define RENDER_FLAG_DISPLAY_BOUNDING_BOX 1u
#define RENDER_FLAG_DISPLAY_HEATMAP 2u
#define RENDER_FLAG_DISPLAY_NORMALS 4u
#define RENDER_FLAG_DISPLAY_BRICK_INDEX 8u


struct MyAttributes
{
	XMFLOAT3 normal;
	UINT heatmap;		// Sphere marching iteration count
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
	float AABBHalfExtent; // this could be constant?

	// Which brick in the pool does this point to
	UINT BrickIndex;
};

#endif