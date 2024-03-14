#pragma once
#include "Renderer/Buffer/UploadBuffer.h"
#include "HlslCompat/RaytracingHlslCompat.h"

using namespace DirectX;


class MaterialManager
{
public:
	MaterialManager(size_t capacity);
	~MaterialManager() = default;

	DISALLOW_COPY(MaterialManager)
	DISALLOW_MOVE(MaterialManager)

	void UploadMaterialData() const;
	D3D12_GPU_VIRTUAL_ADDRESS GetMaterialBufferAddress() const;

	MaterialGPUData& GetMaterial(size_t index) { return m_MaterialStaging.at(index); }

	void DrawGui();

private:
	std::vector<MaterialGPUData> m_MaterialStaging;
	std::vector<UploadBuffer<MaterialGPUData>> m_MaterialBuffers;
};
