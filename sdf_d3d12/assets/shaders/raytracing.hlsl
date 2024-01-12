#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

#define HLSL
#include "../../src/Renderer/Hlsl/RaytracingHlslCompat.h"

#include "ray_helper.hlsli"


RaytracingAccelerationStructure g_Scene : register(t0, space0);
RWTexture2D<float4> g_RenderTarget : register(u0);

ConstantBuffer<PassConstantBuffer> g_PassCB : register(b0);
StructuredBuffer<PrimitiveInstancePerFrameBuffer> g_InstanceBuffer : register(t1);

SamplerState g_Sampler : register(s0);

// The SDF volume that will be ray-marched in the intersection shader
// This is a local root argument
Texture3D<float> l_SDFVolume : register(t2);


#define NORMAL_DELTA_X float3(0.01f, 0.0f, 0.0f)
#define NORMAL_DELTA_Y float3(0.0f, 0.01f, 0.0f)
#define NORMAL_DELTA_Z float3(0.0f, 0.0f, 0.01f)


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
	PrimitiveInstancePerFrameBuffer attr = g_InstanceBuffer[InstanceID()];

    // Retrieve a ray origin position and direction in bottom level AS space 
    // and transform them into the AABB primitive's local space.
	Ray ray;
	ray.origin = mul(float4(ObjectRayOrigin(), 1), attr.BottomLevelASToLocalSpace).xyz;
	ray.direction = mul(ObjectRayDirection(), (float3x3) attr.BottomLevelASToLocalSpace);
	return ray;
}


float3 ComputeSurfaceNormal(float3 p)
{
	return normalize(float3(
        l_SDFVolume.SampleLevel(g_Sampler, p + NORMAL_DELTA_X, 0) - l_SDFVolume.SampleLevel(g_Sampler, p - NORMAL_DELTA_X, 0),
        l_SDFVolume.SampleLevel(g_Sampler, p + NORMAL_DELTA_Y, 0) - l_SDFVolume.SampleLevel(g_Sampler, p - NORMAL_DELTA_Y, 0),
        l_SDFVolume.SampleLevel(g_Sampler, p + NORMAL_DELTA_Z, 0) - l_SDFVolume.SampleLevel(g_Sampler, p - NORMAL_DELTA_Z, 0)
    ));
}


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
	ray.TMin = 0.001;
	ray.TMax = 10000.0;
	RayPayload payload = { float4(0, 0, 0, 0) };
	TraceRay(g_Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);

    // Write the raytraced color to the output texture.
	g_RenderTarget[DispatchRaysIndex().xy] = payload.color;
}

[shader("intersection")]
void MyIntersectionShader()
{
	const Ray ray = GetRayInAABBPrimitiveLocalSpace();

	float3 aabb[2];
	float tMin, tMax;
	{
		const PrimitiveInstancePerFrameBuffer attr = g_InstanceBuffer[InstanceID()];
		aabb[0] = mul(g_InstanceBuffer[InstanceID()].AABBMin, attr.BottomLevelASToLocalSpace).xyz;
		aabb[1] = mul(g_InstanceBuffer[InstanceID()].AABBMax, attr.BottomLevelASToLocalSpace).xyz;
	}

	// Get the tmin and tmax of the intersection between the ray and this aabb
	if (RayAABBIntersectionTest(ray, aabb, tMin, tMax))
	{
		float3 uvw = ray.origin + tMin * ray.direction;
		// Remap uvw to [0,1]
		uvw *= 0.5f;
		uvw += 0.5f;

		// step through volume to find surface
		float s = 1; // can be any number > epsilon
	
		while (s > 0.001f)
		{
			s = l_SDFVolume.SampleLevel(g_Sampler, uvw, 0);
			tMin += 0.001f;
			uvw += 0.001f * ray.direction;

		
			if (max(max(uvw.x, uvw.y), uvw.z) >= 1.0001f || min(min(uvw.x, uvw.y), uvw.z) <= -0.0001f)
			{
				// Exited box: no intersection
				return;
			}
		}

		MyAttributes attr;
		attr.normal = ComputeSurfaceNormal(uvw);
		ReportHit(tMin, 0, attr);
	}
}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
	payload.color = float4(0.5f + 0.5f * attr.normal, 1);
}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
	payload.color = float4(0, 0, 0.2f, 1);
}

#endif // RAYTRACING_HLSL