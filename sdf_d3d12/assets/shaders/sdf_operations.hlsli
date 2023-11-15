
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

