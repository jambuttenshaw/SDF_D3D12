#ifndef EDITTESTER_HLSL
#define EDITTESTER_HLSL


#include "../include/sdf_helper.hlsli"

#define HLSL
#include "../../../src/Renderer/Hlsl/ComputeHlslCompat.h"

#define EDIT_TESTING_THREADS 32


ConstantBuffer<BrickBuildParametersConstantBuffer> g_BuildParameters : register(b0);

StructuredBuffer<SDFEditData> g_EditList : register(t0);
StructuredBuffer<uint> g_InIndexBuffer : register(t1);

RWStructuredBuffer<Brick> g_Bricks : register(u0);

RWStructuredBuffer<uint> g_OutIndexBuffer : register(u1);
RWByteAddressBuffer g_OutIndexCounter : register(u2);


#define MAX_EDITS_PER_THREAD 32 // how many indices each thread can store


// The brick that this group will process
groupshared Brick gs_Brick;
// The number of edits affecting this brick
groupshared uint gs_EditCount;
// The offset into the index buffer where this bricks indices are stored
groupshared uint gs_IndexOffset;


float EvaluateEdit(SDFEditData edit, float3 p)
{
	// apply primitive transform
	const float3 p_transformed = opTransform(p, edit.InvWorldMat) / edit.Scale;
		
	// evaluate primitive
	float dist = sdPrimitive(p_transformed, GetShape(edit.Primitive), edit.ShapeParams);
	dist *= edit.Scale;

	return dist;
}


[numthreads(EDIT_TESTING_THREADS, 1, 1)]
void main(uint3 GroupID : SV_GroupID, uint GI : SV_GroupIndex)
{
	if (GI == 0)
	{
		// Init GSM
		gs_Brick = g_Bricks.Load(GroupID.x);
		gs_IndexOffset = 0;
		gs_EditCount = 0;
	}

	GroupMemoryBarrierWithGroupSync();

	// Each thread will process an edit and determine if its relevant
	const uint editsPerThread = min((gs_Brick.IndexCount + EDIT_TESTING_THREADS - 1) / EDIT_TESTING_THREADS, MAX_EDITS_PER_THREAD);
	// Which index of the brick to begin processing edits from
	const uint threadIndexOffset = editsPerThread * GI;

	uint localEditCount = 0;
	uint localIndices[MAX_EDITS_PER_THREAD];

	for (uint i = threadIndexOffset; i < min(threadIndexOffset + editsPerThread, gs_Brick.IndexCount); i++)
	{
		// Test edit i

		// Get the index of i and load the edit
		const uint index = g_InIndexBuffer.Load(gs_Brick.IndexOffset + i);
		const SDFEditData edit = g_EditList.Load(index);

		// Evaluate edit
		const float3 brickCentre = gs_Brick.TopLeft + 0.5f * g_BuildParameters.SubBrickSize; // This stage is executed AFTER sub-brick building - so bricks are now sub-brick sized
		if (EvaluateEdit(edit, brickCentre) < g_BuildParameters.SubBrickSize) // Edit is relevant
		{
			localIndices[localEditCount++] = index;
		}
	}

	// Add local count to group count  
	// ReSharper disable once CppLocalVariableMayBeConst
	uint localIndexOffset = 0;
	if (localEditCount > 0)
	{
		InterlockedAdd(gs_EditCount, localEditCount, localIndexOffset);
	}

	GroupMemoryBarrierWithGroupSync();

	if (GI == 0)
	{
		// An atomic counter is used to place the edits into the index buffer
		g_OutIndexCounter.InterlockedAdd(0, gs_EditCount, gs_IndexOffset);

		// Update the brick with its new index buffer
		gs_Brick.IndexOffset = gs_IndexOffset;
		gs_Brick.IndexCount = gs_EditCount;
		g_Bricks[GroupID.x] = gs_Brick;
	}

	GroupMemoryBarrierWithGroupSync();

	// Place the indices into the index buffer
	for (uint i = 0; i < localEditCount; i++)
	{
		g_OutIndexBuffer[gs_IndexOffset + localIndexOffset + i] = localIndices[i];
	}
}

#endif