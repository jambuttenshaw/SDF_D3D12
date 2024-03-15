#ifndef PREFILTEREDENVIRONMENT_HLSL
#define PREFILTEREDENVIRONMENT_HLSL

#define HLSL
#include "../../HlslCompat/LightingHlslCompat.h"
#include "../../include/lighting_ibl.hlsli"


ConstantBuffer<GlobalLightingParamsBuffer> g_ParamsBuffer : register(b0);

TextureCube g_EnvironmentMap : register(t0);
SamplerState g_EnvironmentSampler : register(s1);

RWTexture2D<float4> g_PreFilteredEnvironmentMap : register(u0);



[numthreads(PREFILTERED_ENVIRONMENT_THREADS, PREFILTERED_ENVIRONMENT_THREADS, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	uint2 PEMDims;
	g_PreFilteredEnvironmentMap.GetDimensions(PEMDims.x, PEMDims.y);
    
	if (DTid.x >= PEMDims.x || DTid.y >= PEMDims.y)
		return;
    
    // calculate normal (direction from the centre of the cubemap through this fragment
	float2 uv = DTid.xy / (float2) (PEMDims - uint2(1, 1));
	uv = uv * 2.0f - 1.0f;
    
	const float3 worldPos = g_ParamsBuffer.FaceNormal + uv.x * g_ParamsBuffer.FaceTangent + uv.y * g_ParamsBuffer.FaceBitangent;
	const float3 N = normalize(worldPos);
	const float3 V = N;

	const uint SAMPLE_COUNT = 1024u;
	float totalWeight = 0.0f;
	float3 prefilteredColor = float3(0.0f, 0.0f, 0.0f);
	for (uint i = 0u; i < SAMPLE_COUNT; ++i)
	{
		const float2 Xi = Hammersley(i, SAMPLE_COUNT);
        // generate a half vector
		const float3 H = ImportanceSampleGGX(Xi, N, g_ParamsBuffer.Roughness);
        // calculate light vector
		const float3 L = normalize(2.0f * dot(V, H) * H - V);

		const float NdotL = max(dot(N, L), 0.0f);
		if (NdotL > 0.0f)
		{
            // sample environment map
			prefilteredColor += g_EnvironmentMap.SampleLevel(g_EnvironmentSampler, L, 0.0f).rgb * NdotL;
			totalWeight += NdotL;
		}
	}
	prefilteredColor = prefilteredColor / totalWeight;
    
	g_PreFilteredEnvironmentMap[DTid.xy] = float4(prefilteredColor, 1.0f);
}

#endif