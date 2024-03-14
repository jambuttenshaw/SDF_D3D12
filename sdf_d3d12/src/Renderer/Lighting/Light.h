#pragma once

#include "Core.h"
#include "HlslCompat/RaytracingHlslCompat.h"

#include "Renderer/Buffer/Texture.h"

using namespace DirectX;


class LightManager
{
public:
	LightManager();
	~LightManager();

	DISALLOW_COPY(LightManager);
	DEFAULT_MOVE(LightManager);

	void CopyLightData(LightGPUData* dest, size_t maxLights) const;

	void SetEnvironmentMap(std::unique_ptr<Texture>&& map);
	inline Texture* GetEnvironmentMap() const { return m_EnvironmentMap.get(); }
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetEnvironmentMapSRV() const { return m_EnvironmentMapSRV.GetGPUHandle(); }

	void DrawGui();

private:
	inline static constexpr size_t s_MaxLights = 1;

	std::array<LightGPUData, s_MaxLights> m_Lights;

	// Environmental lighting
	std::unique_ptr<Texture> m_EnvironmentMap;
	DescriptorAllocation m_EnvironmentMapSRV; // Cubemap SRV
};
