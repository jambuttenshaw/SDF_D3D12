#ifndef BRICKCOUNTSCAN_HLSL
#define BRICKCOUNTSCAN_HLSL


StructuredBuffer<uint> g_InCountTable : register(t0);
ByteAddressBuffer g_NumberOfCounts : register(t1);

RWStructuredBuffer<uint> g_BlockSumTable : register(u0); // A buffer containing the prefix sums of each entire block
RWStructuredBuffer<uint> g_ScannedBlockTable : register(u1); // A table of the prefix sums of the block sum table

RWStructuredBuffer<uint> g_OutPrefixSumTable : register(u2);


#define SCAN_GROUP_THREADS 64


groupshared uint gs_Table[SCAN_GROUP_THREADS];
groupshared uint gs_BlockSum;
// The last value to be processed by this group needs to be passed up to the next group
// So we will add it on to the sum sent to the next stage
groupshared uint gs_Last;

[numthreads(SCAN_GROUP_THREADS, 1, 1)]
void scan_blocks(uint3 GroupID : SV_GroupID, uint GI : SV_GroupIndex, uint DTid : SV_DispatchThreadID)
{
	const uint numCounts = g_NumberOfCounts.Load(0);
	if (DTid.x > numCounts)
		return;

	if (GI != 0)
	{
		gs_Table[GI] = g_InCountTable[DTid.x - 1];
	}
	else
	{
		gs_Table[GI] = 0;
		gs_Last = g_InCountTable[(GroupID.x + 1) * SCAN_GROUP_THREADS - 1];
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
	
	// Store the prefix in the buffer - it will be needed in the 3rd step
	g_OutPrefixSumTable[DTid.x] = gs_Table[GI];

	if (GI == 0)
	{
		// the last element in the table will contain the prefix sum of the entire group
		g_BlockSumTable[GroupID.x] = gs_Table[SCAN_GROUP_THREADS - 1] + gs_Last;
	}
}


[numthreads(SCAN_GROUP_THREADS, 1, 1)]
void scan_block_sums(uint GI : SV_GroupIndex, uint DTid : SV_DispatchThreadID)
{
	const uint numCounts = g_NumberOfCounts.Load(0);
	if (DTid.x > uint(ceil(numCounts / 64.0f)))
		return;

	if (GI != 0)
	{
		gs_Table[GI] = g_BlockSumTable[DTid.x - 1];
	}
	else
	{
		gs_Table[GI] = 0;
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
	g_ScannedBlockTable[DTid.x] = gs_Table[GI];
}


[numthreads(SCAN_GROUP_THREADS, 1, 1)]
void sum_scans(uint3 GroupID : SV_GroupID, uint GI : SV_GroupIndex, uint DTid : SV_DispatchThreadID)
{
	const uint numCounts = g_NumberOfCounts.Load(0);
	if (DTid.x > numCounts)
		return;

	if (GI == 0)
	{
		gs_BlockSum = g_ScannedBlockTable[GroupID.x];
	}

	GroupMemoryBarrierWithGroupSync();

	// Add the block sum onto the individual sum
	const uint prefixSum = gs_BlockSum + g_OutPrefixSumTable[DTid.x];
	g_OutPrefixSumTable[DTid.x] = prefixSum;
}

#endif
