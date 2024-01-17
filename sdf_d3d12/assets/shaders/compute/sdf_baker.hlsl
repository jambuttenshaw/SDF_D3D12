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
// Constant Parameters
// 

// constants
static const float D = 30.0f;
static const float epsilon = 0.001f;

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
	
	float3 p = DTid / (float3) (dims - uint3(1, 1, 1));
	p = (p * 2.0f) - 1.0f;
	
	// Calculate aspect ratio
	float3 fDims = (float3) dims;
	float maxDim = max(max(fDims.x, fDims.y), fDims.z);
	float3 aspectRatio = fDims / maxDim;
	
	// Apply aspect ratio of texture
	p *= aspectRatio;
	
	// Evaluate SDF list
	float4 nearest = float4(0.0f, 0.0f, 0.0f, FLOAT_MAX);
	
	for (uint i = 0; i < g_BakeData.PrimitiveCount; i++)
	{
		// apply primitive transform
		float3 p_transformed = opTransform(p, g_PrimitiveData[i].InvWorldMat) / g_PrimitiveData[i].Scale;
		
		// evaluate primitive
		float4 shape = float4(
			g_PrimitiveData[i].Color.rgb,
			sdPrimitive(p_transformed, g_PrimitiveData[i].Shape, g_PrimitiveData[i].ShapeParams)
		);
		shape.w *= g_PrimitiveData[i].Scale;

		// combine with scene
		nearest = opPrimitive(nearest, shape, g_PrimitiveData[i].Operation, g_PrimitiveData[i].BlendingFactor);
	}
	
	g_OutputTexture[DTid] = nearest.w;
}

#endif