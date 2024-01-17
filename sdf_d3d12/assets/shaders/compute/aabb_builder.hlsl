#ifndef AABBBUILDER_HLSL
#define AABBBUILDER_HLSL

#define HLSL
#include "../../../src/Renderer/Hlsl/ComputeHlslCompat.h"
#include "../../../src/Renderer/Hlsl/RaytracingHlslCompat.h"

/*
 *
 *	This shader takes a 3D texture containing distance values and builds a buffer of AABB's around the iso-surface
 *
 */

// These must match those defined in C++
#define NUM_SHADER_THREADS 8


ConstantBuffer<AABBBuilderParametersConstantBuffer> g_BuildParameters : register(b0);
Texture3D<float> g_SDFVolume : register(t0);

RWByteAddressBuffer g_Counter : register(u0);
RWStructuredBuffer<AABB> g_AABBBuffer : register(u1);
RWStructuredBuffer<AABBPrimitiveData> g_PrimitiveDataBuffer : register(u2);

SamplerState g_Sampler : register(s0);


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
	const float3 uvwCentre = (DTid + 0.5f) * g_BuildParameters.UVWIncrement;
	const float3 uvwMax = (DTid + 1.0f) * g_BuildParameters.UVWIncrement;

	{
		// Sample the volume at uvw
		float distance = g_SDFVolume.SampleLevel(g_Sampler, uvwCentre, 0);

		// Check if this distance value is large enough for this AABB to be omitted

		// Basically, define a conservative upper bound distance where every distance
		// value smaller would mean that this AABB definitely contains geometry

		// This distance is half the long diagonal of the AABB
		float diagonal = 0.5f * length(uvwMax - uvwMin);
		if (distance > diagonal)
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
	primitiveData.AABBMin = float4(outAABB.TopLeft, 1);
	primitiveData.AABBMax = float4(outAABB.BottomRight, 1);
	primitiveData.AABBCentre = float4(0.5f * (outAABB.TopLeft + outAABB.BottomRight), 1.0f);

	primitiveData.UVWMin = float4(uvwMin, 0.0f);
	primitiveData.UVWMax = float4(uvwMax, 0.0f);
	
	
	g_PrimitiveDataBuffer[aabbIndex] = primitiveData;
}
#endif