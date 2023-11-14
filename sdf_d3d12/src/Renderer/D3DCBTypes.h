#pragma once

#include "SDF/SDFTypes.h"

using namespace DirectX;


struct RayMarchPropertiesType
{
	float SphereTraceEpsilon;
	float RayMarchEpsilon;
	float RayMarchStepSize;
	float NormalEpsilon;
};


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

	RayMarchPropertiesType RayMarchProperties;
};


struct ObjectCBType
{
	// Inverse world matrix is required to position the bounding box
	XMMATRIX InvWorldMat;

	// Only uniform scale can be applied to SDFs
	float Scale;

	// The extents of the bounding box that contains the SDF
	XMFLOAT3 BoundingBoxExtents;
};


struct SDFPrimitiveBufferType
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


	// Constructors

	SDFPrimitiveBufferType() = default;

	// Construct from a SDFPrimitive
	SDFPrimitiveBufferType(const SDFPrimitive& primitive)
	{
		InvWorldMat = XMMatrixIdentity();
		Scale = 1.0f;

		Shape = static_cast<UINT>(primitive.Shape);
		Operation = static_cast<UINT>(primitive.Operation);
		BlendingFactor = primitive.BlendingFactor;

		memcpy(&ShapeProperties, &primitive.ShapeProperties, sizeof(XMFLOAT4));

		Color = primitive.Color;
	}
};
