#ifndef SDFOPERATIONS_H
#define SDFOPERATIONS_H

enum SDFOperation
{
	SDF_OP_UNION = 0,
	SDF_OP_SUBTRACTION,
	SDF_OP_SMOOTH_UNION,
	SDF_OP_SMOOTH_SUBTRACTION
};


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


float3 opTransform(float3 p, matrix t)
{
	return mul(float4(p, 1.0f), t).xyz;
}



float opPrimitive(float a, float b, uint op, float k)
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

#endif