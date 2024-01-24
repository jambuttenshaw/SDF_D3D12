#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

#define HLSL
#include "../../../src/Renderer/Hlsl/RaytracingHlslCompat.h"

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


float3 ComputeSurfaceNormal(float3 p, float delta)
{
	return normalize(float3(
        (l_SDFVolume.SampleLevel(g_Sampler, p + float3(delta, 0.0f, 0.0f), 0) - l_SDFVolume.SampleLevel(g_Sampler, p - float3(delta, 0.0f, 0.0f), 0)),
        (l_SDFVolume.SampleLevel(g_Sampler, p + float3(0.0f, delta, 0.0f), 0) - l_SDFVolume.SampleLevel(g_Sampler, p - float3(0.0f, delta, 0.0f), 0)),
        (l_SDFVolume.SampleLevel(g_Sampler, p + float3(0.0f, 0.0f, delta), 0) - l_SDFVolume.SampleLevel(g_Sampler, p - float3(0.0f, 0.0f, delta), 0))
    ));
}


// Calculates which voxel in the brick pool this thread will map to
uint3 CalculateBrickPoolPosition(uint brickIndex, uint3 brickPoolDimensions)
{
	// For now bricks are stored linearly
	uint3 brickTopLeft;

	const uint bricksX = brickPoolDimensions.x / SDF_BRICK_SIZE_VOXELS_ADJACENCY;
	const uint bricksY = brickPoolDimensions.y / SDF_BRICK_SIZE_VOXELS_ADJACENCY;

	brickTopLeft.x = brickIndex % bricksX;
	brickIndex /= bricksX;
	brickTopLeft.y = brickIndex % bricksY;
	brickIndex /= bricksY;
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


float3 RGBFromHue(float hue)
{
	const float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
	const float3 p = abs(frac(hue.xxx + K.xyz) * 6.0 - K.www);
	return saturate(p - K.xxx);
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
		const float3 uvwPerVoxel = 1.0f / (float3)poolDims;

		// if we are inside the aabb, begin ray marching from t = 0
		// otherwise, begin from where the view ray first hits the box
		float3 uvwAABB = ray.origin + max(tMin, 0.0f) * ray.direction;
		// map uvw to range [-1,1]
		uvwAABB /= pBrick.AABBHalfExtent;
		// map to [0,1]
		uvwAABB = uvwAABB * 0.5f + 0.5f;

		// get voxel coordinate of top left of brick
		const uint3 brickTopLeftVoxel = CalculateBrickPoolPosition(pBrick.BrickIndex, poolDims);

		// Offset by 1 due to adjacency data
		// e.g., uvwAABB of (0, 0, 0) actually references the voxel at (1, 1, 1) - not (0, 0, 0)
		const float3 brickVoxel = (uvwAABB * SDF_BRICK_SIZE_VOXELS) + 1.0f;


		// Debug: Display the bounding box directly instead of sphere tracing the contents
		if (g_PassCB.Flags & RENDER_FLAG_DISPLAY_BOUNDING_BOX)
		{ // Display AABB
			MyAttributes attr;

			// Debug: Display the brick index that this bounding box would sphere trace
			if (g_PassCB.Flags & RENDER_FLAG_DISPLAY_BRICK_INDEX)
			{
				// Some method of turning the index into a color
				attr.normal = RGBFromHue(frac(pBrick.BrickIndex / 64.0f));
				attr.remap = false;
			}
			// Debug: Display the brick pool uvw of the intersection with the surface of the box
			else if (g_PassCB.Flags & RENDER_FLAG_DISPLAY_POOL_UVW)
			{
				attr.normal = BrickVoxelToPoolUVW(brickVoxel, brickTopLeftVoxel, uvwPerVoxel);
				attr.remap = false;
			}
			else
			{
				attr.normal = uvwAABB;
				attr.remap = false;
			}
			
			attr.heatmap = 0;
			ReportHit(max(tMin, RayTMin()), 0, attr);
			return;
		}


		// Get the pool UVW
		float3 uvw = BrickVoxelToPoolUVW(brickVoxel, brickTopLeftVoxel, uvwPerVoxel);
		float3 uvwMin = BrickVoxelToPoolUVW(1.0f, brickTopLeftVoxel, uvwPerVoxel);
		float3 uvwMax = BrickVoxelToPoolUVW(SDF_BRICK_SIZE_VOXELS + 1.0f, brickTopLeftVoxel, uvwPerVoxel);

		const float stride_voxelToUVW = (uvwMax - uvwMin).x / SDF_BRICK_SIZE_VOXELS;


		// step through volume to find surface
		uint iterationCount = 0;
		while (iterationCount < 32) // iteration guard
		{
			// Sample the volume
			float s = l_SDFVolume.SampleLevel(g_Sampler, uvw, 0);

			// 0.0625 was the largest threshold before unacceptable artifacts were produced
			if (s <= 0.0625f)
				break;

			// Remap s
			s *= SDF_VOLUME_STRIDE * stride_voxelToUVW;
			uvw += s * ray.direction;
			 
			if (uvw.x > uvwMax.x || uvw.y > uvwMax.y || uvw.z > uvwMax.z ||
				uvw.x < uvwMin.x || uvw.y < uvwMin.y || uvw.z < uvwMin.z)
			{
				// Exited box: no intersection
				return;
			}
			iterationCount++;
		}

		// Calculate the hit point as a UVW of the AABB
		float3 hitUVWAABB = (PoolUVWToBrickVoxel(uvw, brickTopLeftVoxel, poolDims) - 1.0f) / SDF_BRICK_SIZE_VOXELS;
		// point of intersection in local space
		const float3 pointOfIntersection = (hitUVWAABB * 2.0f - 1.0f) * pBrick.AABBHalfExtent;
		// t is the distance from the ray origin to the point of intersection
		const float newT = length(ray.origin - pointOfIntersection);

		MyAttributes attr;
		// Transform from object space to world space
		attr.normal = normalize(mul(ComputeSurfaceNormal(uvw, 0.5f * uvwPerVoxel), transpose((float3x3) ObjectToWorld())));
		attr.heatmap = iterationCount;
		attr.remap = true;
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
		float3 normalColor;
		if (attr.remap)
			normalColor = 0.5f + 0.5f * attr.normal;
		else
			normalColor = attr.normal;
		payload.color = float4(normalColor, 1);
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