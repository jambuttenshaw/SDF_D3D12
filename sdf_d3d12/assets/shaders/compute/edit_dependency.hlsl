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
RWStructuredBuffer<UINT> g_DependencyIndices : register(u1);


[numthreads(EDIT_DEPENDENCY_THREAD_COUNT, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	if (DTid.x >= g_Parameters.SDFEditCount)
		return;

	SDFEditData edit = g_EditList.Load(DTid.x);
	uint dependencies = 0;

	const uint baseIndex = DTid.x * (DTid.x - 1) / 2;

	for (uint i = 0; i < DTid.x; i++)
	{
		bool overlaps;
		if (true)
		{
			g_DependencyIndices[baseIndex + dependencies] = i;
			dependencies++;
		}
	}

	// Store dependencies in the upper 16 bits
	// There can be a max of 1023 dependencies for any one edit - only 10 bits required
	edit.PrimitivesAndDependencies |= (dependencies << 16) & 0xFFFF0000;
	g_EditList[DTid.x] = edit;
}

#endif