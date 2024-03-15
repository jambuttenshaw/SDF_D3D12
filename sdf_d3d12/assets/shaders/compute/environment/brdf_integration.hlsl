#ifndef BRDFINTEGRATION_HLSL
#define BRDFINTEGRATION_HLSL

#define HLSL
#include "../../HlslCompat/LightingHlslCompat.h"
#include "../../include/lighting_ibl.hlsli"


RWTexture2D<float2> g_BRDFIntegrationMap : register(u0);


#define SAMPLE_COUNT 1024u


float2 IntegrateBRDF(float NdotV, float roughness)
{
	float3 V;
	V.x = sqrt(1.0f - NdotV * NdotV);
	V.y = 0.0f;
	V.z = NdotV;

	float A = 0.0f;
	float B = 0.0f;

	const float3 N = float3(0.0f, 0.0f, 1.0f);

	for (uint i = 0u; i < SAMPLE_COUNT; ++i)
	{
		const float2 Xi = Hammersley(i, SAMPLE_COUNT);
        // generate a half vector
		float3 H = ImportanceSampleGGX(Xi, N, roughness);
        // calculate light vector
		float3 L = normalize(2.0f * dot(V, H) * H - V);

		const float NdotL = saturate(L.z);
		const float NdotH = saturate(H.z);
		const float VdotH = saturate(dot(V, H));

		if (NdotL > 0.0f)
		{
            // calculate linear and constant coefficients
			const float G = smith_geometry_ibl(N, V, L, roughness);
			const float G_Vis = (G * VdotH) / (NdotH * NdotV);
			const float Fc = pow(1.0f - VdotH, 5.0f);

			A += (1.0f - Fc) * G_Vis;
			B += Fc * G_Vis;
		}
	}
	A /= float(SAMPLE_COUNT);
	B /= float(SAMPLE_COUNT);
	return float2(A, B);
}


[numthreads(BRDF_INTEGRATION_THREADS, BRDF_INTEGRATION_THREADS, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	uint2 BRDFIntegrationDims;
	g_BRDFIntegrationMap.GetDimensions(BRDFIntegrationDims.x, BRDFIntegrationDims.y);
    
	if (DTid.x >= BRDFIntegrationDims.x || DTid.y >= BRDFIntegrationDims.y)
		return;
    
	float2 uv = DTid.xy / (float2) (BRDFIntegrationDims - uint2(1, 1));
	const float2 integratedBRDF = IntegrateBRDF(uv.x, uv.y);
    
	g_BRDFIntegrationMap[DTid.xy] = integratedBRDF;
}

#endif