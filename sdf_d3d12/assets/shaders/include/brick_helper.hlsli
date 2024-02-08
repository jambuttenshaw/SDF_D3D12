#ifndef BRICKHELPER_H
#define BRICKHELPER_H

#define HLSL
#include "../../../src/Renderer/Hlsl/HlslDefines.h"


// float3 (in range [0,1]) -> 30-bit Morton code
// From https://developer.nvidia.com/blog/thinking-parallel-part-iii-tree-construction-gpu/

// Expands a 10-bit integer into 30 bits
// by inserting 2 zeros after each bit.
unsigned int expandBits(unsigned int v)
{
	v = (v * 0x00010001u) & 0xFF0000FFu;
	v = (v * 0x00000101u) & 0x0F00F00Fu;
	v = (v * 0x00000011u) & 0xC30C30C3u;
	v = (v * 0x00000005u) & 0x49249249u;
	return v;
}

// Calculates a 30-bit Morton code for the
// given 3D point located within the unit cube [0,1].
unsigned int morton3D(float3 p)
{
	p.x = min(max(p.x * 1024.0f, 0.0f), 1023.0f);
	p.y = min(max(p.y * 1024.0f, 0.0f), 1023.0f);
	p.z = min(max(p.z * 1024.0f, 0.0f), 1023.0f);
	unsigned int xx = expandBits((unsigned int) p.x);
	unsigned int yy = expandBits((unsigned int) p.y);
	unsigned int zz = expandBits((unsigned int) p.z);
	return xx * 4 + yy * 2 + zz;
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
uint3 CalculateBrickPoolPosition(uint brickIndex, uint3 brickPoolCapacity)
{
	// For now bricks are stored linearly
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