#ifndef SUBBRICKBUILDER_HLSL
#define SUBBRICKBUILDER_HLSL


#define HLSL
#include "../../../src/Renderer/Hlsl/ComputeHlslCompat.h"


#define BRICK_COUNTING_THREADS 4
#define BRICK_COUNTING_GROUP BRICK_COUNTING_THREADS * BRICK_COUNTING_THREADS * BRICK_COUNTING_THREADS


ConstantBuffer<BrickBuildParametersConstantBuffer> g_BuildParameters : register(b0);

// Ping-pong buffers for outputting the new bricks created from the sub-bricks
ByteAddressBuffer g_InBrickCounter : register(t0);
StructuredBuffer<Brick> g_InBricks : register(t1);
StructuredBuffer<uint> g_InPrefixSumTable : register(t2);

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
	if (GI == 0)
	{
		// The first thread in the group will load the brick
		gs_Brick = g_InBricks[GroupID.x];

		// Init gsm
		gs_PrefixSum = g_InPrefixSumTable[GroupID.x];
		gs_GroupBrickCount = 0;
	}

	GroupMemoryBarrierWithGroupSync();

	// Check if this thread has a sub-brick to write
	uint buildBrick = 0;
	if (GI < 32)
		buildBrick = gs_Brick.SubBrickMask.x & (uint(1) << GI);
	else
		buildBrick = gs_Brick.SubBrickMask.y & (uint(1) << (GI - 32));
	if (buildBrick != 0)
	{
		// Create brick
		Brick outBrick;
		outBrick.SubBrickMask = uint2(0, 0);
		outBrick.TopLeft_EvalSpace = gs_Brick.TopLeft_EvalSpace + g_BuildParameters.SubBrickSize * GTid;

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