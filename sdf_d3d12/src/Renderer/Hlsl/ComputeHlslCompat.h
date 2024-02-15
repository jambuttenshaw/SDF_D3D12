#ifndef COMPUTEHLSLCOMPAT_H
#define COMPUTEHLSLCOMPAT_H

#ifdef HLSL
#include "HlslCompat.h"
#else
using namespace DirectX;
#endif

#include "StructureHlslCompat.h"
#include "HlslDefines.h"


#define MORTON_ENUMERATOR_THREADS 8 // 8x8x8
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
};

struct BrickEvaluationConstantBuffer
{
	// Object-space size of a brick
	float EvalSpace_BrickSize;

	// Information about how many bricks the pool can store
	XMUINT3 BrickPool_BrickCapacityPerAxis;

	float EvalSpace_VoxelsPerUnit;

	// How many bricks are in the object
	UINT BrickCount;

	UINT SDFEditCount;
};


struct BrickBuildParametersConstantBuffer
{
	float BrickSize;
	float SubBrickSize; // = BrickSize / 4
	float EvalSpaceSize;

	UINT SDFEditCount;
};


#endif