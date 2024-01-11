#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

#define HLSL
#include "../../src/Renderer/Hlsl/RaytracingHlslCompat.h"


RaytracingAccelerationStructure Scene : register(t0, space0);
RWTexture2D<float4> RenderTarget : register(u0);

ConstantBuffer<PassConstantBuffer> g_passCB : register(b0);
StructuredBuffer<PrimitiveInstancePerFrameBuffer> g_instanceBuffer : register(t1);


// Generate a ray in world space for a camera pixel corresponding to an index from the dispatched 2D grid.
inline void GenerateCameraRay(uint2 index, out float3 origin, out float3 direction)
{
	float2 xy = index + 0.5f; // center in the middle of the pixel.
	float2 screenPos = xy / DispatchRaysDimensions().xy * 2.0 - 1.0;

    // Invert Y for DirectX-style coordinates.
	screenPos.y = -screenPos.y;

    // Unproject the pixel coordinate into a ray.
	float4 world = mul(float4(screenPos, 0, 1), g_passCB.InvViewProj);

	world.xyz /= world.w;
	origin = g_passCB.WorldEyePos;
	direction = normalize(world.xyz - origin);
}

// Get ray in AABB's local space.
Ray GetRayInAABBPrimitiveLocalSpace()
{
	PrimitiveInstancePerFrameBuffer attr = g_instanceBuffer[InstanceID()];

    // Retrieve a ray origin position and direction in bottom level AS space 
    // and transform them into the AABB primitive's local space.
	Ray ray;
	ray.origin = mul(float4(ObjectRayOrigin(), 1), attr.BottomLevelASToLocalSpace).xyz;
	ray.direction = mul(ObjectRayDirection(), (float3x3) attr.BottomLevelASToLocalSpace);
	return ray;
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

float sdSphere(float3 p, float r)
{
	return length(p) - r;
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
	TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);

    // Write the raytraced color to the output texture.
	RenderTarget[DispatchRaysIndex().xy] = payload.color;
}

[shader("intersection")]
void MyIntersectionShader()
{
	const Ray ray = GetRayInAABBPrimitiveLocalSpace();

	float3 aabb[2];
	float tMin, tMax;
	{
		const PrimitiveInstancePerFrameBuffer attr = g_instanceBuffer[InstanceID()];
		aabb[0] = mul(g_instanceBuffer[InstanceID()].AABBMin, attr.BottomLevelASToLocalSpace).xyz;
		aabb[1] = mul(g_instanceBuffer[InstanceID()].AABBMax, attr.BottomLevelASToLocalSpace).xyz;
	}

	// Get the tmin and tmax of the intersection between the ray and this aabb
	if (RayAABBIntersectionTest(ray, aabb, tMin, tMax))
	{
		const float threshold = 0.0001;
		float t = tMin;
		const UINT MaxSteps = 512;

		// Do sphere tracing through the AABB.
		UINT i = 0;
		while (i++ < MaxSteps && t <= tMax)
		{
			const float3 position = ray.origin + t * ray.direction;
			const float distance = sdSphere(position, 0.5f);

			// Has the ray intersected the primitive? 
			if (distance <= threshold * t)
			{
				MyAttributes attr;
				attr.position = position;
				ReportHit(t, 0, attr);
				return;
			}

			// Since distance is the minimum distance to the primitive, 
			// we can safely jump by that amount without intersecting the primitive.
			// We allow for scaling of steps per primitive type due to any pre-applied 
			// transformations that don't preserve true distances.
			t += distance;
		}
	}
}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
	payload.color = float4(attr.position, 1);
}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
	payload.color = float4(0, 0, 0.2f, 1);
}

#endif // RAYTRACING_HLSL