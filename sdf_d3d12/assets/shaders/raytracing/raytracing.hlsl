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

ConstantBuffer<VolumeConstantBuffer> l_VolumeCB : register(b0, space1);
StructuredBuffer<AABBPrimitiveData> l_PrimitiveBuffer : register(t1, space1);


/////////////////
//// DEFINES ////
/////////////////

#define EPSILON 1.0f / 256.0f // the smallest value representable in an R8_UNORM format


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
	const float invRes = l_VolumeCB.InvVolumeDimensions;
	return normalize(float3(
        l_SDFVolume.SampleLevel(g_Sampler, p + float3(invRes, 0.0f, 0.0f), 0) - l_SDFVolume.SampleLevel(g_Sampler, p - float3(invRes, 0.0f, 0.0f), 0),
        l_SDFVolume.SampleLevel(g_Sampler, p + float3(0.0f, invRes, 0.0f), 0) - l_SDFVolume.SampleLevel(g_Sampler, p - float3(0.0f, invRes, 0.0f), 0),
        l_SDFVolume.SampleLevel(g_Sampler, p + float3(0.0f, 0.0f, invRes), 0) - l_SDFVolume.SampleLevel(g_Sampler, p - float3(0.0f, 0.0f, invRes), 0)
    ));
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
	AABBPrimitiveData prim = l_PrimitiveBuffer[PrimitiveIndex()];

	Ray ray;
	ray.origin = ObjectRayOrigin() - prim.AABBCentre.xyz;
	ray.direction = ObjectRayDirection();

	float3 aabb[2];
	aabb[1] = float3(prim.AABBHalfExtent, prim.AABBHalfExtent, prim.AABBHalfExtent);
	aabb[0] = -aabb[1];

	// Get the tmin and tmax of the intersection between the ray and this aabb
	float tMin, tMax;
	if (RayAABBIntersectionTest(ray, aabb, tMin, tMax))
	{
		const float halfBoxExtent = 0.5f * abs(aabb[0] - aabb[1]).x;

		// if we are inside the aabb, begin ray marching from t = 0
		// otherwise, begin from where the view ray first hits the box
		float3 uvw = ray.origin + max(tMin, 0.0f) * ray.direction;

		// map uvw to range [-1,1]
		uvw /= halfBoxExtent;

		if (g_PassCB.Flags & RENDER_FLAG_DISPLAY_BOUNDING_BOX)
		{ // Display AABB
			MyAttributes attr;
			attr.normal = uvw;
			attr.heatmap = 0;
			ReportHit(max(tMin, RayTMin()), 0, attr);
			return;
		}

		// Remap uvw from [-1,1] to [UVWMin,UVWMax]
		float3 uvwMin = prim.UVW;
		float3 uvwMax = prim.UVW + prim.UVWExtent;
		uvw = 0.5f * (uvw * (uvwMax - uvwMin) + uvwMax + uvwMin);

		// Add a small amount of padding to the uvw bounds to hide the aabb edges
		uvwMin -= l_VolumeCB.UVWVolumeStride;
		uvwMax += l_VolumeCB.UVWVolumeStride;

		// step through volume to find surface
		uint iterationCount = 0;
		while (true)
		{
			// Sample the volume
			float s = l_SDFVolume.SampleLevel(g_Sampler, uvw, 0);

			// Remap s
			s *= l_VolumeCB.UVWVolumeStride;

			if (s <= l_VolumeCB.InvVolumeDimensions)
				break;

			uvw += s * ray.direction;
			 
			if (uvw.x > uvwMax.x || uvw.y > uvwMax.y || uvw.z > uvwMax.z ||
				uvw.x < uvwMin.x || uvw.y < uvwMin.y || uvw.z < uvwMin.z)
			{
				// Exited box: no intersection
				return;
			}
			iterationCount++;
		}
		// point of intersection in local space
		const float3 pointOfIntersection = (uvw * 2.0f - 1.0f) * halfBoxExtent;
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
	else
	{
		payload.color = float4(0.5f + 0.5f * attr.normal, 1);
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