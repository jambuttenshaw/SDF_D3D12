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

	XMMATRIX LocalSpaceToBottomLevelAS;
	XMMATRIX BottomLevelASToLocalSpace;
};

#endif