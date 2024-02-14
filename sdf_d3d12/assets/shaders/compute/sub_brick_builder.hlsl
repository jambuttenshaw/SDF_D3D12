#ifndef SUBBRICKBUILDER_HLSL
#define SUBBRICKBUILDER_HLSL


#include "../include/brick_helper.hlsli"

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
groupshared Brick gs_InBrick;
groupshared Brick gs_OutBricks[BRICK_COUNTING_GROUP];

groupshared uint gs_PrefixSum;
groupshared uint gs_GroupBrickCount;

// For sorting the bricks to output
groupshared uint gs_MortonCodes[BRICK_COUNTING_GROUP];
groupshared uint gs_OutBrickIndices[BRICK_COUNTING_GROUP];


[numthreads(BRICK_COUNTING_THREADS, BRICK_COUNTING_THREADS, BRICK_COUNTING_THREADS)]
void main(uint3 GroupID : SV_GroupID, uint GI : SV_GroupIndex, uint3 GTid : SV_GroupThreadID)
{
	if (GI == 0)
	{
		// The first thread in the group will load the brick
		gs_InBrick = g_InBricks[GroupID.x];

		// Init gsm
		gs_PrefixSum = g_InPrefixSumTable[GroupID.x];
		gs_GroupBrickCount = 0;
	}

	GroupMemoryBarrierWithGroupSync();

	// Check if this thread has a sub-brick to write
	uint buildBrick = 0;
	if (GI < 32)
		buildBrick = gs_InBrick.SubBrickMask.x & ((uint(1) << GI));
	else
		buildBrick = gs_InBrick.SubBrickMask.y & ((uint(1) << (GI - 32)));
	if (buildBrick != 0)
	{
		// Create brick
		gs_OutBricks[GI].SubBrickMask = uint2(0, 0);
		gs_OutBricks[GI].TopLeft_EvalSpace = gs_InBrick.TopLeft_EvalSpace + g_BuildParameters.SubBrickSize * GTid;
		gs_OutBricks[GI].IndexOffset = gs_InBrick.IndexOffset; // These will be refined in the next stage
		gs_OutBricks[GI].IndexCount = gs_InBrick.IndexCount;

		// Calculate morton code for brick
		const uint mortonCode = morton3Df(0.5f + (gs_OutBricks[GI].TopLeft_EvalSpace / g_BuildParameters.EvalSpaceSize));
		gs_MortonCodes[GI] = mortonCode;

		// Increment the group brick counter
		uint _;
		InterlockedAdd(gs_GroupBrickCount, 1, _);
	}
	else
	{
		// this will ensure all empty bricks get sorted to the end of the array and don't get placed into global memory
		gs_MortonCodes[GI] = 0xFFFFFFFF;
	}

	// An indirection table that will contain which brick to place where in the final buffer
	gs_OutBrickIndices[GI] = GI;

	GroupMemoryBarrierWithGroupSync();

	// Sort bricks based on morton code
	const uint mortonCode = gs_MortonCodes[GI];
	uint destIndex = 0;
	for (uint n = 0; n < BRICK_COUNTING_GROUP; n++)
	{
		if (mortonCode >= gs_MortonCodes[n] && (mortonCode != gs_MortonCodes[n] || GI > n))
			destIndex++;
	} 

	const uint tempIndex = gs_OutBrickIndices[GI];

	GroupMemoryBarrierWithGroupSync();

	// Re-order
	gs_OutBrickIndices[destIndex] = tempIndex;

	GroupMemoryBarrierWithGroupSync();


	if (GI < gs_GroupBrickCount)
	{
		// Write brick to global memory
		const uint index = gs_PrefixSum + GI;
		g_OutBricks[index] = gs_OutBricks[gs_OutBrickIndices[GI]];
	}

	if (GI == 0)
	{
		// Increment the global brick counter
		uint _;
		g_OutBrickCounter.InterlockedAdd(0, gs_GroupBrickCount, _);
	}
}

#endif