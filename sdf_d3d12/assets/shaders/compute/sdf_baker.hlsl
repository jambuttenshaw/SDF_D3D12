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

#define FLOAT_MAX 3.402823466e+38F

// These must match those defined in C++
#define NUM_SHADER_THREADS 8


ConstantBuffer<BakeDataConstantBuffer> g_BakeData : register(b0);

struct PrimitiveData
{
	matrix InvWorldMat;
	float Scale;
	
	// SDF primitive data
	uint Shape;
	uint Operation;
	float BlendingFactor;

	float4 ShapeParams;

	float4 Color;
};
StructuredBuffer<PrimitiveData> g_PrimitiveData : register(t0);

RWTexture3D<float> g_OutputTexture : register(u0);

//
// UTILITY
//

float map(float value, float min1, float max1, float min2, float max2)
{
	return min2 + (value - min1) * (max2 - min2) / (max1 - min1);
}


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
	
	// Evaluate SDF list
	float4 nearest = float4(0.0f, 0.0f, 0.0f, FLOAT_MAX);
	
	for (uint i = 0; i < g_BakeData.PrimitiveCount; i++)
	{
		// apply primitive transform
		const float3 p_transformed = opTransform(p, g_PrimitiveData[i].InvWorldMat) / g_PrimitiveData[i].Scale;
		
		// evaluate primitive
		float4 shape = float4(
			g_PrimitiveData[i].Color.rgb,
			sdPrimitive(p_transformed, g_PrimitiveData[i].Shape, g_PrimitiveData[i].ShapeParams)
		);
		shape.w *= g_PrimitiveData[i].Scale;

		// combine with scene
		nearest = opPrimitive(nearest, shape, g_PrimitiveData[i].Operation, g_PrimitiveData[i].BlendingFactor);
	}

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
	float voxelDistance = map(nearest.w, 0, 2, 0, dims.x);
	voxelDistance = max(0.0f, min(dims.x, voxelDistance));

	// Now map the distance such that 1 = maxDistance
	const float mappedDistance = saturate(voxelDistance / g_BakeData.VolumeStride);

	// Store the mapped distance in the volume
	g_OutputTexture[DTid] = mappedDistance;
}

#endif