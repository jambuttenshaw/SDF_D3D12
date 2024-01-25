#ifndef SDFBAKER_HLSL
#define SDFBAKER_HLSL

/*
 *
 *	This shader takes a buffer of edits to evaluate and stores the result in a 3D Texture
 *
 */

#define HLSL
#include "../../../src/Renderer/Hlsl/ComputeHlslCompat.h"
#include "../../../src/Renderer/Hlsl/RaytracingHlslCompat.h"
#include "../include/sdf_primitives.hlsli"
#include "../include/sdf_operations.hlsli"


ConstantBuffer<SDFBuilderConstantBuffer> g_BuildParameters : register(b0);
StructuredBuffer<SDFEditData> g_EditList : register(t0);

StructuredBuffer<BrickPointer> g_BrickBuffer : register(t1);
RWTexture3D<float> g_OutputTexture : register(u1);


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
	const float voxelsPerAxis = g_BuildParameters.EvalSpace_BricksPerAxis.x * SDF_BRICK_SIZE_VOXELS;
	const float voxelsPerUnit = voxelsPerAxis / (g_BuildParameters.EvalSpace_MaxBoundary - g_BuildParameters.EvalSpace_MinBoundary).x;
	const float voxelDistance = inDistance * voxelsPerUnit;

	// Now map the distance such that 1 = SDF_VOLUME_STRIDE number of voxels
	return voxelDistance / SDF_VOLUME_STRIDE;
}


// Calculates which voxel in the brick pool this thread will map to
uint3 CalculateBrickPoolPosition(uint brickIndex)
{
	// For now bricks are stored linearly
	uint3 brickTopLeft;

	brickTopLeft.x = brickIndex % g_BuildParameters.BrickPool_BrickCapacityPerAxis.x;
	brickIndex /= g_BuildParameters.BrickPool_BrickCapacityPerAxis.x;
	brickTopLeft.y = brickIndex % g_BuildParameters.BrickPool_BrickCapacityPerAxis.y;
	brickIndex /= g_BuildParameters.BrickPool_BrickCapacityPerAxis.y;
	brickTopLeft.z = brickIndex;

	return brickTopLeft * SDF_BRICK_SIZE_VOXELS_ADJACENCY;
}


[numthreads(SDF_BRICK_SIZE_VOXELS_ADJACENCY, SDF_BRICK_SIZE_VOXELS_ADJACENCY, SDF_BRICK_SIZE_VOXELS_ADJACENCY)]
void main(uint3 GroupID : SV_GroupID, uint3 GTid : SV_GroupThreadID)
{
	// Which brick is this thread processing
	const BrickPointer brick = g_BrickBuffer[GroupID.x];

	// How much of evaluation space does 1 voxel take up
	const float voxelInEvaluationSpaceUnits = g_BuildParameters.EvalSpace_BrickSize / SDF_BRICK_SIZE_VOXELS;

	// Calculate the point in space that this thread is processing
	const float3 evaluationPosition = (brick.AABBCentre - 0.5f * g_BuildParameters.EvalSpace_BrickSize)
									+ ((float3) GTid - 0.5f) * voxelInEvaluationSpaceUnits;

	// Evaluate SDF volume
	const float nearest = EvaluateEditList(evaluationPosition);
	const float mappedDistance = FormatDistance(nearest);

	// Now calculate where to store the voxel in the brick pool
	const uint3 brickVoxel = CalculateBrickPoolPosition(brick.BrickIndex) + GTid;

	// Store the mapped distance in the volume
	g_OutputTexture[brickVoxel] = mappedDistance;
}

#endif