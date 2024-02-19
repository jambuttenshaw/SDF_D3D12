#ifndef EDITDEPENDENCY_HLSL
#define EDITDEPENDENCY_HLSL


#include "../include/sdf_helper.hlsli"

#define HLSL
#include "../../../src/Renderer/Hlsl/ComputeHlslCompat.h"


#define EDIT_DEPENDENCY_THREAD_COUNT 64


struct Parameters
{
	UINT SDFEditCount;
};
ConstantBuffer<Parameters> g_Parameters : register(b0);

RWStructuredBuffer<SDFEditData> g_EditList : register(t0);
RWStructuredBuffer<UINT> g_DependencyIndices : register(u0);


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
		if (overlaps)
		{
			g_DependencyIndices[baseIndex + dependencies++] = i;
		}
	}

	edit.Dependencies = dependencies;
	g_EditList[DTid.x] = edit;
}

#endif