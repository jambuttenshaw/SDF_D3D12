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
