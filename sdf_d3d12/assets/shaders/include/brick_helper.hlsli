#ifndef BRICKHELPER_H
#define BRICKHELPER_H

#define HLSL
#include "../../../src/Renderer/Hlsl/HlslDefines.h"


// float3 (in range [0,1]) -> 30-bit Morton code
// From https://developer.nvidia.com/blog/thinking-parallel-part-iii-tree-construction-gpu/

// Expands a 10-bit integer into 30 bits
// by inserting 2 zeros after each bit.
uint expandBits(unsigned int v)
{
	v = (v * 0x00010001u) & 0xFF0000FFu;
	v = (v * 0x00000101u) & 0x0F00F00Fu;
	v = (v * 0x00000011u) & 0xC30C30C3u;
	v = (v * 0x00000005u) & 0x49249249u;
	return v;
}

// Calculates a 30-bit Morton code for the
// given 3D point located within the unit cube [0,1].
uint morton3Df(float3 p)
{
	p.x = min(max(p.x * 1024.0f, 0.0f), 1023.0f);
	p.y = min(max(p.y * 1024.0f, 0.0f), 1023.0f);
	p.z = min(max(p.z * 1024.0f, 0.0f), 1023.0f);
	unsigned int xx = expandBits((unsigned int) p.x);
	unsigned int yy = expandBits((unsigned int) p.y);
	unsigned int zz = expandBits((unsigned int) p.z);
	return xx * 4 + yy * 2 + zz;
}

// Calculates a 30-bit Morton code for the
// given 3D point located within the range [0,1023].
uint morton3Du(uint3 p)
{
	p.x = min(p.x, 1023u);
	p.y = min(p.y, 1023u);
	p.z = min(p.z, 1023u);
	unsigned int xx = expandBits(p.x);
	unsigned int yy = expandBits(p.y);
	unsigned int zz = expandBits(p.z);
	return xx * 4 + yy * 2 + zz;
}

// Takes a 30 bit integer and compacts it into 10 bits
// By removing ever 2nd and 3rd bit
uint compactBits(uint x)
{
	x &= 0x09249249;
	x = (x ^ (x >> 2)) & 0x030c30c3;
	x = (x ^ (x >> 4)) & 0x0300f00f;
	x = (x ^ (x >> 8)) & 0xff0000ff;
	x = (x ^ (x >> 16)) & 0x000003ff;
	return x;
}

uint3 decodeMorton3D(uint v)
{
	uint x = compactBits(v >> 2);
	uint y = compactBits(v >> 1);
	uint z = compactBits(v);
	return uint3(x, y, z);
}


// Takes a distance from evaluation space and then maps it to a distance to be stored in a brick
// Voxels per unit can be calculated from the evaluation range and brick size
float FormatDistance(float inDistance, float voxelsPerUnit)
{
	// Calculate the distance value in terms of voxels
	const float voxelDistance = inDistance * voxelsPerUnit;

	// Now map the distance such that 1 = SDF_VOLUME_STRIDE number of voxels
	return voxelDistance / SDF_VOLUME_STRIDE;
}


// Calculates which voxel in the brick pool this thread will map to
uint3 CalculateBrickPoolPosition(uint brickIndex, uint brickCount, uint3 brickPoolCapacity)
{
	// max size for component = ceil(log2(brickCount) / 3)
	// normalized = (decodeMorton3D(index) / (max size for component))
	// brickTopLeft = normalized * (capacity - 1)

	//uint3 brickTopLeft = decodeMorton3D(brickIndex);

	//const float maxComponentSize = float(1U << uint(ceil(log2(brickCount) / 3.0f)));
	//uint3 brickTopLeft = (decodeMorton3D(brickIndex) / maxComponentSize) * (brickPoolCapacity - 1);
	
	uint3 brickTopLeft;
	
	brickTopLeft.x = brickIndex % brickPoolCapacity.x;
	brickIndex /= brickPoolCapacity.x;
	brickTopLeft.y = brickIndex % brickPoolCapacity.y;
	brickIndex /= brickPoolCapacity.y;
	brickTopLeft.z = brickIndex;

	return brickTopLeft * SDF_BRICK_SIZE_VOXELS_ADJACENCY;
}

// Maps from a voxel within a brick to its uvw coordinate within the entire brick pool
// Uses float3 instead of uint3 to maintain sub-voxel precision
float3 BrickVoxelToPoolUVW(float3 voxel, float3 brickTopLeft, float3 uvwPerVoxel)
{
	return (brickTopLeft + voxel) * uvwPerVoxel;
}

// Maps from a uvw coordinate within the entire brick pool to its voxel within a brick 
// Uses float3 instead of uint3 to maintain sub-voxel precision
float3 PoolUVWToBrickVoxel(float3 uvw, float3 brickTopLeft, uint3 brickPoolDims)
{
	return (uvw * brickPoolDims) - brickTopLeft;
}


#endif