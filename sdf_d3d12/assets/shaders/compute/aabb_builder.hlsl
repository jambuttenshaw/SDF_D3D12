#ifndef SUBBRICKCOUNTER_HLSL
#define SUBBRICKCOUNTER_HLSL


#define HLSL
#include "../../../src/Renderer/Hlsl/ComputeHlslCompat.h"
#include "../../../src/Renderer/Hlsl/RaytracingHlslCompat.h"


ConstantBuffer<AABBBuilderConstantBuffer> g_BuilderCB : register(b0);
StructuredBuffer<Brick> g_Bricks : register(t0);

RWStructuredBuffer<AABB> g_AABBs : register(u0);


[numthreads(AABB_BUILDING_THREADS, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	// Exit if there isn't a brick to consume
	if (DTid.x > g_BuilderCB.BrickCount)
	{
		return;
	}

	const Brick brick = g_Bricks.Load(DTid.x);

	// Build a raytracing AABB from this brick
	AABB outAABB;
	outAABB.TopLeft = brick.TopLeft_EvalSpace;
	outAABB.BottomRight = brick.TopLeft_EvalSpace + g_BuilderCB.BrickSize;

	g_AABBs[DTid.x] = outAABB;
}

#endif