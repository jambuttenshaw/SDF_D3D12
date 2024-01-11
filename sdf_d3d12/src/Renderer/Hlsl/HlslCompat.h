#ifndef HLSLCOMPAT_H
#define HLSLCOMPAT_H

typedef float2 XMFLOAT2;
typedef float3 XMFLOAT3;
typedef float4 XMFLOAT4;
typedef float4 XMVECTOR;
typedef float4x4 XMMATRIX;
typedef uint UINT;


struct Ray
{
	XMFLOAT3 origin;
	XMFLOAT3 direction;
};

#endif // HLSLCOMPAT_H
