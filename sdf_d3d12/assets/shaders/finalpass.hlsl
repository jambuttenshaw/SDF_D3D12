
cbuffer PassCB : register(b0)
{
	float4x4 gView;
	float4x4 gInvView;
	float4x4 gProj;
	float4x4 gInvProj;
	float4x4 gViewProj;
	float4x4 gInvViewProj;
	
	float3 gWorldEyePos;
	
	uint gObjectCount;
	
	float2 gRTSize;
	float2 gInvRTSize;
	
	float gNearZ;
	float gFarZ;
	
	float gTotalTime;
	float gDeltaTime;
};


Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);


struct PSInput
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD;
};


PSInput VSMain(uint id : SV_VertexID)
{
	PSInput output;
 
    // generate a texture coordinate for one of the 4 corners of the screen filling quad
	output.uv = float2(id % 2, id / 2);
    // map that texture coordinate to a position in ndc
	output.position = float4((output.uv.x - 0.5f) * 2.0f, (0.5f - output.uv.y) * 2.0f, 0.0f, 1.0f);
	return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	return g_texture.Sample(g_sampler, input.uv);
}
