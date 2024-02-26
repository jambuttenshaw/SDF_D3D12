#include "pch.h"
#include "Demos.h"

#include "imgui.h"
#include "Framework/Math.h"



DropsDemo::DropsDemo()
{
	Random::Seed(0);
	for (UINT i = 0; i < m_MaxSphereCount; i++)
	{
		m_Spheres.push_back({});
		SphereData& sphereData = m_Spheres.at(i);

		sphereData.radius = Random::Float(0.25f, 0.75f);
		sphereData.scale = {
			Random::Float(-2.0f, 2.0f),
			Random::Float(-5.0f, 5.0f),
			Random::Float(-2.0f, 2.0f)
		};
		sphereData.speed = {
			Random::Float(-1.0f, 1.0f) / sphereData.radius,
			Random::Float(-1.0f, 1.0f) / sphereData.radius,
			Random::Float(-1.0f, 1.0f) / sphereData.radius
		};
		sphereData.offset = {
			Random::Float(-3.0f, 3.0f),
			0.0f,
			Random::Float(-3.0f, 3.0f)
		};
	}
}


SDFEditList DropsDemo::BuildEditList(float deltaTime)
{
	m_Time += deltaTime;

	SDFEditList editList(m_SphereCount + 2, 12.0f);

	// Create base
	editList.AddEdit(SDFEdit::CreateBox({}, { 6.0f, 0.05f, 6.0f }));

	for (UINT i = 0; i < m_SphereCount; i++)
	{
		Transform transform;
		transform.SetTranslation(
			{
				m_Spheres.at(i).offset.x + m_Spheres.at(i).scale.x * cosf(m_Spheres.at(i).speed.x * m_Time),
				m_Spheres.at(i).offset.y + m_Spheres.at(i).scale.y * cosf(m_Spheres.at(i).speed.y * m_Time),
				m_Spheres.at(i).offset.z + m_Spheres.at(i).scale.z * cosf(m_Spheres.at(i).speed.z * m_Time)
			});
		editList.AddEdit(SDFEdit::CreateSphere(transform, m_Spheres.at(i).radius, SDF_OP_SMOOTH_UNION, m_SphereBlend));
	}

	// Delete anything poking from the bottom
	editList.AddEdit(SDFEdit::CreateBox({ 0.0f, -3.05f, 0.f }, { 6.0f, 3.0f, 6.0f }, SDF_OP_SUBTRACTION));

	return editList;
}

bool DropsDemo::DisplayGUI()
{
	bool open = true;
	if (ImGui::Begin("Drops Demo", &open))
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(255, 255, 0)));
		ImGui::Text("Stats");
		ImGui::PopStyleColor();

		ImGui::Separator();

		ImGui::Text("Edit Count: %d", m_SphereCount + 2);

		ImGui::Separator();

		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(255, 255, 0)));
		ImGui::Text("Controls");
		ImGui::PopStyleColor();

		ImGui::Separator();

		int sphereCount = static_cast<int>(m_SphereCount);
		if (ImGui::SliderInt("Spheres", &sphereCount, 1, 1022))
		{
			m_SphereCount = sphereCount;
		}
		ImGui::SliderFloat("Blending", &m_SphereBlend, 0.0f, 1.0f);

		ImGui::Separator();
	}
	ImGui::End();

	return true;
}


/*
SDFEditList Demos::IceCreamDemo(float deltaTime)
{
	SDFEditList editList(64, 8.0f);

	static UINT segments = 16;
	static float height = 0.1f;
	static float width = 1.0f;

	// Create cone
	for (UINT i = 0; i < segments; i++)
	{
		const float ir = width * static_cast<float>(i) / static_cast<float>(segments);
		const float y = 2.0f * height * static_cast<float>(i);

		Transform t{ 0.0f, y, 0.0f };

		editList.AddEdit(SDFEdit::CreateTorus(t, ir, height, SDF_OP_SMOOTH_UNION, 0.3f));
	}

	// Create ice cream
	editList.AddEdit(SDFEdit::CreateSphere({ 0.0f, 1.9f * segments * height, 0.0f }, 0.9f * width, SDF_OP_UNION, 0.0f));

	return editList;
}
*/
