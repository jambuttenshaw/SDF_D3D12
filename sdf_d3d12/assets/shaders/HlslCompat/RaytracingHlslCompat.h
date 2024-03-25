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
#define RENDER_FLAG_NONE										(0)
#define RENDER_FLAG_DISPLAY_BOUNDING_BOX						(1 << 0)
#define RENDER_FLAG_DISPLAY_HEATMAP								(1 << 1)
#define RENDER_FLAG_DISPLAY_NORMALS								(1 << 2)
#define RENDER_FLAG_DISPLAY_BRICK_INDEX							(1 << 3)
#define RENDER_FLAG_DISPLAY_POOL_UVW							(1 << 4)
#define RENDER_FLAG_DISPLAY_BRICK_EDIT_COUNT					(1 << 5)
#define RENDER_FLAG_DISABLE_IBL									(1 << 6)
#define RENDER_FLAG_DISABLE_SKYBOX								(1 << 7)
#define RENDER_FLAG_DISABLE_SHADOW								(1 << 8)
#define RENDER_FLAG_DISABLE_REFLECTION							(1 << 9)


// Raytracing Params and structures

#define MAX_RAY_RECURSION_DEPTH 3 // Primary rays + reflection rays + shadow rays


struct SDFIntersectAttrib
{
	XMFLOAT3 hitUVW;	// The UVW coordinates of the hit-point within the volume texture. This is used to calculate normals and materials
	UINT utility;		// general purpose integer to pass through to the closest hit shader for debug visualization purposes
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