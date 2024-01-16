#ifndef RAYHELPER_H
#define RAYHELPER_H

#define HLSL
#include "../../../src/Renderer/Hlsl/HlslCompat.h"


bool IsInRange(in float val, in float min, in float max)
{
	return (val >= min && val <= max);
}

bool IsAValidHit(in float thit)
{
	return IsInRange(thit, RayTMin(), RayTCurrent());
}

bool RayAABBIntersectionTest(Ray ray, float3 aabb[2], out float tmin, out float tmax)
{
	float3 tmin3, tmax3;
	int3 sign3 = ray.direction > 0;

	float3 invRayDirection = 1 / ray.direction;

	tmin3.x = (aabb[1 - sign3.x].x - ray.origin.x) * invRayDirection.x;
	tmax3.x = (aabb[sign3.x].x - ray.origin.x) * invRayDirection.x;

	tmin3.y = (aabb[1 - sign3.y].y - ray.origin.y) * invRayDirection.y;
	tmax3.y = (aabb[sign3.y].y - ray.origin.y) * invRayDirection.y;
    
	tmin3.z = (aabb[1 - sign3.z].z - ray.origin.z) * invRayDirection.z;
	tmax3.z = (aabb[sign3.z].z - ray.origin.z) * invRayDirection.z;
    
	tmin = max(max(tmin3.x, tmin3.y), tmin3.z);
	tmax = min(min(tmax3.x, tmax3.y), tmax3.z);
    
	return tmax > tmin && tmax >= RayTMin() && tmin <= RayTCurrent();
}

#endif
