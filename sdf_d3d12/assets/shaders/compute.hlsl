
#define MAX_OBJECT_COUNT 2
#define NUM_SHADER_THREADS 8

struct ObjectCB
{
	// Each object must be 256-byte aligned
	
	float4x4 gWorld;
	float padding[48];
};

cbuffer AllObjectsCB : register(b0)
{
	ObjectCB AllObjects[MAX_OBJECT_COUNT];
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
	
	float gPadding0;
	
	float2 gRTSize;
	float2 gInvRTSize;
	
	float gNearZ;
	float gFarZ;
	
	float gTotalTime;
	float gDeltaTime;
};

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
static const float epsilon = 0.0001f;

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
// PRIMITIVES
//

float sdPlane(float3 p, float3 n, float h)
{
  // n must be normalized
	return dot(p, n) + h;
}

float sdSphere(float3 p, float r)
{
	return length(p) - r;
}

float sdBox(float3 p, float3 b)
{
	float3 q = abs(p) - b;
	return length(max(q, 0.0f)) + min(max(q.x, max(q.y, q.z)), 0.0f);
}

float sdTorus(float3 p, float2 t)
{
	float2 q = float2(length(p.xz) - t.x, p.y);
	return length(q) - t.y;
}

float sdOctahedron(float3 p, float s)
{
	p = abs(p);
	float m = p.x + p.y + p.z - s;
	float3 q;
	if (3.0f * p.x < m)
		q = p.xyz;
	else if (3.0f * p.y < m)
		q = p.yzx;
	else if (3.0f * p.z < m)
		q = p.zxy;
	else
		return m * 0.57735027f;
    
	float k = clamp(0.5 * (q.z - q.y + s), 0.0f, s);
	return length(float3(q.x, q.y - s + k, q.z - k));
}

//
// OPERATIONS
//

float4 opIntersect(float4 a, float4 b)
{
	return a.w > b.w ? a : b;
}
  
float4 opUnion(float4 a, float4 b)
{
	return a.w < b.w ? a : b;
}
 
float4 opDifference(float4 a, float4 b)
{
	return a.w > -b.w ? a : float4(b.rgb, -b.w);
}

float4 opSmoothIntersection(float4 a, float4 b, float k)
{
	float h = clamp(0.5f - 0.5f * (a.w - b.w) / k, 0.0f, 1.0f);
	float3 c = lerp(a.rgb, b.rgb, h);
	float d = lerp(a.w, b.w, h) + k * h * (1.0f - h);
   
	return float4(c, d);
}
 
float4 opSmoothUnion(float4 a, float4 b, float k)
{
	float h = clamp(0.5f + 0.5f * (a.w - b.w) / k, 0.0f, 1.0f);
	float3 c = lerp(a.rgb, b.rgb, h);
	float d = lerp(a.w, b.w, h) - k * h * (1.0f - h);
   
	return float4(c, d);
}
 
float4 opSmoothDifference(float4 a, float4 b, float k)
{
	float h = clamp(0.5f - 0.5f * (a.w + b.w) / k, 0.0f, 1.0f);
	float3 c = lerp(a.rgb, b.rgb, h);
	float d = lerp(a.w, -b.w, h) + k * h * (1.0f - h);
   
	return float4(c, d);
}

// domain operations

float2 repeat2d(float2 p, float s, out float2 id)
{
	id = round(p / s);
	return p - s * id;
}

//
// SCENE
//

float4 sdScene(float3 p)
{
	float4 nearest = float4(float3(0.13f, 0.2f, 0.17f), sdPlane(p, float3(0.0f, 1.0f, 0.0f), 1.0f));
    
	float4 torus = float4(BLUE, sdTorus(p - float3(-1.7f, 0.0f, 0.4f), float2(0.8f, 0.3f)));
	float4 octahedron = float4(RED, sdOctahedron(p - float3(0.0f, 0.3f, 0.0f), 1.0f));

	float4 shape = opSmoothUnion(torus, octahedron, 0.3f);

	nearest = opUnion(nearest, shape);
    
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

float shadow(float3 p, float3 dir)
{
	float dist = 0.0f;
	float result = 1.0f;
    
	while (dist < D)
	{
		float nearest = sdScene(p + dir * dist).w;
        
		if (nearest < epsilon)
		{
			result = 0.0f;
			break;
		}
		dist += nearest;
	}
    
	return result;
}


float smoothShadow(float3 p, float3 dir, float k)
{
    // physical interpretation of k is the light size as a solid angle
    
	float dist = 0.0f;
	float prev_nearest = 1e20f;
	float result = 1.0f;
    
	while (dist < D)
	{
		float nearest = sdScene(p + dir * dist).w;
        
		if (nearest < epsilon)
		{
			result = 0.0f;
			break;
		}
        // soft shadowing
        // penumbra is proportional to closest_miss / distance_to_closest_miss
       
		float y = nearest * nearest / (2.0f * prev_nearest);
		float d = sqrt(nearest * nearest - y * y);
        
		result = min(result, d / (k * max(0.0f, dist - y)));
        
		prev_nearest = nearest;
		dist += nearest;
	}
    
	return result;
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
            
            // get hit point, normal, light direction
			float3 hit = p + dir * dist;
			float3 normal = computeSurfaceNormal(hit);
			float3 l = lightDirection;
            
            // ray march again for shadow
			float shadow = smoothShadow(hit + 2.0f * epsilon * normal, l, 0.05f);
            
            // calculate lighting
			float light = max(0.0f, shadow * dot(l, normal));
			result = float3(light, light, light) + lightAmbient;
            
            // calculate fog
			float fog = map(clamp(dist, Dfog, D), Dfog, D, 1.0f, 0.0f);
			result *= fog;
            
            // Apply object colour
			result *= nearest.rgb;
            
            
			break;
		}
		dist += nearest.w;
	}
    
	return result;
}


void mainImage(out float4 fragColor, in float2 fragCoord)
{
	
}



[numthreads(NUM_SHADER_THREADS, NUM_SHADER_THREADS, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	// Write a constant colour into the output texture
	float2 uv = float2(DTid.xy) / gRTSize;
	
	float3 tot = float3(0.0f, 0.0f, 0.0f);
    
    // remap from [0,1] to [-1,-1]
	float2 camUV = uv * 2.0f - float2(1.0f, 1.0f);
	
    // get pixel coords in view space by un-projecting
	float4 pixel = mul(gInvViewProj, float4(camUV, gNearZ, 0.0f));
	
    // view dir is from camera to pixel in view space
	float3 rayDirection = normalize(pixel.xyz);
    
	float3 col = intersectWithWorld(gWorldEyePos, rayDirection);
        
    // gamma
	col = pow(col, float3(0.4545f, 0.4545f, 0.4545f));
        
	tot += col;
    
	const float4 color = float4(tot, 1.0f);
	OutputTexture[DTid.xy] = color;
}
