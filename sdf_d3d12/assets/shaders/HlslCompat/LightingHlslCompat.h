#ifndef LIGHTINGHLSLCOMPAT_H
#define LIGHTINGHLSLCOMPAT_H

#ifdef HLSL
#include "HlslCompat.h"
#else
using namespace DirectX;
#endif

#define IRRADIANCE_THREADS 8
#define BRDF_INTEGRATION_THREADS 8
#define PREFILTERED_ENVIRONMENT_THREADS 8


struct GlobalLightingParamsBuffer
{
	XMFLOAT3 FaceNormal;
	float Roughness;
	XMFLOAT3 FaceTangent;
	float Padding1;
	XMFLOAT3 FaceBitangent;
	float Padding2;
};


#endif
