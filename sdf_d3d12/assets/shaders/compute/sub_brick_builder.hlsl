#ifndef SUBBRICKCOUNTER_HLSL
#define SUBBRICKCOUNTER_HLSL


#define HLSL
#include "../../../src/Renderer/Hlsl/ComputeHlslCompat.h"


#define BRICK_COUNTING_THREADS 4
#define BRICK_COUNTING_GROUP BRICK_COUNTING_THREADS * BRICK_COUNTING_THREADS * BRICK_COUNTING_THREADS


ConstantBuffer<BrickBuildParametersConstantBuffer> g_BuildParameters : register(b0);

// Ping-pong buffers for outputting the new bricks created from the sub-bricks
ByteAddressBuffer g_InBrickCounter : register(t0);
StructuredBuffer<Brick> g_InBricks : register(t1);

RWByteAddressBuffer g_OutBrickCounter : register(u0);
RWStructuredBuffer<Brick> g_OutBricks : register(u1);


// Group-shared variables

// The brick that this group will process is copied from global memory once
groupshared Brick gs_Brick;

groupshared uint gs_PrefixSum;
groupshared uint gs_GroupBrickCount;


[numthreads(BRICK_COUNTING_THREADS, BRICK_COUNTING_THREADS, BRICK_COUNTING_THREADS)]
void main(uint3 GroupID : SV_GroupID, uint GI : SV_GroupIndex, uint3 GTid : SV_GroupThreadID)
{
	const uint inBrickCount = g_InBrickCounter.Load(0);

	// Exit if there isn't a brick to consume
	if (GroupID.x > inBrickCount)
	{
		// This path is universal for the whole group
		return;
	}

	if (GI == 0)
	{
		// The first thread in the group will load the brick
		gs_Brick = g_InBricks[GroupID.x];

		// Init gsm
		gs_PrefixSum = 0;
		gs_GroupBrickCount = 0;
	}

	GroupMemoryBarrierWithGroupSync();

	// Calculate the prefix sum table
	// TODO: this is horrible doing it serially
	// TODO: this will be done parallel soon
	if (GI == 0)
	{
		uint i = 0;
		while (i++ < GI)
		{
			gs_PrefixSum += g_InBricks.Load(i).SubBrickCount;
		}
	}

	GroupMemoryBarrierWithGroupSync();

	// Check if this thread has a sub-brick to write
	if (gs_Brick.SubBrickMask & (1 << GI))
	{
		// Create brick
		Brick outBrick;
		outBrick.TopLeft_EvalSpace = gs_Brick.TopLeft_EvalSpace + g_BuildParameters.SubBrickSize * GTid;
		outBrick.SubBrickCount = 0;
		outBrick.SubBrickMask = 0;

		// write it to global memory at the correct index
		uint groupIndex;
		InterlockedAdd(gs_GroupBrickCount, 1, groupIndex);
		const uint index = gs_PrefixSum + groupIndex;

		g_OutBricks[index] = outBrick;
	}

	GroupMemoryBarrierWithGroupSync();

	if (GI == 0)
	{
		// Increment the global brick counter
		uint _;
		g_OutBrickCounter.InterlockedAdd(0, gs_GroupBrickCount, _);
	}
}

#endif