#ifndef BRICKHELPER_H
#define BRICKHELPER_H

#define HLSL
#include "../../../src/Renderer/Hlsl/HlslDefines.h"


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