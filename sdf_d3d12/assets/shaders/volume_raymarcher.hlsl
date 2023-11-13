
#define FLOAT_MAX 3.402823466e+38F

// These must match those defined in C++
#define MAX_OBJECT_COUNT 16
#define NUM_SHADER_THREADS 8


struct ObjectCB
{
	// Each object must be 256-byte aligned
	matrix InvWorldMat;
	
	float Scale;
	
	// SDF primitive data
	uint Shape;
	uint Operation;
	float BlendingFactor;

	float4 ShapeParams;

	float4 Color;
	
	float4 padding1[9];
};

cbuffer AllObjectsCB : register(b0)
{
	ObjectCB gAllObjects[MAX_OBJECT_COUNT];
};

cbuffer PassCB : register(b1)
{
	float4x4 gView;
	float4x4 gInvView;
	float4x4 gProj;
	float4x4 gInvProj;
	float4x4 gViewProj;
	float4x4 gInvViewProj;
	
	float3 gWorldEyePos;
	
	uint gObjectCount;
	
	float2 gRTSize;
	float2 gInvRTSize;
	
	float gNearZ;
	float gFarZ;
	
	float gTotalTime;
	float gDeltaTime;
};

Texture3D<float> SDFVolume : register(t0);

RWTexture2D<float4> OutputTexture : register(u0);

//
// Constant Parameters
// 

// direction TO LIGHT
// or can be treated as a position for a point light
static const float3 lightDirection = normalize(float3(0.7f, 1.0f, 0.7f));
static const float3 lightAmbient = float3(0.2f, 0.2f, 0.2f);

// constants
#define PI 3.14159265359f;

static const float D = 30.0f;
static const float Dfog = 10.0f;
static const float epsilon = 0.001f;

// const delta vectors for normal calculation
static const float S = 0.001f;
static const float3 deltax = float3(S, 0.0f, 0.0f);
static const float3 deltay = float3(0.0f, S, 0.0f);
static const float3 deltaz = float3(0.0f, 0.0f, S);

static const float3 WHITE = float3(1.0f, 1.0f, 1.0f);
static const float3 RED = float3(0.92f, 0.2f, 0.2f);
static const float3 BLUE = float3(0.2f, 0.58f, 0.92f);

//
// UTILITY
//

float map(float value, float min1, float max1, float min2, float max2)
{
	return min2 + (value - min1) * (max2 - min2) / (max1 - min1);
}

//
// SCENE
//

float4 sdScene(float3 p)
{
#ifdef DISPLAY_HEATMAP
	heatmap++;
#endif
	
	float4 nearest = float4(0.0f, 0.0f, 0.0f, FLOAT_MAX);

	return nearest;
}


// 
// LIGHTING
// 

float3 computeSurfaceNormal(float3 p)
{
	float d = sdScene(p).w;
	return normalize(float3(
        sdScene(p + deltax).w - d,
        sdScene(p + deltay).w - d,
        sdScene(p + deltaz).w - d
    ));
}


float3 intersectWithWorld(float3 p, float3 dir)
{
    // main raymarching function
    
	float dist = 0.0f;
	float4 nearest = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float3 result = float3(0.0f, 0.0f, 0.0f);
    
    // D is the max dist to cast; effectively a far clipping sphere centred at the camera
	while (dist < D)
	{
        // find nearest object in scene
		nearest = sdScene(p + dir * dist);
        
		if (nearest.w < epsilon)
		{
            // nearest is within threshold
            
			// Apply object colour
			result = nearest.rgb;
			
#ifndef DISABLE_LIGHT
			
            // get hit point, normal, light direction
			float3 hit = p + dir * dist;
			float3 normal = computeSurfaceNormal(hit);
			float3 l = lightDirection;
            
            // calculate lighting
			float light = max(0.0f, dot(l, normal));
			
			result *= float3(light, light, light) + lightAmbient;
			
#endif // DISABLE_LIGHT
			
            // calculate fog
			float fog = map(clamp(dist, Dfog, D), Dfog, D, 1.0f, 0.0f);
			result *= fog;
            
			break;
		}
		dist += nearest.w;
	}
    
	return result;
}


[numthreads(NUM_SHADER_THREADS, NUM_SHADER_THREADS, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint GI : SV_GroupIndex)
{
	// Make sure thread has a pixel in the texture to process
	uint2 dims;
	OutputTexture.GetDimensions(dims.x, dims.y);
	/*
	if (DTid.x > dims.x || DTid.y > dims.y)
		return;
	// Write a constant colour into the output texture
	float2 uv = float2(DTid.xy) / (gRTSize - 1.0f);
	
    // map to NDC coordinates
	float2 camUV = uv * 2.0f - float2(1.0f, 1.0f);
	camUV.y = -camUV.y;
	
	// construct a vector (in view space) from the origin to this pixel
	float4 v = float4(
        camUV.x / gProj._11,
        camUV.y / gProj._22,
        1.0f,
        0.0f
    );
	// get the view vector in world space
	float3 viewVector = normalize(mul(v, gInvView).xyz);
	
	// perform ray marching
	//float3 col = intersectWithWorld(gWorldEyePos, viewVector);
      
	*/
	float s = SDFVolume.Load(int4(
		DTid.x % dims.x,
		DTid.y % dims.y,
		(DTid.y / dims.y) + (DTid.x / dims.x),
		0
	));
	
	float3 col = float3(s, s, s);
	
    // gamma
	col = pow(col, float3(0.4545f, 0.4545f, 0.4545f));
    
	// write to output
	float4 color = float4(col, 1.0f);
	
#ifdef DISPLAY_HEATMAP
	color = float4(
		heatmap / 100.0f,
		heatmap / 250.0f,
		heatmap / 400.0f,
		1.0f);
#endif
	
	OutputTexture[DTid.xy] = color;
}
