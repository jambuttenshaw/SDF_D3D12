#ifndef SUBBRICKCOUNTER_HLSL
#define SUBBRICKCOUNTER_HLSL


#define HLSL
#include "../HlslCompat/ComputeHlslCompat.h"
#include "../HlslCompat/RaytracingHlslCompat.h"

#include "../include/brick_helper.hlsli"


ConstantBuffer<BrickEvaluationConstantBuffer> g_BuilderCB : register(b0);
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
	outAABB.TopLeft = brick.TopLeft;
	outAABB.BottomRight = brick.TopLeft + g_BuilderCB.EvalSpace_BrickSize;

	g_AABBs[DTid.x] = outAABB;
}

#endif