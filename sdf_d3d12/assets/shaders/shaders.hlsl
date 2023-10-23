
cbuffer ObjectCB : register(b0)
{
	float4x4	gWorld;
};

cbuffer PassCB : register(b1)
{
	float4x4	gView;
	float4x4	gInvView;
	float4x4	gProj;
	float4x4	gInvProj;
	float4x4	gViewProj;
	float4x4	gInvViewProj;
	
	float3		gWorldEyePos;
	
	float		gPadding0;
	
	float2		gRTSize;
	float2		gInvRTSize;
	
	float		gNearZ;
	float		gFarZ;
	
	float		gTotalTime;
	float		gDeltaTime;
};

struct PSInput
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

PSInput VSMain(float4 position : POSITION, float4 color : COLOR)
{
	PSInput result;

	float4 worldPosition = mul(position, gWorld);
	result.position = mul(worldPosition, gViewProj);
	result.color = color;

	return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	return input.color;
}
