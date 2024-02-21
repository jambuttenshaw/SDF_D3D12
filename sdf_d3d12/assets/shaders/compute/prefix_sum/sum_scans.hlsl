#ifndef SUMSCANS_HLSL
#define SUMSCANS_HLSL


StructuredBuffer<uint> g_InCountTable : register(t0);
ByteAddressBuffer g_NumberOfCounts : register(t1);

RWStructuredBuffer<uint> g_BlockSumTable : register(u0);
RWStructuredBuffer<uint> g_BlockSumOutputTable : register(u1);

RWStructuredBuffer<uint> g_OutPrefixSumTable : register(u2);


#define SCAN_GROUP_THREADS 64

groupshared uint gs_BlockSum;


[numthreads(SCAN_GROUP_THREADS, 1, 1)]
void main(uint3 GroupID : SV_GroupID, uint GI : SV_GroupIndex, uint DTid : SV_DispatchThreadID)
{
	const uint numCounts = g_NumberOfCounts.Load(0);
	if (DTid.x >= numCounts)
		return;

	if (GI == 0)
	{
		gs_BlockSum = g_BlockSumOutputTable[GroupID.x];

		uint x = 63;
		while (x < GroupID.x)
		{
			gs_BlockSum += g_BlockSumOutputTable[x];
			x += 64;
		}
	}

	GroupMemoryBarrierWithGroupSync();

	// Add the block sum onto the individual sum
	const uint prefixSum = gs_BlockSum + g_OutPrefixSumTable[DTid.x];
	g_OutPrefixSumTable[DTid.x] = prefixSum;
}

#endif
