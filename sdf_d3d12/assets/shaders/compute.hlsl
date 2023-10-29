
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


[numthreads(NUM_SHADER_THREADS, NUM_SHADER_THREADS, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	// Write a constant colour into the output texture
	const float4 color = float4(1.0f, 0.0f, 0.0f, 1.0f);
	OutputTexture[DTid.xy] = color;
}
