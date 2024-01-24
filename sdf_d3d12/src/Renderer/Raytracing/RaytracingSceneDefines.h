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
		BrickPoolSlot = 0,		// Brick pool to raymarch
		BrickBufferSlot,		// Indirection data for brick
		Count
	};

	struct RootArguments
	{
		D3D12_GPU_DESCRIPTOR_HANDLE brickPoolSRV;
		D3D12_GPU_VIRTUAL_ADDRESS brickBuffer;
	};
}
