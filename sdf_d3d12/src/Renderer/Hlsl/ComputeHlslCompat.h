#ifndef COMPUTEHLSLCOMPAT_H
#define COMPUTEHLSLCOMPAT_H

#ifdef HLSL
#include "HlslCompat.h"
#else
using namespace DirectX;
#endif


#define AABB_BUILD_NUM_THREADS_PER_GROUP 8 // 8x8x8 threads per group to build AABBs

#define SDF_BRICK_SIZE 8
#define SDF_VOLUME_STRIDE 4


struct SDFEditData
{
	XMMATRIX InvWorldMat;
	float Scale;

	// SDF primitive data
	UINT Shape;
	UINT Operation;
	float BlendingFactor;

	XMFLOAT4 ShapeParams;

	XMFLOAT4 Color;
};

struct SDFBuilderConstantBuffer
{
	UINT SDFEditCount;
	float UVWPerAABB;	// The UVW increment between each AABB ( = 1.0f / Divisions) 
};

#endif