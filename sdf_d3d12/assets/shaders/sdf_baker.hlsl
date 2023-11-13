
#define FLOAT_MAX 3.402823466e+38F

// These must match those defined in C++
#define MAX_OBJECT_COUNT 16
#define NUM_SHADER_THREADS 8


// Shape IDs
#define SHAPE_SPHERE 0u
#define SHAPE_BOX 1u
#define SHAPE_PLANE 2u
#define SHAPE_TORUS 3u
#define SHAPE_OCTAHEDRON 4u

// Operation IDs
#define OP_UNION 0u
#define OP_SUBTRACTION 1u
#define OP_INTERSECTION 2u
#define OP_SMOOTH_UNION 3u
#define OP_SMOOTH_SUBTRACTION 4u
#define OP_SMOOTH_INTERSECTION 5u


RWTexture3D<float> OutputTexture : register(u0);


//
// Constant Parameters
// 

// constants
#define PI 3.14159265359f;

static const float D = 30.0f;
static const float epsilon = 0.001f;

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

float4 opUnion(float4 a, float4 b)
{
	return a.w < b.w ? a : b;
}

float4 opSubtraction(float4 a, float4 b)
{
	return a.w > -b.w ? a : float4(b.rgb, -b.w);
}

float4 opIntersection(float4 a, float4 b)
{
	return a.w > b.w ? a : b;
}
  
float4 opSmoothUnion(float4 a, float4 b, float k)
{
	float h = clamp(0.5f + 0.5f * (a.w - b.w) / k, 0.0f, 1.0f);
	float3 c = lerp(a.rgb, b.rgb, h);
	float d = lerp(a.w, b.w, h) - k * h * (1.0f - h);
   
	return float4(c, d);
}
 
float4 opSmoothSubtraction(float4 a, float4 b, float k)
{
	float h = clamp(0.5f - 0.5f * (a.w + b.w) / k, 0.0f, 1.0f);
	float3 c = lerp(a.rgb, b.rgb, h);
	float d = lerp(a.w, -b.w, h) + k * h * (1.0f - h);
   
	return float4(c, d);
}

float4 opSmoothIntersection(float4 a, float4 b, float k)
{
	float h = clamp(0.5f - 0.5f * (a.w - b.w) / k, 0.0f, 1.0f);
	float3 c = lerp(a.rgb, b.rgb, h);
	float d = lerp(a.w, b.w, h) + k * h * (1.0f - h);
   
	return float4(c, d);
}


float3 opTransform(float3 p, matrix t)
{
	return mul(float4(p, 1.0f), t).xyz;
}


// Resolve primitive/operation

float sdPrimitive(float3 p, uint prim, float4 param)
{
	switch (prim)
	{
		case SHAPE_SPHERE:
			return sdSphere(p, param.x);
		case SHAPE_BOX:
			return sdBox(p, param.xyz);
		case SHAPE_PLANE:
			return sdPlane(p, param.xyz, param.w);
		case SHAPE_TORUS:
			return sdTorus(p, param.xy);
		case SHAPE_OCTAHEDRON:
			return sdOctahedron(p, param.x);
		default:
			return 0.0f;
	}
}

float4 opPrimitive(float4 a, float4 b, uint op, float k)
{
	switch (op)
	{
		case OP_UNION:
			return opUnion(a, b);
		case OP_SUBTRACTION:
			return opSubtraction(a, b);
		case OP_INTERSECTION:
			return opIntersection(a, b);
		case OP_SMOOTH_UNION:
			return opSmoothUnion(a, b, k);
		case OP_SMOOTH_SUBTRACTION:
			return opSmoothSubtraction(a, b, k);
		case OP_SMOOTH_INTERSECTION:
			return opSmoothIntersection(a, b, k);
		default:
			return a;
	}
}


[numthreads(NUM_SHADER_THREADS, NUM_SHADER_THREADS, NUM_SHADER_THREADS)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	// Make sure thread has a pixel in the texture to process
	uint3 dims;
	OutputTexture.GetDimensions(dims.x, dims.y, dims.z);
	if (DTid.x >= dims.x || DTid.y >= dims.y || DTid.z >= dims.z)
		return;
	
	float3 uvw = DTid / (float3) (dims - uint3(1, 1, 1));
	uvw = (uvw * 2.0f) - 1.0f;
	
	float shape = min(
		sdSphere(uvw - float3(-0.25f, 0.0f, 0.0f), 0.25f),
		sdSphere(uvw - float3(0.25f, 0.0f, 0.0f), 0.25f)
	);
	
	OutputTexture[DTid] = abs(shape);
}
