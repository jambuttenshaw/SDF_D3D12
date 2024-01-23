#ifndef AABBBUILDER_HLSL
#define AABBBUILDER_HLSL

#define HLSL
#include "../../../src/Renderer/Hlsl/ComputeHlslCompat.h"
#include "../../../src/Renderer/Hlsl/RaytracingHlslCompat.h"

#include "../include/sdf_primitives.hlsli"
#include "../include/sdf_operations.hlsli"

/*
 *
 *	This shader takes a 3D texture containing distance values and builds a buffer of AABB's around the iso-surface
 *
 */

ConstantBuffer<SDFBuilderConstantBuffer> g_BuildParameters : register(b0);
StructuredBuffer<SDFEditData> g_EditList : register(t0);

RWByteAddressBuffer g_Counter : register(u0);
RWStructuredBuffer<AABB> g_AABBBuffer : register(u1);
RWStructuredBuffer<BrickPointer> g_BrickBuffer : register(u2);


float EvaluateEditList(float3 p)
{
	// Evaluate SDF list
	float4 nearest = float4(0.0f, 0.0f, 0.0f, FLOAT_MAX);
	
	for (uint i = 0; i < g_BuildParameters.SDFEditCount; i++)
	{
		// apply primitive transform
		const float3 p_transformed = opTransform(p, g_EditList[i].InvWorldMat) / g_EditList[i].Scale;
		
		// evaluate primitive
		float4 shape = float4(
			g_EditList[i].Color.rgb,
			sdPrimitive(p_transformed, g_EditList[i].Shape, g_EditList[i].ShapeParams)
		);
		shape.w *= g_EditList[i].Scale;

		// combine with scene
		nearest = opPrimitive(nearest, shape, g_EditList[i].Operation, g_EditList[i].BlendingFactor);
	}

	return nearest.w;
}

float FormatDistance(float inDistance)
{
	// Calculate the distance value in terms of voxels
	const float voxelsPerAxis = g_BuildParameters.EvalSpace_BricksPerAxis.x * SDF_BRICK_SIZE_IN_VOXELS;
	const float voxelsPerUnit = voxelsPerAxis / (g_BuildParameters.EvalSpace_MaxBoundary - g_BuildParameters.EvalSpace_MinBoundary).x;
	const float voxelDistance = inDistance * voxelsPerUnit;

	// Now map the distance such that 1 = SDF_VOLUME_STRIDE number of voxels
	return voxelDistance / SDF_VOLUME_STRIDE;
}


[numthreads(AABB_BUILD_NUM_THREADS_PER_GROUP, AABB_BUILD_NUM_THREADS_PER_GROUP, AABB_BUILD_NUM_THREADS_PER_GROUP)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	// Map thread to a region of space
	// The thread will determine if space is empty in that region
	// If space is not empty, then it will create a brick for that region

	// Calculate where in the evaluation region the centre of this brick lies
	const float3 t = DTid / float3(g_BuildParameters.EvalSpace_BricksPerAxis) + 0.5f * g_BuildParameters.EvalSpace_BrickSize;
	const float3 brickCentre = (1.0f - t) * g_BuildParameters.EvalSpace_MinBoundary.xyz + t * g_BuildParameters.EvalSpace_MaxBoundary.xyz;

	// Evaluate object at this point
	const float distance = EvaluateEditList(brickCentre);
	const float mappedDistance = FormatDistance(distance);

	// If distance is larger than the size of this voxel then this voxel cannot contain geometry
	// Use abs(distance) to cull bounding boxes within the geometry
	if (abs(mappedDistance) > 2.25f)
		return;

	// Use the counter to get the index in  the aabb buffer that this thread will operate on
	// Only do this if this thread is going to add a box
	uint brickIndex;
	g_Counter.InterlockedAdd(0, 1, brickIndex);

	AABB outAABB;
	outAABB.TopLeft = brickCentre - 0.5f * g_BuildParameters.EvalSpace_BrickSize;
	outAABB.BottomRight = brickCentre + 0.5f * g_BuildParameters.EvalSpace_BrickSize;
	g_AABBBuffer[brickIndex] = outAABB;

	BrickPointer brickPointer;
	brickPointer.AABBCentre = brickCentre;
	brickPointer.AABBHalfExtent = 0.5f * g_BuildParameters.EvalSpace_BrickSize;
	brickPointer.BrickIndex = brickIndex;
	g_BrickBuffer[brickIndex] = brickPointer;
}

#endif