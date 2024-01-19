#ifndef AABBBUILDER_HLSL
#define AABBBUILDER_HLSL

#define HLSL
#include "../../../src/Renderer/Hlsl/ComputeHlslCompat.h"
#include "../../../src/Renderer/Hlsl/RaytracingHlslCompat.h"

#include "../include/sdf_primitives.hlsli"
#include "../include/sdf_operations.hlsli"

/*
 *
 *	This shader takes a 3D texture containing distance values and builds a buffer of AABB's around the iso-surface
 *
 */

ConstantBuffer<AABBBuilderConstantBuffer> g_BuildParameters : register(b0);
StructuredBuffer<SDFEditData> g_EditList : register(t0);

RWByteAddressBuffer g_Counter : register(u0);
RWStructuredBuffer<AABB> g_AABBBuffer : register(u1);
RWStructuredBuffer<AABBPrimitiveData> g_PrimitiveDataBuffer : register(u2);


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


#define NUM_SHADER_THREADS 8
[numthreads(NUM_SHADER_THREADS, NUM_SHADER_THREADS, NUM_SHADER_THREADS)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	// Map thread to a region of the volume
	// The thread will determine if the volume is empty in that region
	// If the volume is not empty, then it will append an AABB for that region to the append buffer

	// Work out if this thread is in range
	// Each thread is mapped to one AABB
	if (max(DTid.x, max(DTid.y, DTid.z)) >= g_BuildParameters.Divisions)
		return;

	// Get UV coordinate in the volume for this thread to sample
	const float3 uvwMin = (DTid) * g_BuildParameters.UVWIncrement;

	{
		// Check if this distance value is large enough for this AABB to be omitted

		// Basically, define a conservative upper bound distance where every distance
		// value smaller would mean that this AABB definitely contains geometry

		const float3 uvwCentre = uvwMin + 0.5f * g_BuildParameters.UVWIncrement;
		// Remap to [-1, 1]
		const float3 p = (2.0f * uvwCentre) - 1.0f;

		// Evaluate object at this point
		const float distance = EvaluateEditList(p);
		// If distance is larger than the size of this voxel then this voxel cannot contain geometry
		if (distance > 1.74f /* sqrt(3) */ * g_BuildParameters.UVWIncrement)
			return;
	}


	// AABB positions should be such that the centre of the volume is at the origin
	AABB outAABB;
	outAABB.TopLeft = (DTid - 0.5f * g_BuildParameters.Divisions) * g_BuildParameters.AABBDimensions;
	outAABB.BottomRight = outAABB.TopLeft + g_BuildParameters.AABBDimensions;

	// Use the counter to get the index in  the aabb buffer that this thread will operate on
	// Only do this if this thread is going to add a box
	uint aabbIndex;
	g_Counter.InterlockedAdd(0, 1, aabbIndex);

	// Populate AABB buffer
	g_AABBBuffer[aabbIndex] = outAABB;

	// Build primitive data
	AABBPrimitiveData primitiveData;

	primitiveData.AABBCentre = 0.5f * (outAABB.TopLeft + outAABB.BottomRight);
	primitiveData.AABBHalfExtent = 0.5f * g_BuildParameters.AABBDimensions;

	primitiveData.UVW = uvwMin;
	primitiveData.UVWExtent = g_BuildParameters.UVWIncrement;

	g_PrimitiveDataBuffer[aabbIndex] = primitiveData;
}

#endif