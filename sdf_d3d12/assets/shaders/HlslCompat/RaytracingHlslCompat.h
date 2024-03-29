#ifndef RAYTRACINGHLSLCOMPAT_H
#define RAYTRACINGHLSLCOMPAT_H

#ifdef HLSL
#include "HlslCompat.h"
#else
using namespace DirectX;
#endif

#include "StructureHlslCompat.h"
#include "HlslDefines.h"


// Flags
#define RENDER_FLAG_NONE										0u
#define RENDER_FLAG_DISPLAY_BOUNDING_BOX						1u
#define RENDER_FLAG_DISPLAY_HEATMAP								2u
#define RENDER_FLAG_DISPLAY_NORMALS								4u
#define RENDER_FLAG_DISPLAY_BRICK_INDEX							8u
#define RENDER_FLAG_DISPLAY_POOL_UVW							16u
#define RENDER_FLAG_DISPLAY_BRICK_EDIT_COUNT					32u

#define INTERSECTION_FLAG_NONE									0u
#define INTERSECTION_FLAG_NO_REMAP_NORMALS						1u


struct MyAttributes
{
	XMFLOAT3 normal;
	UINT utility;		// general purpose integer to pass through to the closest hit shader for debug visualization purposes
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

	UINT HeatmapQuantization;
	float HeatmapHueRange;
};


// Data that is constant among all bricks in an object
struct BrickPropertiesConstantBuffer
{
	// The brick dimensions
	float BrickSize;
	// Total number of bricks in the object
	UINT BrickCount;
};

#endif