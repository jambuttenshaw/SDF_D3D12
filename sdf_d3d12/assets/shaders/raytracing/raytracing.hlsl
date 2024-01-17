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

// Get ray in AABB's local space.
Ray GetRayInAABBPrimitiveLocalSpace()
{
	AABBPrimitiveData prim = l_PrimitiveBuffer[PrimitiveIndex()];

    // Retrieve a ray origin position and direction in bottom level AS space 
    // and transform them into the AABB primitive's local space.
	Ray ray;
	ray.origin = ObjectRayOrigin() - prim.AABBCentre.xyz;
	ray.direction = ObjectRayDirection();
	return ray;
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
	const Ray ray = GetRayInAABBPrimitiveLocalSpace();

	float3 aabb[2];
	float tMin, tMax;
	AABBPrimitiveData prim = l_PrimitiveBuffer[PrimitiveIndex()];
	aabb[0] = (prim.AABBMin - prim.AABBCentre).xyz;
	aabb[1] = (prim.AABBMax - prim.AABBCentre).xyz;

	// Get the tmin and tmax of the intersection between the ray and this aabb
	if (RayAABBIntersectionTest(ray, aabb, tMin, tMax))
	{
		const float stepScale = 0.5f;
		const float halfBoxExtent = 0.5f * abs(aabb[0] - aabb[1]).x;

		// if we are inside the aabb, begin ray marching from t = 0
		// otherwise, begin from where the view ray first hits the box
		float3 uvw = ray.origin + max(tMin, 0.0f) * ray.direction;
		uvw /= halfBoxExtent;

		if (true)
		{ // Display AABB
			MyAttributes attr;
			attr.normal = uvw;
			ReportHit(max(tMin, RayTMin()), 0, attr);
			return;
		}

		// Remap uvw from [-1,1] to [UVWMin,UVWMax]
		const float3 pMin = prim.UVWMin.xyz;
		const float3 pMax = prim.UVWMax.xyz;
		uvw = 0.5f * (uvw * (pMax - pMin) + pMax + pMin);

		// step through volume to find surface
		while (true)
		{
			const float s = l_SDFVolume.SampleLevel(g_Sampler, uvw, 0);
			if (s <= EPSILON)
				break;
			uvw += stepScale * s * ray.direction;
			 
			//if (max(max(uvw.x, uvw.y), uvw.z) >= 1.0f || min(min(uvw.x, uvw.y), uvw.z) <= 0.0f)
			if (uvw.x > pMax.x || uvw.y > pMax.y || uvw.z > pMax.z ||
				uvw.x < pMin.x || uvw.y < pMin.y || uvw.z < pMin.z)
			{
				// Exited box: no intersection
				return;
			}
		}
		// point of intersection in local space
		const float3 pointOfIntersection = (uvw * 2.0f - 1.0f) * halfBoxExtent;
		// t is the distance from the ray origin to the point of intersection
		const float newT = length(ray.origin - pointOfIntersection);

		MyAttributes attr;
		// Transform from object space to world space
		attr.normal = normalize(mul(ComputeSurfaceNormal(uvw), transpose((float3x3) ObjectToWorld())));
		ReportHit(newT, 0, attr);
	}
}


/////////////////////////////
//// CLOSEST HIT SHADERS ////
/////////////////////////////

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
	payload.color = float4(0.5f + 0.5f * attr.normal, 1);
	//payload.color = float4(attr.normal, 1);
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