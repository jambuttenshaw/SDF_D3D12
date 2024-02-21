#ifndef EDITDEPENDENCY_HLSL
#define EDITDEPENDENCY_HLSL


#include "../include/sdf_helper.hlsli"

#define HLSL
#include "../HlslCompat/ComputeHlslCompat.h"


ConstantBuffer<EditDependencyParameters> g_Parameters : register(b0);

RWStructuredBuffer<SDFEditData> g_EditList : register(u0);
RWStructuredBuffer<uint16_t> g_DependencyIndices : register(u1);


float EvaluateBoundingSphere(SDFEditData edit, float3 p)
{
	// apply primitive transform
	const float3 p_transformed = opTransform(p, edit.InvRotation, edit.InvTranslation) / edit.Scale;
		
	// evaluate primitive
	float dist = boundingSpherePrimitive(p_transformed, GetShape(edit.PrimitivesAndDependencies), edit.ShapeParams);
	dist *= edit.Scale;

	// Apply blending range for smooth edits
	dist -= IsSmoothPrimitive(edit.PrimitivesAndDependencies) * edit.BlendingRange;

	return dist;
}


[numthreads(EDIT_DEPENDENCY_THREAD_COUNT, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	if (DTid.x >= g_Parameters.DependencyPairsCount)
		return;

	// Work out which pair of edits that this thread will compare

	// Index A will be the largest triangular number less than DTid
	// Index B will be DTid - Index A

	// Largest triangular number less than a natural number:
	// https://math.stackexchange.com/questions/1417579/largest-triangular-number-less-than-a-given-natural-number.

	// This formula will calculate pairs of indices such that every possible dependent pair will be processed
	// In this table, the column is indexA, row indexB, and value is DTid.x
	//
	//   | 0 1 2 3 4 
	// --+-----------
	// 0 | x 0 1 3 6 
	// 1 | x x 2 4 7
	// 2 | x x x 5 8
	// 3 | x x x x 9
	// 4 | x x x x x

	const float x = 0.5f * (1.0f + sqrt(8.0f * DTid.x + 2.0f));
	const uint indexA = floor(x);
	const uint indexB = floor(indexA * frac(x));

	// Load editA, and only process if it is a smooth edit
	// Hard edits will not have dependencies
	const SDFEditData editA = g_EditList.Load(indexA);

	if (IsSmoothPrimitive(editA.PrimitivesAndDependencies))
	{
		// get the world-space position of this edit
		const float3 pA = -editA.InvTranslation;
		const float dA = EvaluateBoundingSphere(editA, pA);

		// evaluate the distance to edit2
		const SDFEditData edit2 = g_EditList.Load(indexB);
		const float d2 = EvaluateBoundingSphere(edit2, pA);
		// Check if they are close enough to be dependent
		if (d2 - dA <= editA.BlendingRange)
		{
			uint dependencies;
			// 0x00010000 is added as dependencies is stored in the upper 16 bits of the variable
			InterlockedAdd(g_EditList[indexA].PrimitivesAndDependencies, 0x00010000, dependencies);

			const uint dependencyIndex = dependencies >> 16;
			g_DependencyIndices[indexA + dependencyIndex] = (uint16_t)(indexB);
		}
	}
}

#endif