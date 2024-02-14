#ifndef EDITTESTER_HLSL
#define EDITTESTER_HLSL


#define HLSL
#include "../../../src/Renderer/Hlsl/ComputeHlslCompat.h"

#define EDIT_TESTING_THREADS 32


struct EditTestingParameters
{
	UINT SDFEditCount;
};
ConstantBuffer<EditTestingParameters> g_Parameters : register(b0);

StructuredBuffer<SDFEditData> g_EditList : register(t0);

ByteAddressBuffer g_BrickCounter : register(t1);

RWStructuredBuffer<Brick> g_Bricks : register(u0);

RWStructuredBuffer<uint> g_IndexBuffer : register(u1);
RWByteAddressBuffer g_IndexCounter : register(u2);


#define MAX_EDITS_PER_THREAD 32 // how many indices each thread can store


// The brick that this group will process
groupshared Brick gs_Brick;
// The number of edits affecting this brick
groupshared uint gs_EditCount;
// The offset into the index buffer where this bricks indices are stored
groupshared uint gs_IndexOffset;


[numthreads(EDIT_TESTING_THREADS, 1, 1)]
void main(uint3 GroupID : SV_GroupID, uint GI : SV_GroupIndex)
{
	const uint brickCount = g_BrickCounter.Load(0);
	if (GroupID.x >= brickCount)
		return;

	if (GI == 0)
	{
		gs_Brick = g_Bricks.Load(GroupID.x);
	}

	GroupMemoryBarrierWithGroupSync();

	// Each thread will process an edit and determine if its relevant
	uint localEditCount = 0;
	uint localIndices[MAX_EDITS_PER_THREAD];


	const uint editsPerThread = min((g_Parameters.SDFEditCount + EDIT_TESTING_THREADS - 1) / EDIT_TESTING_THREADS, MAX_EDITS_PER_THREAD);
	const uint editOffset = editsPerThread * GI;
	for (uint i = editOffset; i < min(editOffset + editsPerThread, g_Parameters.SDFEditCount); i++)
	{
		// Test edit i
		if (true)
		{
			localIndices[localEditCount++] = i;
		}
	}

	// Add local count to group count
	uint localIndexOffset = 0;
	InterlockedAdd(gs_EditCount, localEditCount, localIndexOffset);

	GroupMemoryBarrierWithGroupSync();

	if (GI == 0)
	{
		// An atomic counter is used to place the edits into the index buffer
		g_IndexCounter.InterlockedAdd(0, gs_EditCount, gs_IndexOffset);

		gs_Brick.IndexOffset = gs_EditCount;
		gs_Brick.IndexCount = gs_IndexOffset;
		g_Bricks[GroupID.x] = gs_Brick;
	}

	GroupMemoryBarrierWithGroupSync();

	// Place the indices into the index buffer
	for (uint i = 0; i < localEditCount; i++)
	{
		g_IndexBuffer[gs_IndexOffset + localIndexOffset + i] = localIndices[i];
	}
}

#endif