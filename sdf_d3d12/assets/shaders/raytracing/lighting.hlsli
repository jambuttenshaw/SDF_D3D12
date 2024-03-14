#ifndef LIGHTING_H
#define LIGHTING_H


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


float3 calculateLighting(
    float3 n, // world space normal
    float3 v  // world space view direction
)
{
	const float3 lightDirection = normalize(float3(0.5f, 1.0f, -1.0f));
	const float3 lightIrradiance = 4.0f * float3(1.0f, 1.0f, 1.0f);

	const float3 albedo = float3(1.0f, 0.1f, 0.1f);
	const float roughness = 0.05f;
	const float metalness = 0.0f;

	float3 f0 = float3(0.04f, 0.04f, 0.04f);
	f0 = lerp(f0, albedo, metalness);
    
	float3 lo = float3(0.0f, 0.0f, 0.0f);
	
	// calculate light direction and irradiance
	const float3 el = lightIrradiance;
	const float3 l = lightDirection;

	// evaluate shading equation
	const float3 brdf = ggx_brdf(v, l, n, albedo, f0, roughness, metalness) * el * saturate(dot(n, l));
	lo += brdf;
    
	const float3 ambient = float3(0.0f, 0.0f, 0.0f);
    
	return ambient + lo;
}

#endif