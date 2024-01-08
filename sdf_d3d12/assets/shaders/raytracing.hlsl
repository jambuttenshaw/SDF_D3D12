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

struct CustomAttributes
{
	float3 p;
};
struct RayPayload
{
	float4 color;
};

[shader("raygeneration")]
void RaygenShader()
{
	float2 lerpValues = (float2) DispatchRaysIndex() / (float2) DispatchRaysDimensions();

    // Trace the ray.
    // Set the ray's extents.
	RayDesc ray;
	ray.Origin = float3(
        lerpValues.x,
        lerpValues.y,
        0.0f);
	ray.Direction = float3(0, 0, 1);
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
void IntersectionShader()
{
	const CustomAttributes attr = { 0.0f, 0.0f, 0.0f };
	ReportHit(RayTCurrent(), 0, attr);
}

[shader("closesthit")]
void ClosestHitShader(inout RayPayload payload, in CustomAttributes attr)
{
	payload.color = float4(1.0f, 0.0f, 0.0f, 1.0f);
}

[shader("miss")]
void MissShader(inout RayPayload payload)
{
	payload.color = float4(0, 0, 1, 1);
}

#endif // RAYTRACING_HLSL