#ifndef COMPUTEHLSLCOMPAT_H
#define COMPUTEHLSLCOMPAT_H

#ifdef HLSL
#include "HlslCompat.h"
#else
using namespace DirectX;
#endif

#include "StructureHlslCompat.h"
#include "HlslDefines.h"

#define EDIT_DEPENDENCY_THREAD_COUNT 64
#define AABB_BUILDING_THREADS 128


enum SDFShape
{
	SDF_SHAPE_SPHERE = 0,
	SDF_SHAPE_BOX,
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
	// Store transform as individual rotation, translation and scale
	// SDFs can only be scaled uniformly
	XMFLOAT4 InvRotation;
	XMFLOAT3 InvTranslation;

	float Scale;

	// Shape-specific data - eg radius of a sphere, extents of a box
	XMFLOAT4 ShapeParams;

	// First byte - Primitive Shape (allows max 256 shapes)
	// Second byte - Operation (max 4 operations - only 2 bits required)
	//				 First bit of operation is union/subtraction
	//				 Second bit is hard/smooth
	// Third and Fourth byte - Dependencies (max 1023 dependencies - only 10 bits required)

	// Dependencies are other shapes in the edit list that this shape depends on
	// For smooth blending - the things that this shape will smooth blend into must not be culled
	UINT PrimitivesAndDependencies;

	float BlendingRange;
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