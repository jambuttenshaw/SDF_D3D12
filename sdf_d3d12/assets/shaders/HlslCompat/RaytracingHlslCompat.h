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
#define RENDER_FLAG_DISABLE_IBL									64u
#define RENDER_FLAG_DISABLE_SKYBOX								128u
#define RENDER_FLAG_DISABLE_SHADOW								256u

#define INTERSECTION_FLAG_NONE									0u
#define INTERSECTION_FLAG_NO_REMAP_NORMALS						1u


// Raytracing Params and structures

#define MAX_RAY_RECURSION_DEPTH 2 // Primary rays + shadow rays


struct SDFIntersectAttrib
{
	XMFLOAT3 normal;
	XMUINT3 materials;
	UINT utility;		// general purpose integer to pass through to the closest hit shader for debug visualization purposes
	UINT flags;
};

struct RadianceRayPayload
{
	XMFLOAT4 color;
	UINT recursionDepth;
};
struct ShadowRayPayload
{
	bool hit;
};


enum RayType
{
	RayType_Primary = 0,
	RayType_Shadow,
	RayType_Count
};


namespace TraceRayParameters
{
	static const UINT InstanceMask = ~0; // Everything is visible

	namespace HitGroup
	{
		static const UINT Offset[RayType_Count] =
		{
			0,	// Primary rays
			1	// Shadow rays
		};
		static const UINT GeometryStride = RayType_Count;	// The number of entries in the shader table between each geometry
	}

	namespace MissShader
	{
		static const UINT Offset[RayType_Count] =
		{
			0,	// Primary rays
			1	// Shadow rays
		};
	}
}



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

	float TotalTime;
	float DeltaTime;

	UINT HeatmapQuantization;
	float HeatmapHueRange;

	// Lighting
	LightGPUData Light;
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