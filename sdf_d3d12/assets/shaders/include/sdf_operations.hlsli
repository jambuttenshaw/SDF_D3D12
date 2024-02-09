#ifndef SDFOPERATIONS_H
#define SDFOPERATIONS_H

// Operation IDs
#define OP_UNION 0u
#define OP_SUBTRACTION 1u
#define OP_INTERSECTION 2u
#define OP_SMOOTH_UNION 3u
#define OP_SMOOTH_SUBTRACTION 4u
#define OP_SMOOTH_INTERSECTION 5u


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

float opIntersection(float a, float b)
{
	return max(a, b);
}
  
float opSmoothUnion(float a, float b, float k)
{
	float h = clamp(0.5f + 0.5f * (a - b) / k, 0.0f, 1.0f);
	float d = lerp(a, b, h) - k * h * (1.0f - h);
	return d;
}
 
float opSmoothSubtraction(float a, float b, float k)
{
	float h = clamp(0.5f - 0.5f * (a + b) / k, 0.0f, 1.0f);
	float d = lerp(a, -b, h) + k * h * (1.0f - h);
	return d;
}

float opSmoothIntersection(float a, float b, float k)
{
	float h = clamp(0.5f - 0.5f * (a - b) / k, 0.0f, 1.0f);
	float d = lerp(a, b, h) + k * h * (1.0f - h);
	return d;
}


float3 opTransform(float3 p, matrix t)
{
	return mul(float4(p, 1.0f), t).xyz;
}



float opPrimitive(float a, float b, uint op, float k)
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

#endif