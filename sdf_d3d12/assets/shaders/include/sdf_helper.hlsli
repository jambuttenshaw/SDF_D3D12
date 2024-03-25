#ifndef SDFHELPER_H
#define SDFHELPER_H

#define HLSL
#include "../HlslCompat/ComputeHlslCompat.h"

#include "Quaternion.hlsli"

//
// PRIMITIVES
//

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

float sdFractal(float3 p)
{
	const float3 a1 = float3(1.0f, 1.0f, 1.0f);
	const float3 a2 = float3(-1.0f, -1.0f, 1.0f);
	const float3 a3 = float3(1.0f, -1.0f, -1.0f);
	const float3 a4 = float3(-1.0f, 1.0f, -1.0f);
	int n = 0;
	while (n < 15)
	{
		float3 c = a1;
		float dist = length(p - a1);
		float d = length(p - a2);
		if (d < dist)
		{
			c = a2;
			dist = d;
		}
		d = length(p - a3);
		if (d < dist)
		{
			c = a3;
			dist = d;
		}
		d = length(p - a4);
		if (d < dist)
		{
			c = a4;
		}
		p = 2.0f * p - c;
		n++;
	}

	return length(p) * pow(2.0f, float(-n)) - 0.005f;
}

float sdPrimitive(float3 p, SDFShape prim, float4 param)
{
	switch (prim)
	{
		case SDF_SHAPE_SPHERE:
			return sdSphere(p, param.x);
		case SDF_SHAPE_BOX:
			return sdBox(p, param.xyz);
		case SDF_SHAPE_TORUS:
			return sdTorus(p, param.xy);
		case SDF_SHAPE_OCTAHEDRON:
			return sdOctahedron(p, param.x);
		case SDF_SHAPE_BOX_FRAME:
			return sdBoxFrame(p, param.xyz, param.w);
		case SDF_SHAPE_FRACTAL:
			return sdFractal(p);
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
	return max(a, -b) + e * e * 0.125f / r;
}


float3 opTransform(float3 p, matrix t)
{
	return mul(float4(p, 1.0f), t).xyz;
}

float3 opTransform(float3 p, float4 q, float3 t)
{
	return rotate_vector(p + t, q);
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


// Operations with material interpolation parameters
float4 opUnion_Material(float a, float b, float4 ta, float4 tb)
{
	return a < b ? ta : tb;
}
  
float4 opSmoothUnion_Material(float a, float b, float4 ta, float4 tb, float r)
{
	float e = saturate(r - (b - a) / r);
	return lerp(ta, tb, e);
}

float4 opPrimitive_Material(float a, float b, float4 ta, float4 tb, SDFOperation op, float k)
{
	switch (op)
	{
		case SDF_OP_UNION:
			return opUnion_Material(a, b, ta, tb);
		case SDF_OP_SMOOTH_UNION:
			return opSmoothUnion_Material(a, b, ta, tb, k);
		default:
			return ta;
	}
}


//
// BOUNDING SPHERES
// Gives the distance to the surface of a bounding sphere for each primitive
//

float boundingSpherePrimitive(float3 p, SDFShape prim, float4 param)
{
	switch (prim)
	{
		case SDF_SHAPE_SPHERE:
			return sdSphere(p, param.x);
		case SDF_SHAPE_BOX:
			return sdSphere(p, length(param.xyz));
		case SDF_SHAPE_TORUS:
			return sdSphere(p, param.x + param.y);
		case SDF_SHAPE_OCTAHEDRON:
			return sdSphere(p, param.x);
		case SDF_SHAPE_BOX_FRAME:
			return sdSphere(p, length(param.xyz) + param.w);
		case SDF_SHAPE_FRACTAL:
			return sdSphere(p, 1.75f /*sqrt(3)*/);
		default:
			return 0.0f;
	}
}


//
// HELPERS
//

SDFShape GetShape(uint editParams)
{
	return (SDFShape) (editParams & 0x000000FF);
}

SDFOperation GetOperation(uint editParams)
{
	return (SDFOperation) ((editParams >> 8) & 0x00000003);
}

uint GetMaterialTableIndex(uint editParams)
{
	return ((editParams >> 10) & 0x00000003);
}

uint GetDependencyCount(uint editParams)
{
	return ((editParams >> 16) & 0x0000FFFF);
}

bool IsSmoothOperation(SDFOperation op)
{
	// smooth if second bit is set
	return op & 2;
}

bool IsSmoothEdit(uint editParams)
{
	// smooth if second bit of op is set
	return editParams & 0x00000200;
}

#endif