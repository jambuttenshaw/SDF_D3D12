#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

#define HLSL
#include "../../../src/Renderer/Hlsl/RaytracingHlslCompat.h"
#include "../../../src/Renderer/Hlsl/ComputeHlslCompat.h"

#include "ray_helper.hlsli"


//////////////////////////
//// Global Arguments ////
//////////////////////////

RaytracingAccelerationStructure g_Scene : register(t0);
RWTexture2D<float4> g_RenderTarget : register(u0);

ConstantBuffer<PassConstantBuffer> g_PassCB : register(b0);

// This is a static bilinear sampler
SamplerState g_Sampler : register(s0);


/////////////////////////
//// Local Arguments ////
/////////////////////////

// The SDF volume that will be ray-marched in the intersection shader
Texture3D<float> l_SDFVolume : register(t0, space1);
StructuredBuffer<BrickPointer> l_BrickBuffer : register(t1, space1);


/////////////////
//// DEFINES ////
/////////////////

#define EPSILON 1.0f / 256.0f // the smallest value representable in an R8_UNORM format

#define LIGHT_DIRECTION normalize(float3(0.5f, 1.0f, -1.0f))
#define LIGHT_AMBIENT float3(0.2f, 0.2f, 0.2f)
#define LIGHT_DIFFUSE float3(1.0f, 1.0f, 1.0f)


//////////////////////////
//// HELPER FUNCTIONS ////
//////////////////////////

// Generate a ray in world space for a camera pixel corresponding to an index from the dispatched 2D grid.
inline void GenerateCameraRay(uint2 index, out float3 origin, out float3 direction)
{
	float2 xy = index + 0.5f; // center in the middle of the pixel.
	float2 screenPos = xy / DispatchRaysDimensions().xy * 2.0 - 1.0;

    // Invert Y for DirectX-style coordinates.
	screenPos.y = -screenPos.y;

    // Unproject the pixel coordinate into a ray.
	float4 world = mul(float4(screenPos, 0, 1), g_PassCB.InvViewProj);

	world.xyz /= world.w;
	origin = g_PassCB.WorldEyePos;
	direction = normalize(world.xyz - origin);
}


float3 ComputeSurfaceNormal(float3 p)
{
	const float invRes = 0.01f;
	return normalize(float3(
        (l_SDFVolume.SampleLevel(g_Sampler, p + float3(invRes, 0.0f, 0.0f), 0) - l_SDFVolume.SampleLevel(g_Sampler, p - float3(invRes, 0.0f, 0.0f), 0)),
        (l_SDFVolume.SampleLevel(g_Sampler, p + float3(0.0f, invRes, 0.0f), 0) - l_SDFVolume.SampleLevel(g_Sampler, p - float3(0.0f, invRes, 0.0f), 0)),
        (l_SDFVolume.SampleLevel(g_Sampler, p + float3(0.0f, 0.0f, invRes), 0) - l_SDFVolume.SampleLevel(g_Sampler, p - float3(0.0f, 0.0f, invRes), 0))
    ));
}


// Calculates which voxel in the brick pool this thread will map to
uint3 CalculateBrickPoolPosition(uint brickIndex, uint3 brickPoolDimensions)
{
	// For now bricks are stored linearly
	uint3 brickTopLeft;

	const uint bricksX = brickPoolDimensions.x / SDF_BRICK_SIZE_IN_VOXELS;
	const uint bricksY = brickPoolDimensions.y / SDF_BRICK_SIZE_IN_VOXELS;

	brickTopLeft.x = brickIndex % (bricksX + 1);
	brickIndex /= (bricksX + 1);
	brickTopLeft.y = brickIndex % (bricksY + 1);
	brickIndex /= (bricksY + 1);
	brickTopLeft.z = brickIndex;

	return brickTopLeft * SDF_BRICK_SIZE_IN_VOXELS;
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


////////////////////////
//// RAYGEN SHADERS ////
////////////////////////

[shader("raygeneration")]
void MyRaygenShader()
{
	float3 rayDir;
	float3 origin;
    
    // Generate a ray for a camera pixel corresponding to an index from the dispatched 2D grid.
	GenerateCameraRay(DispatchRaysIndex().xy, origin, rayDir);

    // Trace the ray.
    // Set the ray's extents.
	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = rayDir;
    // Set TMin to a non-zero small value to avoid aliasing issues due to floating - point errors.
    // TMin should be kept small to prevent missing geometry at close contact areas.
	ray.TMin = 0.001f;
	ray.TMax = 10000.0f;
	RayPayload payload = { float4(0, 0, 0, 0) };
	TraceRay(g_Scene, RAY_FLAG_NONE, ~0, 0, 1, 0, ray, payload);

    // Write the raytraced color to the output texture.
	g_RenderTarget[DispatchRaysIndex().xy] = payload.color;
}


/////////////////////////////
//// INTERSECTION SHADERS////
/////////////////////////////

[shader("intersection")]
void MyIntersectionShader()
{
	BrickPointer pBrick = l_BrickBuffer[PrimitiveIndex()];

	Ray ray;
	ray.origin = ObjectRayOrigin() - pBrick.AABBCentre;
	ray.direction = ObjectRayDirection();

	float3 aabb[2];
	aabb[1] = float3(pBrick.AABBHalfExtent, pBrick.AABBHalfExtent, pBrick.AABBHalfExtent);
	aabb[0] = -aabb[1];

	// Get the tmin and tmax of the intersection between the ray and this aabb
	float tMin, tMax;
	if (RayAABBIntersectionTest(ray, aabb, tMin, tMax))
	{
		// The dimensions of the pool texture are required to convert from voxel coordinates to uvw for sampling
		uint3 poolDims;
		l_SDFVolume.GetDimensions(poolDims.x, poolDims.y, poolDims.z);
		float3 uvwPerVoxel = 1.0f / (float3)poolDims;

		// if we are inside the aabb, begin ray marching from t = 0
		// otherwise, begin from where the view ray first hits the box
		float3 uvw = ray.origin + max(tMin, 0.0f) * ray.direction;
		// map uvw to range [-1,1]
		uvw /= pBrick.AABBHalfExtent;
		// map to [0,1]
		uvw = uvw * 2.0f - 1.0f;

		if (g_PassCB.Flags & RENDER_FLAG_DISPLAY_BOUNDING_BOX)
		{ // Display AABB
			MyAttributes attr;
			attr.normal = uvw;
			attr.heatmap = 0;
			ReportHit(max(tMin, RayTMin()), 0, attr);
			return;
		}

		// get voxel coordinate of top left of brick
		const uint3 brickTopLeftVoxel = CalculateBrickPoolPosition(pBrick.BrickIndex, poolDims);

		float3 voxel = uvw * SDF_BRICK_SIZE_IN_VOXELS;

		// calculate voxel boundaries

		// step through volume to find surface
		uint iterationCount = 0;

		// 0.0625 was the largest threshold before unacceptable artifacts were produced
		while (iterationCount < 32)
		{
			// Sample the volume
			float s = l_SDFVolume.SampleLevel(g_Sampler, BrickVoxelToPoolUVW(voxel, brickTopLeftVoxel, uvwPerVoxel), 0);

			if (s <= 0.0625f)
				break;

			// Remap s
			s *= SDF_VOLUME_STRIDE;
			voxel += s * ray.direction;
			 
			//if (uvw.x > uvwMax.x || uvw.y > uvwMax.y || uvw.z > uvwMax.z ||
			//	uvw.x < uvwMin.x || uvw.y < uvwMin.y || uvw.z < uvwMin.z)
			if (uvw.x > SDF_BRICK_SIZE_IN_VOXELS || uvw.y > SDF_BRICK_SIZE_IN_VOXELS || uvw.z > SDF_BRICK_SIZE_IN_VOXELS ||
				uvw.x < 0 || uvw.y < 0 || uvw.z < 0)
			{
				// Exited box: no intersection
				return;
			}
			iterationCount++;
		}
		// point of intersection in local space
		const float3 pointOfIntersection = (uvw * 2.0f - 1.0f) * pBrick.AABBHalfExtent;
		// t is the distance from the ray origin to the point of intersection
		const float newT = length(ray.origin - pointOfIntersection);

		MyAttributes attr;
		// Transform from object space to world space
		attr.normal = normalize(mul(ComputeSurfaceNormal(uvw), transpose((float3x3) ObjectToWorld())));
		attr.heatmap = iterationCount;
		ReportHit(newT, 0, attr);
	}
}


/////////////////////////////
//// CLOSEST HIT SHADERS ////
/////////////////////////////

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
	if (g_PassCB.Flags & RENDER_FLAG_DISPLAY_HEATMAP)
	{
		payload.color = float4(attr.heatmap / 8.0f, attr.heatmap / 16.0f, attr.heatmap / 32.0f, 1.0f);
	}
	else if (g_PassCB.Flags & (RENDER_FLAG_DISPLAY_NORMALS | RENDER_FLAG_DISPLAY_BOUNDING_BOX))
	{
		payload.color = float4(0.5f + 0.5f * attr.normal, 1);
	}
	else
	{
		// Simple phong lighting with directional light
		const float irradiance = max(0.0f, dot(attr.normal, LIGHT_DIRECTION));
		const float3 lightColor = LIGHT_AMBIENT + irradiance * LIGHT_DIFFUSE;
		payload.color = float4(lightColor, 1.0f);
	}
}


/////////////////////
//// MISS SHADERS////
/////////////////////

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
	payload.color = float4(0, 0, 0.2f, 1);
}

#endif // RAYTRACING_HLSL