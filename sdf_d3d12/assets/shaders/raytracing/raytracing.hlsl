#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

#define HLSL
#include "../HlslCompat/RaytracingHlslCompat.h"

#include "ray_helper.hlsli"
#include "../include/lighting.hlsli"
#include "../include/brick_helper.hlsli"


//////////////////////////
//// Global Arguments ////
//////////////////////////

RaytracingAccelerationStructure g_Scene : register(t0);
RWTexture2D<float4> g_RenderTarget : register(u0);

ConstantBuffer<PassConstantBuffer> g_PassCB : register(b0);
StructuredBuffer<MaterialGPUData> g_Materials : register(t1);

TextureCube g_EnvironmentMap : register(t2);
TextureCube g_IrradianceMap : register(t3);
Texture2D g_BRDFMap : register(t4);
TextureCube g_PrefilterMap : register(t5);

SamplerState g_EnvironmentSampler : register(s0);
SamplerState g_BRDFSampler : register(s1);

SamplerState g_VolumePointSampler : register(s2); // Point sampler to sample normals
SamplerState g_VolumeLinearSampler : register(s3);


/////////////////////////
//// Local Arguments ////
/////////////////////////

// The SDF volume that will be ray-marched in the intersection shader
ConstantBuffer<BrickPropertiesConstantBuffer> l_BrickProperties : register(b0, space1);
Texture3D<float> l_BrickPool : register(t0, space1);
StructuredBuffer<Brick> l_BrickBuffer : register(t1, space1);

/////////////////
//// DEFINES ////
/////////////////

#define EPSILON 0.00390625f // 1 / 256 - the smallest value representable in an R8_UNORM format

#define LIGHT_DIRECTION normalize(float3(0.5f, 1.0f, -1.0f)) // direction to the light
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

float3 ComputeSurfaceNormal(float3 p, float3 delta)
{
	const float2 k = float2(1, -1);

	return normalize(k.xyy * l_BrickPool.SampleLevel(g_VolumeLinearSampler, p + k.xyy * delta, 0) +
                     k.yyx * l_BrickPool.SampleLevel(g_VolumeLinearSampler, p + k.yyx * delta, 0) +
                     k.yxy * l_BrickPool.SampleLevel(g_VolumeLinearSampler, p + k.yxy * delta, 0) +
                     k.xxx * l_BrickPool.SampleLevel(g_VolumeLinearSampler, p + k.xxx * delta, 0));
}

float3 ComputeSurfaceNormal(float3 p, float3 uvwPerVoxel, float3 brickTopLeft, uint3 poolDims)
{
	const float2 k = float2(1, -1);
	const float3 delta = uvwPerVoxel * g_PassCB.NormalSampleDelta;

	const float3 pMin = BrickVoxelToPoolUVW(0.5f, brickTopLeft, uvwPerVoxel);
	const float3 pMax = BrickVoxelToPoolUVW(SDF_BRICK_SIZE_VOXELS_ADJACENCY - 0.5f, brickTopLeft, uvwPerVoxel);

	
	const float3 o0 = clamp(p + k.xyy * delta, pMin, pMax);

	float3 v = PoolUVWToBrickVoxel(o0, brickTopLeft, poolDims);
	// if v is less than 0.5 or greater than 7.5, we must sample from a different brick to get a correct gradient

	const float3 o1 = clamp(p + k.yyx * delta, pMin, pMax);
	const float3 o2 = clamp(p + k.yxy * delta, pMin, pMax);
	const float3 o3 = clamp(p + k.xxx * delta, pMin, pMax);

	return normalize(k.xyy * l_BrickPool.SampleLevel(g_VolumeLinearSampler, o0, 0) +
                     k.yyx * l_BrickPool.SampleLevel(g_VolumeLinearSampler, o1, 0) +
                     k.yxy * l_BrickPool.SampleLevel(g_VolumeLinearSampler, o2, 0) +
                     k.xxx * l_BrickPool.SampleLevel(g_VolumeLinearSampler, o3, 0));
}

float3 RGBFromHSV(float3 input)
{
	float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
	float3 p = abs(frac(input.xxx + K.xyz) * 6.0 - K.www);
	return input.z * lerp(K.xxx, clamp(p - K.xxx, 0.0, 1.0), input.y);
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
	const Brick brick = l_BrickBuffer[PrimitiveIndex()];

	Ray ray;
	ray.origin = ObjectRayOrigin() - brick.TopLeft;
	ray.direction = ObjectRayDirection();

	float3 aabb[2];
	aabb[0] = float3(0.0f, 0.0f, 0.0f);
	aabb[1] = float3(l_BrickProperties.BrickSize, l_BrickProperties.BrickSize, l_BrickProperties.BrickSize);

	// Get the tmin and tmax of the intersection between the ray and this aabb
	float tMin, tMax;
	if (RayAABBIntersectionTest(ray, aabb, tMin, tMax))
	{
		// Intersection attributes to be filled out
		MyAttributes attr;
		attr.flags = INTERSECTION_FLAG_NONE;


		// The dimensions of the pool texture are required to convert from voxel coordinates to uvw for sampling
		uint3 poolDims;
		l_BrickPool.GetDimensions(poolDims.x, poolDims.y, poolDims.z);
		const float3 uvwPerVoxel = 1.0f / (float3)poolDims;

		// if we are inside the aabb, begin ray marching from t = 0
		// otherwise, begin from where the view ray first hits the box
		float3 uvwAABB = ray.origin + max(tMin, 0.0f) * ray.direction;
		// map uvw to range [0,1]
		uvwAABB /= l_BrickProperties.BrickSize;

		// get voxel coordinate of top left of brick
		const uint3 brickTopLeftVoxel = CalculateBrickPoolPosition(PrimitiveIndex(), l_BrickProperties.BrickCount, poolDims / SDF_BRICK_SIZE_VOXELS_ADJACENCY);

		// Offset by 1 due to adjacency data
		// e.g., uvwAABB of (0, 0, 0) actually references the voxel at (1, 1, 1) - not (0, 0, 0)
		const float3 brickVoxel = (uvwAABB * (SDF_BRICK_SIZE_VOXELS - 1.0f)) + 1.5f;


		// Debug: Display the bounding box directly instead of sphere tracing the contents
		if (g_PassCB.Flags & RENDER_FLAG_DISPLAY_BOUNDING_BOX)
		{ // Display AABB
			attr.flags |= INTERSECTION_FLAG_NO_REMAP_NORMALS;
			attr.utility = 0;

			// Debug: Display the brick index that this bounding box would sphere trace
			if (g_PassCB.Flags & RENDER_FLAG_DISPLAY_BRICK_INDEX)
			{
				// Some method of turning the index into a color
				attr.utility = PrimitiveIndex();
			}
			// Debug: Display the brick pool uvw of the intersection with the surface of the box
			else if (g_PassCB.Flags & RENDER_FLAG_DISPLAY_POOL_UVW)
			{
				attr.normal = BrickVoxelToPoolUVW(brickVoxel, brickTopLeftVoxel, uvwPerVoxel);
			}
			else if (g_PassCB.Flags & RENDER_FLAG_DISPLAY_BRICK_EDIT_COUNT)
			{
				attr.utility = brick.IndexCount;
			}
			else
			{
				attr.normal = uvwAABB;
			}
			
			ReportHit(max(tMin, RayTMin()), 0, attr);
			return;
		}

		// Get the pool UVW
		float3 uvw = BrickVoxelToPoolUVW(brickVoxel, brickTopLeftVoxel, uvwPerVoxel);
		float3 uvwMin = BrickVoxelToPoolUVW(1.5f, brickTopLeftVoxel, uvwPerVoxel);
		float3 uvwMax = BrickVoxelToPoolUVW(SDF_BRICK_SIZE_VOXELS + 0.5f, brickTopLeftVoxel, uvwPerVoxel);

		// This is a vector type as uvw range in each direction might not be uniform
		// This is the case in non-cubic brick pools
		const float3 stride_voxelToUVW = (uvwMax - uvwMin) / SDF_BRICK_SIZE_VOXELS;


		// step through volume to find surface
		uint iterationCount = 0;

		while (iterationCount < 32) // iteration guard
		{
			// Sample the volume
			const float s = l_BrickPool.SampleLevel(g_VolumeLinearSampler, uvw, 0);

			// Check for crossing the iso-surface
			if (s <= 0.0f)
			{
				break;
			}

			uvw += (s * SDF_VOLUME_STRIDE * stride_voxelToUVW) // Convert from formatted distance in texture to a distance in the uvw space of the brick pool
					* ray.direction;
			 
			if (uvw.x >= uvwMax.x || uvw.y >= uvwMax.y || uvw.z >= uvwMax.z ||
				uvw.x <= uvwMin.x || uvw.y <= uvwMin.y || uvw.z <= uvwMin.z)
			{
				// Exited box: no intersection
				return;
			}
			iterationCount++;
		}

		// Calculate the hit point as a UVW of the AABB
		const float3 hitVoxel = PoolUVWToBrickVoxel(uvw, brickTopLeftVoxel, poolDims);
		const float3 hitUVWAABB = (hitVoxel - 1.5f) / (SDF_BRICK_SIZE_VOXELS - 1.0f);
		// point of intersection in local space
		const float3 pointOfIntersection = hitUVWAABB * l_BrickProperties.BrickSize;
		// t is the distance from the ray origin to the point of intersection
		const float newT = length(ray.origin - pointOfIntersection);

		// Transform from object space to world space
		const float3 n = ComputeSurfaceNormal(uvw, uvwPerVoxel * g_PassCB.NormalSampleDelta);
		attr.normal = normalize(mul(n, transpose((float3x3) ObjectToWorld())));
		attr.utility = (g_PassCB.Flags & RENDER_FLAG_DISPLAY_BRICK_EDIT_COUNT) ? brick.IndexCount : iterationCount;
		ReportHit(newT, 0, attr);
	}
}


/////////////////////////////
//// CLOSEST HIT SHADERS ////
/////////////////////////////

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
	const float3 hitPos = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();

	// DEBUG MODES
	if (g_PassCB.Flags & (RENDER_FLAG_DISPLAY_BRICK_EDIT_COUNT | RENDER_FLAG_DISPLAY_HEATMAP | RENDER_FLAG_DISPLAY_BRICK_INDEX))
	{
		const float i = saturate((attr.utility - 1.0f) / g_PassCB.HeatmapQuantization);
		const float hue = g_PassCB.HeatmapHueRange * (1.0f - i);

		payload.color = float4(RGBFromHSV(float3(hue, 1, 1)), 1.0f);
	}
	else if (g_PassCB.Flags & (RENDER_FLAG_DISPLAY_NORMALS | RENDER_FLAG_DISPLAY_BOUNDING_BOX))
	{
		float3 normalColor;
		if (attr.flags & INTERSECTION_FLAG_NO_REMAP_NORMALS)
			normalColor = attr.normal;
		else
			normalColor = 0.5f + 0.5f * attr.normal;
		payload.color = float4(normalColor, 1);
	}
	else
	{
		// Load material
		const MaterialGPUData mat = g_Materials.Load(0);

		const float3 v = normalize(g_PassCB.WorldEyePos - hitPos);

		// Lighting
		float3 lightColor = calculateLighting(
			g_PassCB.Flags,
			attr.normal,
			v,
			g_PassCB.Light,
			mat,
			g_IrradianceMap,
			g_BRDFMap,
			g_PrefilterMap,
			g_EnvironmentSampler,
			g_BRDFSampler);

		lightColor /= 1.0f + lightColor;

		payload.color = float4(lightColor, 1.0f);
	}

	payload.color.xyz = pow(payload.color.xyz, 0.4545f);
}


/////////////////////
//// MISS SHADERS////
/////////////////////

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
	//payload.color = float4(0, 0, 0.2f, 1);

	payload.color = g_EnvironmentMap.SampleLevel(g_EnvironmentSampler, WorldRayDirection(), 0);
}

#endif // RAYTRACING_HLSL