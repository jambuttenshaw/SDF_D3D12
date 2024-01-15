#pragma once



namespace GlobalRootSignatureParams
{
	enum Value
	{
		OutputViewSlot = 0,			// Raytracing output texture
		AccelerationStructureSlot,	// Scene acceleration structure
		PassBufferSlot,				// Pass constants
		Count
	};
}


namespace LocalRootSignatureParams
{
	enum Value
	{
		SDFVolumeSlot = 0,			// SDF Volume to raymarch
		AABBPrimitiveDataSlot,		// Array of data pertaining to each AABB primitive
		Count
	};

	struct RootArguments
	{
		D3D12_GPU_DESCRIPTOR_HANDLE volumeSRV;
		D3D12_GPU_VIRTUAL_ADDRESS aabbPrimitiveData;
	};
}
