#ifndef LIGHTINGIBL_H
#define LIGHTINGIBL_H

#include "lighting.hlsli"


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


float schlichggx_geometry_ibl(float NdotV, float roughness)
{
	float k = (roughness * roughness) / 2.0f; // k has a different definition for ibl than direct lighting
	return NdotV / (NdotV * (1.0f - k) + k);
}

float smith_geometry_ibl(float3 n, float3 v, float3 l, float roughness)
{
	float NdotV = max(dot(n, v), 0.0f);
	float NdotL = max(dot(n, l), 0.0f);
	float ggx2 = schlichggx_geometry_ibl(NdotV, roughness);
	float ggx1 = schlichggx_geometry_ibl(NdotL, roughness);

	return ggx1 * ggx2;
}

// adapted GGX NDF used to generate orientated and biased sample vectors for importance sampling, used for preprocessing the environment map for IBL
float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
	float a = roughness * roughness;
	
	float phi = 2.0f * PI * Xi.x;
	float cosTheta = sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
	float sinTheta = sqrt(1.0f - cosTheta * cosTheta);
	
    // from spherical coordinates to cartesian coordinates
	float3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;
	
    // from tangent-space vector to world-space sample vector
	float3 up = abs(N.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
	float3 tangent = normalize(cross(up, N));
	float3 bitangent = cross(N, tangent);
	
	float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
	return normalize(sampleVec);
}

#endif