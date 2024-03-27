#pragma once


namespace GlobalRootSignatureParams
{
	enum Value
	{
		OutputViewSlot = 0,			// Raytracing output texture

		AccelerationStructureSlot,	// Scene acceleration structure

		PassBufferSlot,				// Pass constants

		MaterialBufferSlot,			// All materials

		GlobalLightingSRVSlot,		// Environment map, Irradiance map, etc in a contiguous array of descriptors
		GlobalLightingSamplerSlot,	// Samplers used for the global lighting map

		VolumeSamplerSlot,			// Volume sampler (this could be specified in a local root signature if it should be different per object)

		PickingParamsSlot,			// Picking params buffer
		PickingOutputSlot,			// Picking output buffer

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
		MaterialTableSlot,			// Indirection table for which materials to apply to the object
		Count
	};

	struct RootArguments
	{
		BrickPropertiesConstantBuffer brickProperties;
		D3D12_GPU_DESCRIPTOR_HANDLE brickPoolSRV;
		D3D12_GPU_VIRTUAL_ADDRESS brickBuffer;
		XMUINT4 materialTable;
	};
}
