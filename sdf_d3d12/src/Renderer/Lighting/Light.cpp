#include "pch.h"
#include "Light.h"

#include "imgui.h"
#include "Framework/Math.h"


LightManager::LightManager()
{
	for (auto& light : m_Lights)
	{
		light.Direction = { 0.0f, -0.707f, 0.707f };
		light.Color = { 1.0f, 1.0f, 1.0f };
		light.Intensity = 1.0f;
	}
}


void LightManager::CopyLightData(LightData* dest, size_t maxLights) const
{
	const size_t numBytes = sizeof(LightData) * min(maxLights, s_MaxLights);
	memcpy(dest, m_Lights.data(), numBytes);
}

void LightManager::DrawGui()
{
	for (size_t i = 0; i < m_Lights.size(); i++)
	{
		auto& light = m_Lights.at(i);

		ImGui::Text("Light %d", i);

		// Direction should be edited in spherical coordinates
		bool newDir = false;
		float theta = acosf(light.Direction.y);
		float phi = Math::Sign(light.Direction.z) * acosf(light.Direction.x / sqrtf(light.Direction.x * light.Direction.x + light.Direction.z * light.Direction.z));
		newDir |= ImGui::SliderAngle("Theta", &theta, 1.0f, 179.0f);
		newDir |= ImGui::SliderAngle("Phi", &phi, -179.0f, 180.0f);
		if (newDir)
		{
			const float sinTheta = sinf(theta);
			const float cosTheta = cosf(theta);
			const float sinPhi = sinf(phi);
			const float cosPhi = cosf(phi);

			light.Direction.x = sinTheta * cosPhi;
			light.Direction.y = cosTheta;
			light.Direction.z = sinTheta * sinPhi;
		}

		ImGui::ColorEdit3("Color", &light.Color.x);
		if (ImGui::DragFloat("Intensity", &light.Intensity, 0.01f))
		{
			light.Intensity = max(0, light.Intensity);
		}
	}
}
