#ifndef LIGHTING_H
#define LIGHTING_H

#define HLSL
#include "../HlslCompat/StructureHlslCompat.h"

#include "lighting_helper.hlsli"
#include "lighting_ibl.hlsli"


float3 calculateLighting(
	UINT flags,
    float3 n,					// world space normal
    float3 v,					// world space view direction
	bool inShadow,
	LightGPUData light,			
	MaterialGPUData material,
	TextureCube irradianceMap,
	Texture2D brdfMap,
	TextureCube prefilterMap,
	SamplerState environmentSampler,
	SamplerState brdfSampler)
{
	// Invert direction - from "the direction the light points" to "the direction towards the light source"
	const float3 lightDirection = -light.Direction;
	const float3 lightIrradiance = light.Intensity * light.Color;

	float3 f0 = float3(0.04f, 0.04f, 0.04f);
	f0 = lerp(f0, material.Albedo, material.Metalness);

	float3 lo = float3(0.0f, 0.0f, 0.0f);
	
	// calculate light direction and irradiance
	const float3 el = lightIrradiance;
	const float3 l = lightDirection;

	// evaluate shading equation
	const float3 brdf = ggx_brdf(v, l, n, material.Albedo, f0, material.Roughness, material.Metalness) * el * saturate(dot(n, l));
	lo += max(brdf, 0.0f) * !inShadow;

	float3 ambient = float3(0.0f, 0.0f, 0.0f);

	if (!(flags & RENDER_FLAG_DISABLE_IBL))
	{
		ambient = calculateAmbientLighting(
			n,
			v,
			material.Albedo,
			f0,
			material.Roughness,
			material.Metalness,
			irradianceMap,
			brdfMap,
			prefilterMap,
			environmentSampler,
			brdfSampler
		);
	}

	return ambient + lo;
}

#endif