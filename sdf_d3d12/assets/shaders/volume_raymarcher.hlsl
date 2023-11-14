
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
	
	// Ray marchine properties
	float gSphereTraceEpsilon;
	float gRayMarchEpsilon;
	float gRayMarchStepSize;
	float gNormalEpsilon;
};

Texture3D<float> SDFVolume : register(t0);
SamplerState sampleState : register(s0);

RWTexture2D<float4> OutputTexture : register(u0);


groupshared ObjectCB gCachedObjects[MAX_OBJECT_COUNT];

//
// Constant Parameters
// 

// direction TO LIGHT
// or can be treated as a position for a point light
static const float3 lightDirection = normalize(float3(0.7f, 1.0f, 0.7f));
static const float3 lightAmbient = float3(0.2f, 0.2f, 0.2f);

// constants

// sphere trace constants
#define D_MAX 30.0f							// the maximum distance to ever reach via sphere tracing
#define D_FOG 10.0f						// the distance at which fog begins

// const delta vectors for normal calculation
#define NORMAL_DELTA_X float3(gNormalEpsilon, 0.0f, 0.0f)	// offset vectors for each axis
#define NORMAL_DELTA_Y float3(0.0f, gNormalEpsilon, 0.0f)
#define NORMAL_DELTA_Z float3(0.0f, 0.0f, gNormalEpsilon)


#ifdef DISPLAY_HEATMAP
static int heatmap = 0;
#endif

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

float sdBox(float3 p, float3 b)
{
	float3 q = abs(p) - b;
	return length(max(q, 0.0f)) + min(max(q.x, max(q.y, q.z)), 0.0f);
}

float3 opTransform(float3 p, matrix t)
{
	return mul(float4(p, 1.0f), t).xyz;
}

float2 sdScene(float3 p)
{
#ifdef DISPLAY_HEATMAP
	heatmap++;
#endif
	// x = distance, y = object id
	float2 nearest = float2(FLOAT_MAX, -1.0f);
	
	for (uint i = 0; i < gObjectCount; i++)
	{
		// apply primitive transform
		float3 p_transformed = opTransform(p, gCachedObjects[i].InvWorldMat) / gCachedObjects[i].Scale;
		
		// evaluate primitive
		float shape = sdBox(p_transformed, gCachedObjects[i].ShapeParams.xyz);
		shape *= gCachedObjects[i].Scale;

		// combine with scene
		nearest = shape < nearest.x ? float2(shape, i) : nearest;
	}

	return nearest;
}

float3 computeSurfaceNormal(float3 p)
{
	return normalize(float3(
        SDFVolume.SampleLevel(sampleState, p + NORMAL_DELTA_X, 0) - SDFVolume.SampleLevel(sampleState, p - NORMAL_DELTA_X, 0),
        SDFVolume.SampleLevel(sampleState, p + NORMAL_DELTA_Y, 0) - SDFVolume.SampleLevel(sampleState, p - NORMAL_DELTA_Y, 0),
        SDFVolume.SampleLevel(sampleState, p + NORMAL_DELTA_Z, 0) - SDFVolume.SampleLevel(sampleState, p - NORMAL_DELTA_Z, 0)
    ));
}

float3 raymarchVolume(uint id, float3 p, float3 dir)
{
	// calculate uvw of intersection
	
	// at the moment, p is in the range [boxCentre - boxExtents, boxCentre + boxExtents]
	// steps:
	// 1: p into range [-boxExtents, +boxExtents]
	// 2: p into range [-1, 1]
	// 3: p into range [0, 1]
	
	// step 1:
	// transform p into the box's local space
	// this is handily exactly what opTransform does!
	float3 uvw = opTransform(p, gCachedObjects[id].InvWorldMat);
	uvw /= gCachedObjects[id].Scale;
	
	// step 2:
	// divide p by the box's local extents, which are given in the shape params
	uvw /= gCachedObjects[id].ShapeParams.xyz;
	
	// step 3:
	// simply transform from [-1,1] to [0,1]
	uvw *= 0.5f;
	uvw += 0.5f;
	
#ifdef DISPLAY_AABB
	return uvw;
#else
	
	// step through volume to find surface
	float3 result;
	float s = FLOAT_MAX; // can be any number > rayMarch_epsilon
	
	while (s > gRayMarchEpsilon)
	{
#ifdef DISPLAY_HEATMAP
		heatmap++;
#endif
		
		s = SDFVolume.SampleLevel(sampleState, uvw, 0);
		uvw += dir * gRayMarchStepSize;
		
		// use sphere trace epsilon, as that is the possible error that we could have entered into this aabb
		if (max(max(uvw.x, uvw.y), uvw.z) >= 1.0f + gSphereTraceEpsilon || min(min(uvw.x, uvw.y), uvw.z) <= -gSphereTraceEpsilon)
		{
			// Exited box: return to sphere tracing
			return float3(0.0f, 0.0f, 0.0f);
		}
	}
	
	float3 normal = computeSurfaceNormal(uvw);
	
#ifdef DISPLAY_NORMALS
	
	result = 0.5f + 0.5f * normal;
#else	
	
            // get hit point, normal, light direction
	float3 l = lightDirection;
            
            // calculate lighting
	float light = max(0.0f, dot(l, normal));
			
	result = float3(light, light, light) + lightAmbient;
	
#endif // DISPLAY_NORMALS
	
	// iterate until surface is reached or exits volume
	return result;
#endif // DISPLAY_AABB
}


float3 intersectWithWorld(float3 p, float3 dir)
{
    // main raymarching function
    
	float dist = 0.0f;
	float2 nearest;
	float3 result = float3(0.0f, 0.0f, 0.0f);
    
    // D is the max dist to cast; effectively a far clipping sphere centred at the camera
	while (dist < D_MAX)
	{
        // find nearest object in scene
		nearest = sdScene(p + dir * dist);
        
		if (nearest.x < gSphereTraceEpsilon)
		{
            // nearest is within threshold
            
			// raymarch within volume
			result = raymarchVolume((uint)(nearest.y), p + dir * dist, dir);
			
            // calculate fog
			float fog = map(clamp(dist, D_FOG, D_MAX), D_FOG, D_MAX, 1.0f, 0.0f);
			result *= fog;
			break;
		}
		dist += nearest.x;
	}
    
	return result;
}


[numthreads(NUM_SHADER_THREADS, NUM_SHADER_THREADS, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint GI : SV_GroupIndex)
{
	if (GI < gObjectCount)
		gCachedObjects[GI] = gAllObjects[GI];
	
	GroupMemoryBarrierWithGroupSync();
	
	// Make sure thread has a pixel in the texture to process
	uint2 dims;
	OutputTexture.GetDimensions(dims.x, dims.y);
	
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
	float3 col = intersectWithWorld(gWorldEyePos, viewVector);
	
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
