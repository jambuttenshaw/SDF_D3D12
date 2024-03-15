#ifndef LIGHTINGHELPER_H
#define LIGHTINGHELPER_H

#define PI 3.14159268535f


// DIFFUSE NDF's
float3 lambertian_diffuse(float3 albedo)
{
	return albedo / PI;
}


// SPECULAR NDF's
float ggx_ndf(float3 n, float3 h, float a)
{
    // definition of the GGX normal distribution function
    // a = roughness
    
	float a2 = a * a;
	float NdotH = saturate(dot(n, h));
	float NdotH2 = NdotH * NdotH;
	
	float denom = (NdotH2 * (a2 - 1.0f)) + 1.0f;
	denom = PI * denom * denom;
	
	return a2 / denom;
}


// FRESNEL
float3 shlick_fresnel_reflectance(float3 f0, float3 v, float3 h)
{
    // shlick approximation of the fresnel equations
	return f0 + (1.0f - f0) * pow(1.0f - saturate(dot(h, v)), 5.0f);
}


// GEOMETRY/VISIBILITY FUNCTIONS
float schlickggx_geometry(float NdotV, float k)
{
	return NdotV / (NdotV * (1.0f - k) + k);
}

float smith_geometry(float3 n, float3 v, float3 l, float k)
{
	float NdotV = saturate(dot(n, v));
	float NdotL = saturate(dot(n, l));
	float ggx1 = schlickggx_geometry(NdotV, k);
	float ggx2 = schlickggx_geometry(NdotL, k);
	
	return ggx1 * ggx2;
}


// BRDF
float3 ggx_brdf(float3 v, float3 l, float3 n, float3 albedo, float3 f0, float roughness, float metallic)
{
	float3 h = normalize(l + v);
 
	// specular
	
	// fresnel effect tells us how much light is reflected
	float3 Rf = shlick_fresnel_reflectance(f0, v, h);
	float ndf = ggx_ndf(n, h, roughness);
	float G = smith_geometry(n, v, l, roughness);
	
	float3 numerator = Rf * ndf * G;
	float denominator = 4.0f * saturate(dot(n, v)) * saturate(dot(n, l)) + 0.0001f;
	
	float3 fspec = numerator / denominator;

	// diffuse
	
	// all light that is not reflected is diffused
	float3 cdiff = albedo - f0;
	cdiff *= 1.0f - metallic;
	
	float3 fdiff = lambertian_diffuse(cdiff);
	
	return fdiff + fspec;
}

#endif