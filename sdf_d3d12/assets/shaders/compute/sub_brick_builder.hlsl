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

// Temporary sorting arrays
groupshared uint gs_BitsZero[BRICK_COUNTING_GROUP];
groupshared uint gs_Scan[BRICK_COUNTING_GROUP];
groupshared uint gs_TotalZeros;
groupshared uint gs_DestIndices[BRICK_COUNTING_GROUP];


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

		// Calculate morton code for brick
		const float3 brickInUnitCube = 0.5f + (gs_OutBricks[GI].TopLeft_EvalSpace / g_BuildParameters.EvalSpaceSize);
		gs_MortonCodes[GI] = morton3D(brickInUnitCube);

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
	// Loop through the first 31 bits (31st bit is required to be sorted as it will be set for empty bricks)
	[unroll(32)]
	for (uint n = 0; n < 31; n++)
	{
		// Populate the array with 1 if the current bit is 0
		gs_BitsZero[GI] = (gs_MortonCodes[GI] & (1U << n)) == 0;

		GroupMemoryBarrierWithGroupSync();

		gs_Scan[GI] = GI > 0 ? gs_BitsZero[GI - 1] : 0;

		GroupMemoryBarrierWithGroupSync();

		[unroll(6)]
		for (uint i = 1; i < BRICK_COUNTING_GROUP; i <<= 1)
		{
			uint temp;
			if (GI > i)
			{
				temp = gs_Scan[GI] + gs_Scan[GI - i];
			}
			else
			{
				temp = gs_Scan[GI];
			}
			GroupMemoryBarrierWithGroupSync();
			gs_Scan[GI] = temp;
			GroupMemoryBarrierWithGroupSync();
		}

		if (GI == 0)
		{
			gs_TotalZeros = gs_BitsZero[BRICK_COUNTING_GROUP - 1] + gs_Scan[BRICK_COUNTING_GROUP - 1];
		}
		GroupMemoryBarrierWithGroupSync();

		gs_DestIndices[GI] = gs_BitsZero[GI] ? gs_Scan[GI] : GI - gs_Scan[GI] + gs_TotalZeros;

		const uint tempCode = gs_MortonCodes[GI];
		const uint tempIndex = gs_OutBrickIndices[GI];

		GroupMemoryBarrierWithGroupSync();

		gs_MortonCodes[gs_DestIndices[GI]] = tempCode;
		gs_OutBrickIndices[gs_DestIndices[GI]] = tempIndex;

		GroupMemoryBarrierWithGroupSync();
	} 
	
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