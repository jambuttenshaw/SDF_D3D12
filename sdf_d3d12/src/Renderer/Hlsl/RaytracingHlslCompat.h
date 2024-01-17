#ifndef RAYTRACINGHLSLCOMPAT_H
#define RAYTRACINGHLSLCOMPAT_H

#ifdef HLSL
#include "HlslCompat.h"
#else
using namespace DirectX;
#endif


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

	float _; // padding

	XMFLOAT2 RTSize;
	XMFLOAT2 InvRTSize;

	float NearZ;
	float FarZ;

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