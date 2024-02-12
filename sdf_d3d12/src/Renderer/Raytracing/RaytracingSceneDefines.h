#pragma once


namespace GlobalRootSignatureParams
{
	enum Value
	{
		OutputViewSlot = 0,			// Raytracing output texture
		AccelerationStructureSlot,	// Scene acceleration structure
		PassBufferSlot,				// Pass constants
		SamplerSlot,				// Volume sampler (this could be specified in a local root signature if it should be different per object)
		Count
	};
}


namespace LocalRootSignatureParams
{
	enum Value
	{
		BrickPropertiesBuffer = 0,	// Properties constant among all bricks
		BrickPoolSlot,				// Brick pool to raymarch
		BrickBufferSlot,			// Indirection data for brick
		Count
	};

	struct RootArguments
	{
		BrickPropertiesConstantBuffer brickProperties;
		D3D12_GPU_DESCRIPTOR_HANDLE brickPoolSRV;
		D3D12_GPU_VIRTUAL_ADDRESS brickBuffer;
	};
}
