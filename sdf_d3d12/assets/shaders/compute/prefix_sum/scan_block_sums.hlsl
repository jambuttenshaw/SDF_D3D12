#ifndef SCANBLOCKSUMS_HLSL
#define SCANBLOCKSUMS_HLSL


StructuredBuffer<uint> g_InCountTable : register(t0);
ByteAddressBuffer g_NumberOfCounts : register(t1);

RWStructuredBuffer<uint> g_BlockSumTable : register(u0);
RWStructuredBuffer<uint> g_BlockSumOutputTable : register(u1);

RWStructuredBuffer<uint> g_OutPrefixSumTable : register(u2);


#define SCAN_GROUP_THREADS 64


groupshared uint gs_Table[SCAN_GROUP_THREADS];
groupshared uint gs_BlockCarry;


[numthreads(SCAN_GROUP_THREADS, 1, 1)]
void main(uint GI : SV_GroupIndex, uint DTid : SV_DispatchThreadID)
{
	const uint numCounts = g_NumberOfCounts.Load(0);
	if (DTid.x > uint(floor(numCounts / 64.0f)))
		return;

	if (GI > 0)
	{
		gs_Table[GI] = g_BlockSumTable[DTid.x - 1];
	}
	else
	{
		gs_Table[GI] = 0;
		gs_BlockCarry = DTid.x > 0 ? g_BlockSumTable[DTid.x - 1] : 0;
	}

	GroupMemoryBarrierWithGroupSync();

	for (uint i = 1; i < SCAN_GROUP_THREADS; i <<= 1)
	{
		uint temp;
		if (GI > i)
		{
			temp = gs_Table[GI] + gs_Table[GI - i];
		}
		else
		{
			temp = gs_Table[GI];
		}
		GroupMemoryBarrierWithGroupSync();
		gs_Table[GI] = temp;
		GroupMemoryBarrierWithGroupSync();
	}

	// Store the prefix sum for this element in the output
	g_BlockSumOutputTable[DTid.x] = gs_Table[GI] + gs_BlockCarry;
}

#endif
