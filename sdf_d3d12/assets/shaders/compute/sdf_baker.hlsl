#ifndef SDFBAKER_HLSL
#define SDFBAKER_HLSL

/*
 *
 *	This shader takes a buffer of edits to evaluate and stores the result in a 3D Texture
 *
 */

#define HLSL
#include "../../../src/Renderer/Hlsl/ComputeHlslCompat.h"
#include "../include/sdf_primitives.hlsli"
#include "../include/sdf_operations.hlsli"


ConstantBuffer<BakeDataConstantBuffer> g_BakeData : register(b0);
StructuredBuffer<EditData> g_EditList : register(t0);

RWTexture3D<float> g_OutputTexture : register(u0);


float EvaluateEditList(float3 p)
{
	// Evaluate SDF list
	float4 nearest = float4(0.0f, 0.0f, 0.0f, FLOAT_MAX);
	
	for (uint i = 0; i < g_BakeData.PrimitiveCount; i++)
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


float FormatDistance(float inDistance, float volumeDimensions)
{
	// Texture format is 8-bit unsigned normalized
	// Values range from 0 to 1, with 256 unique values being able to be stored in the texture

	// Need to decide how to map distances such that the largest steps can be taken when sphere tracing
	// But distances must still be conservative, otherwise the sphere-tracing will intersect with geometry

	// In Claybook, a distance value of 1 means 4 voxels
	// They used a signed normalized format, so they had [-4,+4] range with 256 values to represent it
	// giving 1/32 voxel precision

	// The space the SDFs were evaluated ranged from [-1,1]
	// Therefore a value of 2 = volume resolution

	// Calculate the distance value in terms of voxels
	// This will be in the range [0, volume resolution]
	const float voxelDistance = max(0.0f, min(volumeDimensions, inDistance * volumeDimensions / 2));

	// Now map the distance such that 1 = maxDistance
	return saturate(voxelDistance / g_BakeData.VolumeStride);
}


#define NUM_SHADER_THREADS 8
[numthreads(NUM_SHADER_THREADS, NUM_SHADER_THREADS, NUM_SHADER_THREADS)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	// Make sure thread has a pixel in the texture to process
	uint3 dims;
	g_OutputTexture.GetDimensions(dims.x, dims.y, dims.z);
	if (DTid.x >= dims.x || DTid.y >= dims.y || DTid.z >= dims.z)
		return;

	// Calculate position to evaluate this thread

	// Step 1: let p range [0,1] through the volume
	float3 p = DTid / (float3) (dims - uint3(1, 1, 1));
	// Transform to [-1,1]
	p = (p * 2.0f) - 1.0f;
	
	const float nearest = EvaluateEditList(p);
	const float mappedDistance = FormatDistance(nearest, dims.x);

	// Store the mapped distance in the volume
	g_OutputTexture[DTid] = mappedDistance;
}

#endif