#pragma once

#include "SDF/SDFTypes.h"

using namespace DirectX;


struct PassCBType
{
	XMMATRIX View;
	XMMATRIX InvView;
	XMMATRIX Proj;
	XMMATRIX InvProj;
	XMMATRIX ViewProj;
	XMMATRIX InvViewProj;
	XMFLOAT3 WorldEyePos;
	UINT ObjectCount;
	XMFLOAT2 RTSize;
	XMFLOAT2 InvRTSize;
	float NearZ;
	float FarZ;
	float TotalTime;
	float DeltaTime;
};


struct ObjectCBType
{
	// Inverse world matrix is required to position SDF primitives
	XMMATRIX InvWorldMat;
	// Only uniform scale can be applied to SDFs
	float Scale;

	// SDF primitive data
	UINT Shape;
	UINT Operation;
	float BlendingFactor;


	static_assert(sizeof(SDFShapeProperties) == sizeof(XMFLOAT4));
	XMFLOAT4 ShapeProperties;

	XMFLOAT4 Color;
};
