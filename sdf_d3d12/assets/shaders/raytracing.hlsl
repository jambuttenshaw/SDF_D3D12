//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL


RaytracingAccelerationStructure Scene : register(t0, space0);
RWTexture2D<float4> RenderTarget : register(u0);

struct MyAttributes
{
	float3 position;
};
struct RayPayload
{
	float4 color;
};

[shader("raygeneration")]
void MyRaygenShader()
{
	float2 lerpValues = (float2) DispatchRaysIndex() / (float2) DispatchRaysDimensions();

    // Orthographic projection since we're raytracing in screen space.
	float3 rayDir = float3(0, 0, 1);
	float3 origin = float3(
        lerp(-1, 1, lerpValues.x),
        lerp(-1, 1, lerpValues.y),
        0.0f);


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
	MyAttributes attr;
	attr.position = float3(0, 0, 0);

	ReportHit(RayTCurrent(), 0, attr);
}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
	payload.color = float4(1, 0, 0, 1);
}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
	payload.color = float4(0, 0, 1, 1);
}

#endif // RAYTRACING_HLSL