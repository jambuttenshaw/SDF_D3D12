#pragma once

using namespace DirectX;


struct PassCBType
{
	XMMATRIX View;
	XMMATRIX InvView;
	XMMATRIX Proj;
	XMMATRIX InvProj;
	XMMATRIX ViewProj;
	XMMATRIX InvViewProj;
	XMFLOAT3 WorldEyePos;
	float padding;
	XMFLOAT2 RTSize;
	XMFLOAT2 InvRTSize;
	float NearZ;
	float FarZ;
	float TotalTime;
	float DeltaTime;
};


struct ObjectCBType
{
	XMMATRIX WorldMat;
};
