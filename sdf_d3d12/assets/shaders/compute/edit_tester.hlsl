#ifndef EDITTESTER_HLSL
#define EDITTESTER_HLSL


#include "../include/sdf_helper.hlsli"

#define HLSL
#include "../HlslCompat/ComputeHlslCompat.h"

#define EDIT_TESTING_THREADS 32


ConstantBuffer<BrickBuildParametersConstantBuffer> g_BuildParameters : register(b0);

StructuredBuffer<SDFEditData> g_EditList : register(t0);
StructuredBuffer<uint16_t> g_InIndexBuffer : register(t1);
StructuredBuffer<uint16_t> g_EditDependencyIndices : register(t2);

RWStructuredBuffer<Brick> g_Bricks : register(u0);

RWStructuredBuffer<uint16_t> g_OutIndexBuffer : register(u1);
RWByteAddressBuffer g_OutIndexCounter : register(u2);


// The brick that this group will process
groupshared Brick gs_Brick;
// The offset into the index buffer where this bricks indices are stored
groupshared uint gs_IndexOffset;


// 1024-bit wide mask for indicating edits that apply to this brick
groupshared uint gs_EditMask[32];


// The number of edits per thread
groupshared uint gs_EditCounts[32];
// For scanning the bitmask
groupshared uint gs_SIMDScan[32];



void SetEdit(uint index)
{
	InterlockedOr(gs_EditMask[index / 32], (1u << (index % 32)));
}
bool IsEditSet(uint index)
{
	return gs_EditMask[index / 32] & (1u << (index % 32));
}


float EvaluateEdit(SDFEditData edit, float3 p)
{
	// apply primitive transform
	const float3 p_transformed = opTransform(p, edit.InvRotation, edit.InvTranslation) / edit.Scale;
		
	// evaluate primitive
	float dist = sdPrimitive(p_transformed, GetShape(edit.EditParams), edit.ShapeParams);
	dist *= edit.Scale;

	// Apply blending range for smooth edits
	// 1.74f == sqrt(3) 
	dist -= 1.74f * IsSmoothEdit(edit.EditParams) * edit.BlendingRange;

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
	}

	gs_EditMask[GI] = 0;
	gs_EditCounts[GI] = 0;

	GroupMemoryBarrierWithGroupSync();

	// Each thread will process an edit and determine if its relevant
	const uint editsPerThread = min((gs_Brick.IndexCount + EDIT_TESTING_THREADS - 1) / EDIT_TESTING_THREADS, 32);
	// Which index of the brick to begin processing edits from
	const uint threadIndexOffset = editsPerThread * GI;
	const float3 brickCentre = gs_Brick.TopLeft + 0.5f * g_BuildParameters.SubBrickSize; // This stage is executed AFTER sub-brick building - so bricks are now sub-brick sized

	for (uint i = threadIndexOffset; i < min(threadIndexOffset + editsPerThread, gs_Brick.IndexCount); i++)
	{
		// Test edit i

		// Get the index of i and load the edit
		const uint index = (uint)g_InIndexBuffer.Load(gs_Brick.IndexOffset + i);
		if (IsEditSet(index))
			// If this edit has already been tested then skip (could be due to a smooth edit dependency)
			continue;
		const SDFEditData edit = g_EditList.Load(index);

		// Evaluate edit
		if (EvaluateEdit(edit, brickCentre) < g_BuildParameters.SubBrickSize) // Edit is relevant
		{
			SetEdit(index);

			if (IsSmoothEdit(edit.EditParams))
			{
				// Set all of the smooth edits dependencies too
				const uint offset = index * (index - 1) / 2;
				const uint dependencyCount = GetDependencyCount(edit.EditParams);
				for (uint dependency = 0; dependency < dependencyCount; dependency++)
				{
					SetEdit(g_EditDependencyIndices.Load(offset + dependency));
				}
			}
		}
	}
	
	GroupMemoryBarrierWithGroupSync();

	// Compact the bitmask
	// Each thread can process 32 bits - one UINT
	UINT laneScan[32];
	for (uint bit = 0; bit < 32; bit++)
	{
		const UINT set = (gs_EditMask[GI] >> bit) & 1u;

		laneScan[bit] = gs_EditCounts[GI];
		gs_EditCounts[GI] += set;
	}
	
	GroupMemoryBarrierWithGroupSync();

	// Once the in-lane scan is complete, a SIMD scan is performed to work out the offsets for each lane
	if (GI != 0)
	{
		gs_SIMDScan[GI] = gs_EditCounts[GI - 1];
	}
	else
	{
		gs_SIMDScan[GI] = 0;
	}

	GroupMemoryBarrierWithGroupSync();

	for (uint i = 1; i < 32; i <<= 1)
	{
		uint temp;
		if (GI > i)
		{
			temp = gs_SIMDScan[GI] + gs_SIMDScan[GI - i];
		}
		else
		{
			temp = gs_SIMDScan[GI];
		}
		GroupMemoryBarrierWithGroupSync();
		gs_SIMDScan[GI] = temp;
		GroupMemoryBarrierWithGroupSync();
	}

	GroupMemoryBarrierWithGroupSync();

	if (GI == 0)
	{
		// An atomic counter is used to place the edits into the index buffer
		const uint editCount = gs_SIMDScan[31] + gs_EditCounts[31];
		g_OutIndexCounter.InterlockedAdd(0, editCount, gs_IndexOffset);

		// Update the brick with its new index buffer
		gs_Brick.IndexOffset = gs_IndexOffset;
		gs_Brick.IndexCount = editCount;
		g_Bricks[GroupID.x] = gs_Brick;
	}

	GroupMemoryBarrierWithGroupSync();

	// Place the indices into the index buffer
	for (uint i = 0; i < 32; i++)
	{
		if ((gs_EditMask[GI] >> i) & 1u)
		{
			const uint index = gs_IndexOffset + gs_SIMDScan[GI] + laneScan[i];
			g_OutIndexBuffer[index] = (uint16_t)(32 * GI + i);
		}
	}
}

#endif