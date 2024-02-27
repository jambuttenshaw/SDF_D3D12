#include "pch.h"
#include "Demos.h"

#include "imgui.h"
#include "Framework/Math.h"


std::map<std::string, BaseDemo*> BaseDemo::s_Demos;

void BaseDemo::CreateAllDemos()
{
	s_Demos["drops"] = &DropsDemo::Get();
	s_Demos["cubes"] = &CubesDemo::Get();
}

BaseDemo* BaseDemo::GetDemoFromName(const std::string& demoName)
{
	if (s_Demos.find(demoName) != s_Demos.end())
	{
		return s_Demos.at(demoName);
	}

	return nullptr;
}


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

void DropsDemo::DisplayGUI()
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


CubesDemo::CubesDemo()
{
	Random::Seed(0);

	const UINT numCubes = m_MaxCubeGridSize * m_MaxCubeGridSize * m_MaxCubeGridSize;
	m_CubeScales.resize(numCubes);
	for (auto& data : m_CubeScales)
	{
		data.Scale = Random::Float(0.75f, 1.25f);
		data.DeltaScale = Random::Float(-0.25f, 0.25f);
	}
}


SDFEditList CubesDemo::BuildEditList(float deltaTime)
{
	m_Time += deltaTime;

	const UINT numCubes = m_CubeGridSize * m_CubeGridSize * m_CubeGridSize;
	SDFEditList editList(numCubes, 12.0f);

	for (UINT z = 0; z < m_CubeGridSize; z++)
	for (UINT y = 0; y < m_CubeGridSize; y++)
	for (UINT x = 0; x < m_CubeGridSize; x++)
	{
		const UINT i = (z * m_CubeGridSize + y) * m_CubeGridSize + x;
		const auto fx = static_cast<float>(x);
		const auto fy = static_cast<float>(y);
		const auto fz = static_cast<float>(z);
		const auto fCount = static_cast<float>(m_CubeGridSize);

		Transform transform;
		transform.SetTranslation({ 
			fx - 0.5f * fCount + 0.5f,
			fy - 0.5f * fCount + 0.5f,
			fz - 0.5f * fCount + 0.5f
		});
		transform.SetScale(m_CubeScales.at(i).Scale + m_CubeScales.at(i).DeltaScale * sinf(4.0f * m_Time));

		editList.AddEdit(SDFEdit::CreateBoxFrame(transform, { 0.4f, 0.4f, 0.4f }, 0.02f, SDF_OP_SMOOTH_UNION, m_CubeBlend));
	}

	return editList;
}


void CubesDemo::DisplayGUI()
{
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(255, 255, 0)));
	ImGui::Text("Stats");
	ImGui::PopStyleColor();

	ImGui::Separator();

	const UINT numCubes = m_CubeGridSize * m_CubeGridSize * m_CubeGridSize;
	ImGui::Text("Edit Count: %d", numCubes);

	ImGui::Separator();

	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(255, 255, 0)));
	ImGui::Text("Controls");
	ImGui::PopStyleColor();

	ImGui::Separator();

	int cubeCount = static_cast<int>(m_CubeGridSize);
	if (ImGui::SliderInt("Cubes", &cubeCount, 1, static_cast<int>(m_MaxCubeGridSize)))
	{
		m_CubeGridSize = cubeCount;
	}
	ImGui::SliderFloat("Blending", &m_CubeBlend, 0.0f, 1.0f);

	ImGui::Separator();
}
