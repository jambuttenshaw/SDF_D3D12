#ifndef SUBBRICKCOUNTER_HLSL
#define SUBBRICKCOUNTER_HLSL


#define HLSL
#include "../../../src/Renderer/Hlsl/ComputeHlslCompat.h"

#include "../include/sdf_primitives.hlsli"
#include "../include/sdf_operations.hlsli"


#define BRICK_COUNTING_THREADS 4
#define BRICK_COUNTING_GROUP BRICK_COUNTING_THREADS * BRICK_COUNTING_THREADS * BRICK_COUNTING_THREADS


ConstantBuffer<BrickBuildParametersConstantBuffer> g_BuildParameters : register(b0);

// In the sub-brick counter, no new bricks are created or consumed
// The brick counter should therefore be read-only
ByteAddressBuffer g_BrickCounter : register(t0);
StructuredBuffer<SDFEditData> g_EditList : register(t1);

RWStructuredBuffer<Brick> g_Bricks : register(u0);


// Group-shared variables

// The brick that this group will process is copied from global memory once
// then modified by potentially every thread in the group
// before it is copied back to global memory
groupshared Brick gs_Brick;



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



[numthreads(BRICK_COUNTING_THREADS, BRICK_COUNTING_THREADS, BRICK_COUNTING_THREADS)]
void main(uint3 GroupID : SV_GroupID, uint GI : SV_GroupIndex, uint3 GTid : SV_GroupThreadID)
{
	const uint brickCount = g_BrickCounter.Load(0);

	// Exit if there isn't a brick to consume
	if (GroupID.x > brickCount)
	{
		return;
	}

	if (GI == 0)
	{
		gs_Brick = g_Bricks[GroupID.x];
	}

	GroupMemoryBarrierWithGroupSync();

	// Determine if this thread's sub-region is occupied

	// Calculate the centre of the sub-brick that this thread should evaluate
	const float3 subBrickCentre = gs_Brick.TopLeft_EvalSpace + g_BuildParameters.SubBrickSize * (GTid + 0.5f);
	const float distance = EvaluateEditList(subBrickCentre);

	// If there is a distance value of magnitude small enough, that means its possible this sub-brick
	// could contain geometry and it should be either divided or evaluated
	if (abs(distance) < g_BuildParameters.SubBrickSize)
	{
		// This sub-brick contains geometry and should be evaluated

		uint _;			// temps
		uint64_t __;

		// Increment the count
		InterlockedAdd(gs_Brick.SubBrickCount, 1, _);
		// Set the bit corresponding to this brick
		InterlockedAnd(gs_Brick.SubBrickMask, 1 << GI, __);
	}

	GroupMemoryBarrierWithGroupSync();

	if (GI == 0)
	{
		g_Bricks[GroupID.x] = gs_Brick;
	}
}

#endif