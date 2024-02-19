#ifndef SDFHELPER_H
#define SDFHELPER_H

#define HLSL
#include "../../../src/Renderer/Hlsl/ComputeHlslCompat.h"


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

float sdBoxFrame(float3 p, float3 b, float e)
{
	p = abs(p) - b;
	float3 q = abs(p + e) - e;
	return min(min(
      length(max(float3(p.x, q.y, q.z), 0.0)) + min(max(p.x, max(q.y, q.z)), 0.0),
      length(max(float3(q.x, p.y, q.z), 0.0)) + min(max(q.x, max(p.y, q.z)), 0.0)),
      length(max(float3(q.x, q.y, p.z), 0.0)) + min(max(q.x, max(q.y, p.z)), 0.0));
}


float sdPrimitive(float3 p, SDFShape prim, float4 param)
{
	switch (prim)
	{
		case SDF_SHAPE_SPHERE:
			return sdSphere(p, param.x);
		case SDF_SHAPE_BOX:
			return sdBox(p, param.xyz);
		case SDF_SHAPE_PLANE:
			return sdPlane(p, param.xyz, param.w);
		case SDF_SHAPE_TORUS:
			return sdTorus(p, param.xy);
		case SDF_SHAPE_OCTAHEDRON:
			return sdOctahedron(p, param.x);
		case SDF_SHAPE_BOX_FRAME:
			return sdBoxFrame(p, param.xyz, param.w);
		default:
			return 0.0f;
	}
}


//
// OPERATIONS
//

float opUnion(float a, float b)
{
	return min(a, b);
}

float opSubtraction(float a, float b)
{
	return max(a, -b);
}

  
float opSmoothUnion(float a, float b, float r)
{
	float e = max(r - abs(a - b), 0.0f);
	return min(a, b) - e * e * 0.25f / r;
}

float opSmoothSubtraction(float a, float b, float r)
{
	float e = max(r - abs(a - b), 0.0f);
	return max(a, -b) + e * e * 0.25f / r;
}


float3 opTransform(float3 p, matrix t)
{
	return mul(float4(p, 1.0f), t).xyz;
}


float opPrimitive(float a, float b, SDFOperation op, float k)
{
	switch (op)
	{
		case SDF_OP_UNION:
			return opUnion(a, b);
		case SDF_OP_SUBTRACTION:
			return opSubtraction(a, b);
		case SDF_OP_SMOOTH_UNION:
			return opSmoothUnion(a, b, k);
		case SDF_OP_SMOOTH_SUBTRACTION:
			return opSmoothSubtraction(a, b, k);
		default:
			return a;
	}
}

//
// HELPERS
//

SDFShape GetShape(uint sdfPrimitive)
{
	return (SDFShape)(sdfPrimitive & 0x000000FF);
}

SDFOperation GetOperation(uint sdfPrimitive)
{
	return (SDFOperation)((sdfPrimitive >> 8) & 0x000000FF);
}

bool IsSmoothOperation(SDFOperation op)
{
	// smooth if second bit is set
	return op & 1;
}

bool IsSmoothPrimitive(uint sdfPrimitive)
{
	// smooth if second bit of op is set
	return sdfPrimitive & 0x00000100;
}

#endif