#ifndef COMPUTEHLSLCOMPAT_H
#define COMPUTEHLSLCOMPAT_H

#ifdef HLSL
#include "HlslCompat.h"
#else
using namespace DirectX;
#endif


#define AABB_BUILD_NUM_THREADS_PER_GROUP 8 // 8x8x8 threads per group to build AABBs

#define SDF_BRICK_SIZE_IN_VOXELS 8
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
	// The region of space to run the brick builder over
	XMVECTOR EvalSpace_MinBoundary;
	XMVECTOR EvalSpace_MaxBoundary;

	XMUINT3 EvalSpace_BricksPerAxis;	// The number of bricks along each axis in the space to evaluate
	float EvalSpace_BrickSize;			// Object-space size of a brick

	// Information about how many bricks the pool can store
	XMUINT3 BrickPool_BrickCapacityPerAxis;

	UINT SDFEditCount;
};

#endif