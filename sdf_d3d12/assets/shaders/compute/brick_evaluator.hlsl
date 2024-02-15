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


struct EvalGroupOffset
{
	UINT Offset;
};
ConstantBuffer<EvalGroupOffset> g_EvalGroupOffset : register(b0);
ConstantBuffer<BrickEvaluationConstantBuffer> g_BuildParameters : register(b1);

StructuredBuffer<SDFEditData> g_EditList : register(t0);
StructuredBuffer<uint> g_IndexBuffer : register(t1);

StructuredBuffer<Brick> g_BrickBuffer : register(t2);
RWTexture3D<float> g_OutputTexture : register(u1);


#define MAX_EDITS_CHUNK 64
groupshared SDFEditData gs_Edits[MAX_EDITS_CHUNK];
groupshared Brick gs_Brick;


float EvaluateEditList(float3 p, uint GI)
{
	// Evaluate SDF list
	float nearest = FLOAT_MAX;

	const uint numChunks = (gs_Brick.IndexCount + MAX_EDITS_CHUNK - 1) / MAX_EDITS_CHUNK;
	uint editsRemaining = gs_Brick.IndexCount;
	for (uint chunk = 0; chunk < numChunks; chunk++)
	{
		// Load edits
		GroupMemoryBarrierWithGroupSync();

		if (GI < min(MAX_EDITS_CHUNK, gs_Brick.IndexCount))
		{
			const uint index = g_IndexBuffer.Load(gs_Brick.IndexOffset + chunk * MAX_EDITS_CHUNK + GI);
			gs_Edits[GI] = g_EditList.Load(index);
		}

		GroupMemoryBarrierWithGroupSync();

		// Eval edits
		for (uint edit = 0; edit < min(MAX_EDITS_CHUNK, editsRemaining); edit++)
		{
			// apply primitive transform
			const float3 p_transformed = opTransform(p, gs_Edits[edit].InvWorldMat) / gs_Edits[edit].Scale;
		
			// evaluate primitive
			float dist = sdPrimitive(p_transformed, gs_Edits[edit].Shape, gs_Edits[edit].ShapeParams);
			dist *= gs_Edits[edit].Scale;

			// combine with scene
			nearest = opPrimitive(nearest, dist, gs_Edits[edit].Operation, gs_Edits[edit].BlendingFactor);
		}

		editsRemaining -= MAX_EDITS_CHUNK;
	}

	return nearest;
}

float EvaluateEditList_NoCulling(float3 p, uint GI)
{
	// Evaluate SDF list
	float nearest = FLOAT_MAX;

	const uint numChunks = (g_BuildParameters.SDFEditCount + MAX_EDITS_CHUNK - 1) / MAX_EDITS_CHUNK;
	uint editsRemaining = g_BuildParameters.SDFEditCount;
	for (uint chunk = 0; chunk < numChunks; chunk++)
	{
		// Load edits
		GroupMemoryBarrierWithGroupSync();

		if (GI < min(MAX_EDITS_CHUNK, g_BuildParameters.SDFEditCount))
		{
			gs_Edits[GI] = g_EditList.Load(chunk * MAX_EDITS_CHUNK + GI);
		}

		GroupMemoryBarrierWithGroupSync();

		// Eval edits
		for (uint edit = 0; edit < min(MAX_EDITS_CHUNK, editsRemaining); edit++)
		{
			// apply primitive transform
			const float3 p_transformed = opTransform(p, gs_Edits[edit].InvWorldMat) / gs_Edits[edit].Scale;
		
			// evaluate primitive
			float dist = sdPrimitive(p_transformed, gs_Edits[edit].Shape, gs_Edits[edit].ShapeParams);
			dist *= gs_Edits[edit].Scale;

			// combine with scene
			nearest = opPrimitive(nearest, dist, gs_Edits[edit].Operation, gs_Edits[edit].BlendingFactor);
		}

		editsRemaining -= MAX_EDITS_CHUNK;
	}

	return nearest;
}


[numthreads(SDF_BRICK_SIZE_VOXELS_ADJACENCY, SDF_BRICK_SIZE_VOXELS_ADJACENCY, SDF_BRICK_SIZE_VOXELS_ADJACENCY)]
void main(uint3 GroupID : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	const uint brickIndex = GroupID.x + g_EvalGroupOffset.Offset;
	if (GI == 0)
	{
		// Which brick is this thread processing
		gs_Brick = g_BrickBuffer[brickIndex];
	}

	GroupMemoryBarrierWithGroupSync();

	// Calculate the point in space that this thread is processing
	const float3 evaluationPosition = (gs_Brick.TopLeft) // Top left of brick
									+ ((float3) GTid - 0.5f) / g_BuildParameters.EvalSpace_VoxelsPerUnit;	// Offset within brick
																											// such that group thread (0,0,0) goes to (-0.5, -0.5, -0.5)
																											// and (7,7,7) goes to (6.5, 6.5, 6.5)
																											// and then map from voxels to eval space units

	// Evaluate SDF volume
	const float nearest = EvaluateEditList(evaluationPosition, GI);
	const float mappedDistance = FormatDistance(nearest, g_BuildParameters.EvalSpace_VoxelsPerUnit);

	// Now calculate where to store the voxel in the brick pool
	const uint3 brickVoxel = CalculateBrickPoolPosition(brickIndex, g_BuildParameters.BrickCount, g_BuildParameters.BrickPool_BrickCapacityPerAxis) + GTid;

	// Store the mapped distance in the volume
	g_OutputTexture[brickVoxel] = mappedDistance;
}

#endif