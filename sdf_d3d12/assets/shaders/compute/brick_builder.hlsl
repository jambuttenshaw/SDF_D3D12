#ifndef SDFBAKER_HLSL
#define SDFBAKER_HLSL

/*
 *
 *	This shader takes a buffer of edits to evaluate and stores the result in a 3D Texture
 *
 */

#define HLSL
#include "../../../src/Renderer/Hlsl/ComputeHlslCompat.h"
#include "../../../src/Renderer/Hlsl/RaytracingHlslCompat.h"
#include "../include/sdf_primitives.hlsli"
#include "../include/sdf_operations.hlsli"


ConstantBuffer<SDFBuilderConstantBuffer> g_BuildParameters : register(b0);
StructuredBuffer<SDFEditData> g_EditList : register(t0);

StructuredBuffer<AABBPrimitiveData> g_PrimitiveData : register(t1);
RWTexture3D<float> g_OutputTexture : register(u1);


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


float FormatDistance(float inDistance, float UVWExtent)
{
	// Calculate the distance value in terms of voxels
	const float voxelsPerUnit = 0.5f * SDF_BRICK_SIZE / UVWExtent;
	const float voxelDistance = inDistance * voxelsPerUnit;

	// Now map the distance such that 1 = maxDistance
	// The formatted distance will be clamped to [-1, 1] when it is written to the brick
	return voxelDistance / SDF_VOLUME_STRIDE;
}


[numthreads(SDF_BRICK_SIZE, SDF_BRICK_SIZE, SDF_BRICK_SIZE)]
void main(uint3 GroupID : SV_GroupID, uint3 GTid : SV_GroupThreadID)
{
	// TODO: Eventually this will be replaced as thread ID will no longer map to volume coordinates once bricks are stored in a brick pool
	uint3 dims;
	g_OutputTexture.GetDimensions(dims.x, dims.y, dims.z);
	
	// Get the bounding box that this group will fill
	const AABBPrimitiveData primitiveData = g_PrimitiveData[GroupID.x];

	// Calculate position to evaluate this thread
	// Step 1: Get UVW of this thread
	float3 uvw = primitiveData.UVW + primitiveData.UVWExtent * (float3) GTid / (float) SDF_BRICK_SIZE;
	// Save the voxel that this thread is evaluating
	const uint3 voxel = uvw * dims;

	// Step 2: Remap to [-1,1]
	uvw *= 2.0f;
	uvw -= 1.0f;

	// Evaluate SDF volume
	const float nearest = EvaluateEditList(uvw);
	const float mappedDistance = FormatDistance(nearest, primitiveData.UVWExtent);

	// Store the mapped distance in the volume
	g_OutputTexture[voxel] = mappedDistance;
}

#endif