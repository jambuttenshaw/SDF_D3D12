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

#include "../include/brick_helper.hlsli"
#include "../include/sdf_primitives.hlsli"
#include "../include/sdf_operations.hlsli"


ConstantBuffer<BrickEvaluationConstantBuffer> g_BuildParameters : register(b0);
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





[numthreads(SDF_BRICK_SIZE_VOXELS_ADJACENCY, SDF_BRICK_SIZE_VOXELS_ADJACENCY, SDF_BRICK_SIZE_VOXELS_ADJACENCY)]
void main(uint3 GroupID : SV_GroupID, uint3 GTid : SV_GroupThreadID)
{
	// Which brick is this thread processing
	const BrickPointer brick = g_BrickBuffer[GroupID.x];

	// Calculate the point in space that this thread is processing
	const float3 evaluationPosition = (brick.AABBCentre - 0.5f * g_BuildParameters.EvalSpace_BrickSize)		// Top left of brick
									+ ((float3) GTid - 0.5f) / g_BuildParameters.EvalSpace_VoxelsPerUnit;	// Offset within brick
																											// such that group thread (0,0,0) goes to (-0.5, -0.5, -0.5)
																											// and (7,7,7) goes to (6.5, 6.5, 6.5)
																											// and then map from voxels to eval space units

	// Evaluate SDF volume
	const float nearest = EvaluateEditList(evaluationPosition);
	const float mappedDistance = FormatDistance(nearest, g_BuildParameters.EvalSpace_VoxelsPerUnit);

	// Now calculate where to store the voxel in the brick pool
	const uint3 brickVoxel = CalculateBrickPoolPosition(brick.BrickIndex, g_BuildParameters.BrickPool_BrickCapacityPerAxis) + GTid;

	// Store the mapped distance in the volume
	g_OutputTexture[brickVoxel] = mappedDistance;
}

#endif