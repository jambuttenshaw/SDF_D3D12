#ifndef EDITTESTER_HLSL
#define EDITTESTER_HLSL


#define HLSL
#include "../../../src/Renderer/Hlsl/ComputeHlslCompat.h"

#define EDIT_TESTING_THREADS 32

ByteAddressBuffer g_BrickCounter : register(t0);

RWStructuredBuffer<Brick> g_Bricks : register(u0);
RWStructuredBuffer<uint16_t> g_IndexBuffer : register(u1);


[numthreads(EDIT_TESTING_THREADS, 1, 1)]
void main(uint3 GroupID : SV_GroupID, uint GI : SV_GroupIndex)
{
	
}

#endif