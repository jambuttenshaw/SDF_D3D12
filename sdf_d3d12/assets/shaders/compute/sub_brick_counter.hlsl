#ifndef SUBBRICKCOUNTER_HLSL
#define SUBBRICKCOUNTER_HLSL


#define HLSL
#include "../HlslCompat/ComputeHlslCompat.h"

#include "../include/sdf_helper.hlsli"


#define BRICK_COUNTING_THREADS 4
#define BRICK_COUNTING_GROUP BRICK_COUNTING_THREADS * BRICK_COUNTING_THREADS * BRICK_COUNTING_THREADS


ConstantBuffer<BrickBuildParametersConstantBuffer> g_BuildParameters : register(b0);

// In the sub-brick counter, no new bricks are created or consumed
// The brick counter should therefore be read-only
ByteAddressBuffer g_BrickCounter : register(t0);
StructuredBuffer<SDFEditData> g_EditList : register(t1);
StructuredBuffer<uint16_t> g_IndexBuffer : register(t2);

RWStructuredBuffer<Brick> g_Bricks : register(u0);

// For calculating prefix sums
RWStructuredBuffer<uint> g_CountTable : register(u1);


// Group-shared variables

// The brick that this group will process is copied from global memory once
// then modified by potentially every thread in the group
// before it is copied back to global memory
groupshared Brick gs_Brick;
groupshared uint gs_SubBrickCount;


float EvaluateEditList(float3 p)
{
#ifdef DISABLE_EDIT_CULLING
	const uint editCount = g_BuildParameters.SDFEditCount;
#else
	const uint editCount = gs_Brick.IndexCount;
#endif

	// Evaluate SDF list
	float nearest = FLOAT_MAX;
	
	for (uint i = 0; i < editCount; i++)
	{
		// Load the edit index
		
#ifdef DISABLE_EDIT_CULLING
		const SDFEditData edit = g_EditList.Load(i);
#else
		const uint16_t index = g_IndexBuffer.Load(gs_Brick.IndexOffset + i);
		const SDFEditData edit = g_EditList.Load(index);
#endif

		// apply primitive transform
		const float3 p_transformed = opTransform(p, edit.InvRotation, edit.InvTranslation) / edit.Scale;
		
		// evaluate primitive
		float dist = sdPrimitive(p_transformed, GetShape(edit.EditParams), edit.ShapeParams);
		dist *= edit.Scale;

		// combine with scene
		nearest = opPrimitive(nearest, dist, GetOperation(edit.EditParams), edit.BlendingRange);
	}

	return nearest;
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
		gs_SubBrickCount = 0;
	}

	GroupMemoryBarrierWithGroupSync();

	// Determine if this thread's sub-region is occupied

	// Calculate the centre of the sub-brick that this thread should evaluate
	const float3 subBrickCentre = gs_Brick.TopLeft + g_BuildParameters.SubBrickSize * (GTid + 0.5f);
	const float distance = EvaluateEditList(subBrickCentre);

	// If there is a distance value of magnitude small enough, that means its possible this sub-brick
	// could contain geometry and it should be either divided or evaluated
	if (abs(distance) < g_BuildParameters.SubBrickSize)
	{
		// This sub-brick contains geometry and should be evaluated

		uint _;			// temps

		// Increment the count
		InterlockedAdd(gs_SubBrickCount, 1, _);
		// Set the bit corresponding to this brick
		if (GI < 32)
		{
			InterlockedOr(gs_Brick.SubBrickMask.x, uint(1) << GI, _);
		}
		else
		{
			InterlockedOr(gs_Brick.SubBrickMask.y, uint(1) << (GI - 32), _);
		}
	}

	GroupMemoryBarrierWithGroupSync();

	if (GI == 0)
	{
		g_Bricks[GroupID.x] = gs_Brick;
		g_CountTable[GroupID.x] = gs_SubBrickCount;
	}
}

#endif