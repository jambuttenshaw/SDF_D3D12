#ifndef RAYTRACINGHLSLCOMPAT_H
#define RAYTRACINGHLSLCOMPAT_H

#ifdef HLSL
#include "HlslCompat.h"
#else

using namespace DirectX;

#endif


struct MyAttributes
{
	XMFLOAT3 position;
};
struct RayPayload
{
	XMFLOAT4 color;
};

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



#endif
