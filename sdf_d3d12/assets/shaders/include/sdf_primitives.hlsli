#ifndef SDFPRIMITIVES_H
#define SDFPRIMITIVES_H

// Shape IDs
#define SHAPE_SPHERE 0u
#define SHAPE_BOX 1u
#define SHAPE_PLANE 2u
#define SHAPE_TORUS 3u
#define SHAPE_OCTAHEDRON 4u


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

#endif