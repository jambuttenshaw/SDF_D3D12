#pragma once

#include "RaytracingHlslCompat.h"


namespace GlobalRootSignatureParams
{
	enum Value
	{
		OutputViewSlot = 0,
		AccelerationStructureSlot,
		PassBufferSlot,
		AABBAttributesBuffer,
		Count
	};
}


namespace LocalRootSignatureParams
{
	enum Value
	{
		SDFVolumeSlot = 0,
		Count
	};

	struct RootArguments
	{
		D3D12_GPU_DESCRIPTOR_HANDLE volumeSRV;
	};
}
