#ifndef EDITDEPENDENCY_HLSL
#define EDITDEPENDENCY_HLSL


#include "../include/sdf_helper.hlsli"

#define HLSL
#include "../../../src/Renderer/Hlsl/ComputeHlslCompat.h"


struct Parameters
{
	UINT SDFEditCount;
};
ConstantBuffer<Parameters> g_Parameters : register(b0);

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
	if (DTid.x >= g_Parameters.SDFEditCount)
		return;

	SDFEditData edit1 = g_EditList.Load(DTid.x);
	uint dependencies = 0;

	if (IsSmoothPrimitive(edit1.PrimitivesAndDependencies))
	{
		const uint baseIndex = DTid.x * (DTid.x - 1) / 2;

		// get the world-space position of this edit
		const float3 p1 = -edit1.InvTranslation;
		const float d1 = EvaluateBoundingSphere(edit1, p1);

		for (uint i = 0; i < DTid.x; i++)
		{
			// Compare against another edit to determine if they overlap
			// The bounding sphere is used 
			const SDFEditData edit2 = g_EditList.Load(i);

			const float d2 = EvaluateBoundingSphere(edit2, p1);
			if (d2 - d1 <= edit1.BlendingRange)
			{
				g_DependencyIndices[baseIndex + dependencies] = (uint16_t)(i);
				dependencies++;
			}
		}
	}

	// Store dependencies in the upper 16 bits
	// There can be a max of 1023 dependencies for any one edit - only 10 bits required
	edit1.PrimitivesAndDependencies |= (dependencies << 16) & 0xFFFF0000;
	g_EditList[DTid.x] = edit1;
}

#endif