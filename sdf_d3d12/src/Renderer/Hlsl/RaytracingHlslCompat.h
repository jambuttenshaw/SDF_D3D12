#ifndef RAYTRACINGHLSLCOMPAT_H
#define RAYTRACINGHLSLCOMPAT_H

#ifdef HLSL
#include "HlslCompat.h"
#else
using namespace DirectX;
#endif


// Flags
#define RENDER_FLAG_DISPLAY_BOUNDING_BOX		1 << 0  // Display bounding boxes instead of raymarching to show geometry


struct MyAttributes
{
	XMFLOAT3 normal;
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


// Data used to describe properties of a volume to a shader
struct VolumeConstantBuffer
{
	UINT VolumeDimensions;	// If volume is cubic then only one dimension is needed
	float InvVolumeDimensions;
};

// Data used to describe each primitive to the shader
struct AABBPrimitiveData
{
	XMFLOAT4 AABBMin;
	XMFLOAT4 AABBMax;
	XMFLOAT4 AABBCentre;

	// Which section of the volume should be rendered inside this AABB
	XMFLOAT4 UVWMin;	// Both should be in range [0,1]
	XMFLOAT4 UVWMax;
};

#endif