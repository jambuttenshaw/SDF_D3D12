#ifndef HLSLCOMPAT_H
#define HLSLCOMPAT_H

typedef float2 XMFLOAT2;
typedef float3 XMFLOAT3;
typedef float4 XMFLOAT4;
typedef float4 XMVECTOR;
typedef float4x4 XMMATRIX;

typedef uint UINT;
typedef uint2 XMUINT2;
typedef uint3 XMUINT3;
typedef uint4 XMUINT4;


struct Ray
{
	XMFLOAT3 origin;
	XMFLOAT3 direction;
};

struct AABB
{
	XMFLOAT3 TopLeft;
	XMFLOAT3 BottomRight;
};


#define FLOAT_MAX 3.402823466e+38F


#endif // HLSLCOMPAT_H
