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
SamplerState g_VolumeSampler : register(s2);


/////////////////////////
//// Local Arguments ////
/////////////////////////

// The SDF volume that will be ray-marched in the intersection shader
ConstantBuffer<BrickPropertiesConstantBuffer> l_BrickProperties : register(b0, space1);

Texture3D l_BrickPool : register(t0, space1);
StructuredBuffer<Brick> l_BrickBuffer : register(t1, space1);

struct MaterialTable
{
	uint4 Table;
};
ConstantBuffer<MaterialTable> l_MaterialTable : register(b1, space1);


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


// Trace a shadow ray and return true if it hits any geometry.
bool TraceShadowRay(in Ray ray, in UINT currentRayRecursionDepth)
{
	if (currentRayRecursionDepth >= MAX_RAY_RECURSION_DEPTH)
	{
		return false;
	}

	// Set the ray's extents.
	RayDesc rayDesc;
	rayDesc.Origin = ray.origin;
	rayDesc.Direction = ray.direction;
	rayDesc.TMin = 0.05f;
	rayDesc.TMax = 10000;

	// Initialize shadow ray payload.
	// Set the initial value to true since closest and any hit shaders are skipped. 
	// Shadow miss shader, if called, will set it to false.
	ShadowRayPayload shadowPayload = { true };
	TraceRay(g_Scene,
		RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH
		| RAY_FLAG_FORCE_OPAQUE // ~skip any hit shaders
		| RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, // ~skip closest hit shaders,
		TraceRayParameters::InstanceMask,
		TraceRayParameters::HitGroup::Offset[RayType_Shadow],
		TraceRayParameters::HitGroup::GeometryStride,
		TraceRayParameters::MissShader::Offset[RayType_Shadow],
		rayDesc, shadowPayload);

	return shadowPayload.hit;
}


float3 ComputeSurfaceNormal(float3 p, float3 delta)
{
	return normalize(float3(
        (l_BrickPool.SampleLevel(g_VolumeSampler, p + float3(delta.x, 0.0f, 0.0f), 0).x - l_BrickPool.SampleLevel(g_VolumeSampler, p - float3(delta.x, 0.0f, 0.0f), 0).x),
        (l_BrickPool.SampleLevel(g_VolumeSampler, p + float3(0.0f, delta.y, 0.0f), 0).x - l_BrickPool.SampleLevel(g_VolumeSampler, p - float3(0.0f, delta.y, 0.0f), 0).x),
        (l_BrickPool.SampleLevel(g_VolumeSampler, p + float3(0.0f, 0.0f, delta.z), 0).x - l_BrickPool.SampleLevel(g_VolumeSampler, p - float3(0.0f, 0.0f, delta.z), 0).x)
    ));
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
void PrimaryRaygenShader()
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
	RadianceRayPayload payload = { float4(0, 0, 0, 0), 0 };
	TraceRay(g_Scene, RAY_FLAG_NONE, ~0, 0, 1, 0, ray, payload);

	//payload.color.xyz = pow(payload.color.xyz, 0.4545f);

    // Write the raytraced color to the output texture.
	g_RenderTarget[DispatchRaysIndex().xy] = payload.color;
}


/////////////////////////////
//// INTERSECTION SHADERS////
/////////////////////////////

[shader("intersection")]
void SDFIntersectionShader()
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
		SDFIntersectAttrib attr;
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
		const float3 brickVoxel = (uvwAABB * SDF_BRICK_SIZE_VOXELS) + 1.0f;


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
		float3 uvwMin = BrickVoxelToPoolUVW(1.0f, brickTopLeftVoxel, uvwPerVoxel);
		float3 uvwMax = BrickVoxelToPoolUVW(SDF_BRICK_SIZE_VOXELS + 1.0f, brickTopLeftVoxel, uvwPerVoxel);

		// This is a vector type as uvw range in each direction might not be uniform
		// This is the case in non-cubic brick pools
		const float3 stride_voxelToUVW = (uvwMax - uvwMin) / SDF_BRICK_SIZE_VOXELS;


		// step through volume to find surface
		uint iterationCount = 0;
		float4 s;

		while (iterationCount < 32) // iteration guard
		{
			// Sample the volume
			s = l_BrickPool.SampleLevel(g_VolumeSampler, uvw, 0);
			
			// 0.0625 was the largest threshold before unacceptable artifacts were produced
			if (s.x <= 0.0625f)
			{
				break;
			}

			uvw += (s.x * SDF_VOLUME_STRIDE * stride_voxelToUVW) // Convert from formatted distance in texture to a distance in the uvw space of the brick pool
					* ray.direction;
			 
			if (uvw.x > uvwMax.x || uvw.y > uvwMax.y || uvw.z > uvwMax.z ||
				uvw.x < uvwMin.x || uvw.y < uvwMin.y || uvw.z < uvwMin.z)
			{
				// Exited box: no intersection
				return;
			}
			iterationCount++;
		}

		attr.materials = s.yzw;

		// Calculate the hit point as a UVW of the AABB
		const float3 hitUVWAABB = (PoolUVWToBrickVoxel(uvw, brickTopLeftVoxel, poolDims) - float3(1.0f, 1.0f, 1.0f)) / SDF_BRICK_SIZE_VOXELS;
		// point of intersection in local space
		const float3 pointOfIntersection = hitUVWAABB * l_BrickProperties.BrickSize;
		// t is the distance from the ray origin to the point of intersection
		const float newT = length(ray.origin - pointOfIntersection);

		// Transform from object space to world space
		attr.normal = normalize(mul(ComputeSurfaceNormal(uvw, 0.5f * uvwPerVoxel), transpose((float3x3) ObjectToWorld())));
		attr.utility = (g_PassCB.Flags & RENDER_FLAG_DISPLAY_BRICK_EDIT_COUNT) ? brick.IndexCount : iterationCount;
		ReportHit(newT, 0, attr);
	}
}


/////////////////////////////
//// CLOSEST HIT SHADERS ////
/////////////////////////////

[shader("closesthit")]
void SDFClosestHitShader(inout RadianceRayPayload payload, in SDFIntersectAttrib attr)
{
	// DEBUG MODES
	if (g_PassCB.Flags & (RENDER_FLAG_DISPLAY_BRICK_EDIT_COUNT | RENDER_FLAG_DISPLAY_HEATMAP | RENDER_FLAG_DISPLAY_BRICK_INDEX))
	{
		const float i = saturate((attr.utility - 1.0f) / g_PassCB.HeatmapQuantization);
		const float hue = g_PassCB.HeatmapHueRange * (1.0f - i);

		payload.color = float4(RGBFromHSV(float3(hue, 1, 1)), 1.0f);
		return;
	}
	if (g_PassCB.Flags & (RENDER_FLAG_DISPLAY_NORMALS | RENDER_FLAG_DISPLAY_BOUNDING_BOX))
	{
		float3 normalColor;
		if (attr.flags & INTERSECTION_FLAG_NO_REMAP_NORMALS)
			normalColor = attr.normal;
		else
			normalColor = 0.5f + 0.5f * attr.normal;
		payload.color = float4(normalColor, 1);
		return;
	}

	// Load material
	MaterialGPUData materials[4];
	materials[0] = g_Materials.Load(l_MaterialTable.Table.x);
	materials[1] = g_Materials.Load(l_MaterialTable.Table.y);
	materials[2] = g_Materials.Load(l_MaterialTable.Table.z);
	materials[3] = g_Materials.Load(l_MaterialTable.Table.w);

	// Get material interpolation parameters
	float4 t = float4(attr.materials, 1.0f - (attr.materials.x + attr.materials.y + attr.materials.z));

	// Material blending
	const MaterialGPUData mat = blendMaterial(blendMaterial(blendMaterial(blendMaterial(materials[3], t.w), materials[2], t.z), materials[1], t.y), materials[0], t.x);

	const float3 hitPos = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
	const float3 v = normalize(g_PassCB.WorldEyePos - hitPos);

	Ray shadowRay;
	shadowRay.origin = hitPos;
	shadowRay.direction = -g_PassCB.Light.Direction;

	const bool shadowRayHit = g_PassCB.Flags & RENDER_FLAG_DISABLE_SHADOW
								? false
								: TraceShadowRay(shadowRay, payload.recursionDepth);

	// Lighting
	float3 lightColor = calculateLighting(
		g_PassCB.Flags,
		attr.normal,
		v,
		shadowRayHit,
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


/////////////////////
//// MISS SHADERS////
/////////////////////

[shader("miss")]
void PrimaryMissShader(inout RadianceRayPayload payload)
{
	payload.color = g_PassCB.Flags & RENDER_FLAG_DISABLE_SKYBOX
					? float4(0, 0, 0.2f, 1)
					: g_EnvironmentMap.SampleLevel(g_EnvironmentSampler, WorldRayDirection(), 0);
}


[shader("miss")]
void ShadowMissShader(inout ShadowRayPayload payload)
{
	payload.hit = false;
}

#endif // RAYTRACING_HLSL