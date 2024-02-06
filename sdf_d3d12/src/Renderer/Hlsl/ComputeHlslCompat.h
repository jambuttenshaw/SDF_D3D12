#ifndef COMPUTEHLSLCOMPAT_H
#define COMPUTEHLSLCOMPAT_H

#ifdef HLSL
#include "HlslCompat.h"
#else
using namespace DirectX;
#endif

#include "HlslDefines.h"

// TODO: DEPRECATED
#define AABB_BUILD_NUM_THREADS_PER_GROUP 8 // 8x8x8 threads per group to build AABBs

#define AABB_BUILDING_THREADS 128


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

struct BrickEvaluationConstantBuffer
{
	// The region of space to run the brick builder over
	XMVECTOR EvalSpace_MinBoundary;
	XMVECTOR EvalSpace_MaxBoundary;

	XMUINT3 EvalSpace_BricksPerAxis;	// The number of bricks along each axis in the space to evaluate
	float EvalSpace_BrickSize;			// Object-space size of a brick

	// Information about how many bricks the pool can store
	XMUINT3 BrickPool_BrickCapacityPerAxis;

	// Helpful values
	float EvalSpace_VoxelsPerUnit;

	UINT SDFEditCount;
};

struct AABBBuilderConstantBuffer
{
	UINT BrickCount;
	float BrickSize;
};



////// NEW SDF FACTORY TYPES //////

struct Brick
{
	XMFLOAT3 TopLeft_EvalSpace;	// Top left of this brick in eval space
	UINT SubBrickCount;			// The number of sub-bricks that this brick contains
								// Will initially be 0 before the sub_brick_counter has executed
	uint64_t SubBrickMask;		// A bit mask of sub-bricks
								// Will initially be 0 before the sub_brick_counter has executed

	// Temp debug variables
	UINT PrefixSum;
	UINT GroupID;
	uint64_t Bitmask;		
};

struct BrickBuildParametersConstantBuffer
{
	float BrickSize;
	float SubBrickSize; // = BrickSize / 4

	UINT SDFEditCount;
};


#endif