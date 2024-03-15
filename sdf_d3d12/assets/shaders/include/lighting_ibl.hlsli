#ifndef LIGHTINGIBL_H
#define LIGHTINGIBL_H

#include "lighting_helper.hlsli"


// IBL

// taken from: https://learnopengl.com/PBR/IBL/Specular-IBL
// function for the Hammersley low-discrepancy sequence, based on the Van Der Corput sequence

float RadicalInverse_VdC(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}
// ----------------------------------------------------------------------------
float2 Hammersley(uint i, uint N)
{
	return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

float3 shlick_fresnel_roughness_reflectance(float3 f0, float3 v, float3 h, float roughness)
{
    // shlick approximation of the fresnel equations, with an input roughness parameter
	return f0 + (max((1.0f - roughness).xxx, f0) - f0) * pow(1.0f - saturate(dot(h, v)), 5.0f);
}

float schlichggx_geometry_ibl(float NdotV, float roughness)
{
	const float k = (roughness * roughness) / 2.0f; // k has a different definition for ibl than direct lighting
	return NdotV / (NdotV * (1.0f - k) + k);
}

float smith_geometry_ibl(float3 n, float3 v, float3 l, float roughness)
{
	const float NdotV = max(dot(n, v), 0.0f);
	const float NdotL = max(dot(n, l), 0.0f);
	const float ggx2 = schlichggx_geometry_ibl(NdotV, roughness);
	const float ggx1 = schlichggx_geometry_ibl(NdotL, roughness);

	return ggx1 * ggx2;
}

// adapted GGX NDF used to generate orientated and biased sample vectors for importance sampling, used for preprocessing the environment map for IBL
float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
	const float a = roughness * roughness;
	
	const float phi = 2.0f * PI * Xi.x;
	const float cosTheta = sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
	const float sinTheta = sqrt(1.0f - cosTheta * cosTheta);
	
    // from spherical coordinates to cartesian coordinates
	float3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;
	
    // from tangent-space vector to world-space sample vector
	const float3 up = abs(N.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
	const float3 tangent = normalize(cross(up, N));
	const float3 bitangent = cross(N, tangent);
	
	const float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
	return normalize(sampleVec);
}


float3 calculateAmbientLighting(
	float3 n,
	float3 v,
	float3 albedo,
	float3 f0,
	float roughness,
	float metallic,
	TextureCube irradianceMap,
	Texture2D brdfMap,
	TextureCube prefilterMap,
	SamplerState environmentSampler,
	SamplerState brdfSampler)
{
    // ues IBL for ambient lighting
	const float3 F = shlick_fresnel_roughness_reflectance(f0, v, n, roughness);
        
	const float3 kS = F;
	float3 kD = 1.0f - kS;
	kD *= 1.0f - metallic;
        
	const float3 irradiance = irradianceMap.SampleLevel(environmentSampler, n, 0.0f).rgb;
	const float3 diffuse = irradiance * albedo;
        
    // sample both the pre-filter map and the BRDF lut and combine them together as per the Split-Sum approximation to get the IBL specular part.
	const float MAX_REFLECTION_LOD = 4.0f;
	const float3 r = normalize(reflect(-v, n));

	const float3 prefilteredColor = prefilterMap.SampleLevel(environmentSampler, r, roughness * MAX_REFLECTION_LOD).rgb;
	const float2 brdf = brdfMap.SampleLevel(brdfSampler, float2(max(dot(n, v), 0.0f) + 0.01f, roughness), 0.0f).rg;
	const float3 specular = prefilteredColor * (F * brdf.x + brdf.y);

	return kD * diffuse + specular;
}


#endif