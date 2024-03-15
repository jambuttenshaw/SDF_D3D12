#ifndef IRRADIANCE_HLSL
#define IRRADIANCE_HLSL

#define HLSL
#include "../../HlslCompat/LightingHlslCompat.h"
#include "../../include/lighting_ibl.hlsli"


ConstantBuffer<GlobalLightingParamsBuffer> g_ParamsBuffer : register(b0);

TextureCube g_EnvironmentMap : register(t0);
SamplerState g_EnvironmentSampler : register(s1);

RWTexture2D<float4> g_IrradianceMap : register(u0);


[numthreads(IRRADIANCE_THREADS, IRRADIANCE_THREADS, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	uint2 irradianceMapDims;
	g_IrradianceMap.GetDimensions(irradianceMapDims.x, irradianceMapDims.y);
    
	if (DTid.x > irradianceMapDims.x || DTid.y > irradianceMapDims.y)
		return;
    
	float2 uv = DTid.xy / (float2) (irradianceMapDims - uint2(1, 1));
	uv = uv * 2.0f - 1.0f;
    
	const float3 worldPos = g_ParamsBuffer.FaceNormal + uv.x * g_ParamsBuffer.FaceTangent + uv.y * g_ParamsBuffer.FaceBitangent;
	float3 normal = normalize(worldPos);
    
    // calculate irradiance
	float3 irradiance = float3(0.0f, 0.0f, 0.0f);

	float3 up = float3(0.0f, 1.0f, 0.0f);
    
    // fix artifacts at the poles of the irradiance map when normal and up are co-linear
	if (abs(normal.y) == 1.0f)
		up.yz = up.zy;
    
	float3 right = normalize(cross(up, normal));
	up = normalize(cross(normal, right));
	const float sampleDelta = 0.025f;
	float nrSamples = 0.0f;
	for (float phi = 0.0f; phi < 2.0f * PI; phi += sampleDelta)
	{
		for (float theta = 0.0f; theta < 0.5f * PI; theta += sampleDelta)
		{
			// spherical to cartesian (in tangent space)
			float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
			// tangent space to world
			const float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;

			irradiance += g_EnvironmentMap.SampleLevel(g_EnvironmentSampler, sampleVec, 0).rgb * cos(theta) * sin(theta);
			nrSamples++;
		}
	}
	irradiance = PI * irradiance / nrSamples;
    
	g_IrradianceMap[DTid.xy] = float4(irradiance, 1.0f);
}

#endif