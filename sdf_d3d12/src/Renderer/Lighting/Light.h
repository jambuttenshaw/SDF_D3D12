#pragma once

#include "Core.h"
#include "HlslCompat/RaytracingHlslCompat.h"

#include "Renderer/Buffer/Texture.h"

using namespace DirectX;


class LightManager
{
public:
	LightManager();
	~LightManager() = default;

	DISALLOW_COPY(LightManager);
	DEFAULT_MOVE(LightManager);

	void CopyLightData(LightGPUData* dest, size_t maxLights) const;

	inline Texture* GetEnvironmentMap() const { return m_EnvironmentMap.get(); }
	inline void SetEnvironmentMap(std::unique_ptr<Texture>&& map) { m_EnvironmentMap = std::move(map); }

	void DrawGui();

private:
	inline static constexpr size_t s_MaxLights = 1;

	std::array<LightGPUData, s_MaxLights> m_Lights;

	// Environmental lighting
	std::unique_ptr<Texture> m_EnvironmentMap;
};
