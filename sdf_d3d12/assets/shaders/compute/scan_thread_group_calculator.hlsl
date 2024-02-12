#ifndef SCANTHREADGROUPCALCULATOR_HLSL
#define SCANTHREADGROUPCALCULATOR_HLSL

struct IndirectDispatchArguments
{
	uint ThreadGroupX;
	uint ThreadGroupY;
	uint ThreadGroupZ;
};

ByteAddressBuffer g_SubBrickCounter : register(t0);
//
//	index 0 -> number of groups of scan_blocks to dispatch
//	index 1 -> number of groups of fist dispatch of scan_block_sums to dispatch
//	index 2 -> number of groups of sum_scans to dispatch
//
RWStructuredBuffer<IndirectDispatchArguments> g_IndirectArguments : register(u0);


[numthreads(1, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	//
	// if count < 64, stages 2 and 3 do not need to be dispatched
	//
	// dispatch 2 requires 64 times less groups than stage 1 and 3
	//
	const uint count = g_SubBrickCounter.Load(0);
	const uint threadGroups = (count + 63) / 64;
	
	g_IndirectArguments[0].ThreadGroupX = threadGroups;
	g_IndirectArguments[1].ThreadGroupX = threadGroups > 1 ? (threadGroups + 63) / 64 : 0;
	g_IndirectArguments[2].ThreadGroupX = threadGroups > 1 ? threadGroups : 0;
}

#endif