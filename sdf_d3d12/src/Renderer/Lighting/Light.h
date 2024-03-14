#pragma once

#include "Core.h"
#include "HlslCompat/RaytracingHlslCompat.h"

using namespace DirectX;





class LightManager
{
public:
	LightManager();
	~LightManager() = default;

	DISALLOW_COPY(LightManager);
	DEFAULT_MOVE(LightManager);

	void CopyLightData(LightGPUData* dest, size_t maxLights) const;
	void DrawGui();

private:
	inline static constexpr size_t s_MaxLights = 1;

	std::array<LightGPUData, s_MaxLights> m_Lights;
};
