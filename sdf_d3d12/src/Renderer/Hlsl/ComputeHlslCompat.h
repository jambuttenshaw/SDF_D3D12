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


enum SDFShape
{
	SDF_SHAPE_SPHERE = 0,
	SDF_SHAPE_BOX,
	SDF_SHAPE_PLANE,
	SDF_SHAPE_TORUS,
	SDF_SHAPE_OCTAHEDRON,
	SDF_SHAPE_BOX_FRAME,
};

enum SDFOperation
{
	SDF_OP_UNION = 0,
	SDF_OP_SUBTRACTION,
	SDF_OP_SMOOTH_UNION,
	SDF_OP_SMOOTH_SUBTRACTION
};


struct SDFEditData
{
	XMMATRIX InvWorldMat;
	float Scale;

	// Note - operation will only take two bits.
	// 	if first bit is set, it is a subtraction
	// 	if second bit is set, it is a smooth operation
	UINT Primitive;

	float BlendingRange;

	// Other shapes in the edit list that this shape depends on
	// For smooth blending - the things that this shape will smooth blend into must not be culled
	UINT Dependencies;

	XMFLOAT4 ShapeParams;
};



// Constant buffer types

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