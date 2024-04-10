#include "pch.h"
#include "Demos.h"

#include "imgui.h"
#include "Framework/Math.h"


std::map<std::string, BaseDemo*> BaseDemo::s_Demos;

void BaseDemo::CreateAllDemos()
{
	s_Demos["drops"] = &DropsDemo::Get();
	s_Demos["cubes"] = &CubesDemo::Get();
	s_Demos["rain"] = &RainDemo::Get();
	s_Demos["fractal"] = &FractalDemo::Get();
	s_Demos["test"] = &TestDemo::Get();
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
		sphereData.material = static_cast<UINT>(Random::Int(0, 2));
	}
}


SDFEditList DropsDemo::BuildEditList(float deltaTime)
{
	m_Time += deltaTime;

	SDFEditList editList(m_SphereCount + 2, 12.0f);

	// Create base
	editList.AddEdit(SDFEdit::CreateBox({}, { 6.0f, 0.05f, 6.0f }, SDF_OP_UNION, 0.0f, 3));

	for (UINT i = 0; i < m_SphereCount; i++)
	{
		Transform transform;
		transform.SetTranslation(
			XMVECTOR{
				m_Spheres.at(i).offset.x + m_Spheres.at(i).scale.x * cosf(m_Spheres.at(i).speed.x * m_Time),
				m_Spheres.at(i).offset.y + m_Spheres.at(i).scale.y * cosf(m_Spheres.at(i).speed.y * m_Time),
				m_Spheres.at(i).offset.z + m_Spheres.at(i).scale.z * cosf(m_Spheres.at(i).speed.z * m_Time)
			});
		editList.AddEdit(SDFEdit::CreateSphere(transform, m_Spheres.at(i).radius, SDF_OP_SMOOTH_UNION, m_SphereBlend, m_Spheres.at(i).material));
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

	ImGui::Text("Edit Count: %d", GetEditCount());

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
		transform.SetTranslation(XMVECTOR{ 
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

	ImGui::Text("Edit Count: %d", GetEditCount());

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



RainDemo::RainDemo()
{
	// Init drops array
	m_RainDrops.resize(m_RainDropCount);
	for (auto& drop : m_RainDrops)
	{
		const float initialHeight = Random::Float(m_FloorHeight, m_CloudHeight);

		drop.Mass = Random::Float(0.1f, 0.3f);
		drop.Radius = 0.01f;
		drop.BlendFactor = 0.0f;

		drop.Position = {
			Random::Float(1.0f - m_Dimensions, m_Dimensions - 1.0f),
			initialHeight,
			Random::Float(1.0f - m_Dimensions, m_Dimensions - 1.0f)
		};
		const float v2 = 2.0f * m_Gravity * (initialHeight - m_CloudHeight);
		drop.Velocity = -sqrtf(fabsf(v2));
	}

	m_Clouds.resize(m_CloudCount);
	for (auto& cloud : m_Clouds)
	{
		cloud.Position = {
			Random::Float(-m_Dimensions, m_Dimensions),
			m_CloudHeight + Random::Float(-0.25f, 1.5f),
			Random::Float(-m_Dimensions, m_Dimensions)
		};
		cloud.Radius = Random::Float(0.3f, 0.8f);

		cloud.Frequency = Random::Float(0.7f, 1.2f);
		cloud.Scale = Random::Float(0.1f, 0.3f);
		cloud.Offset = Random::Float(2.0f * XM_PI);
	}
}


SDFEditList RainDemo::BuildEditList(float deltaTime)
{
	m_Time += deltaTime;

	// 4 extra edits for floor ceiling, and subtractions
	// above ceiling and below floor to stop leaking through
	SDFEditList editList(m_RainDropCount + m_CloudCount + 1, 10.0f);

	// Create floor
	editList.AddEdit(SDFEdit::CreateBox({ 0.0f, m_FloorHeight, 0.0f }, { m_Dimensions, 0.5f, m_Dimensions }));

	// Create clouds
	for (const auto& cloud : m_Clouds)
	{
		const float r = cloud.Radius + cloud.Scale * sinf(cloud.Frequency * m_Time + cloud.Offset);
		editList.AddEdit(SDFEdit::CreateSphere(cloud.Position, r, SDF_OP_SMOOTH_UNION, m_CloudBlend));
	}

	// Each rain drop should move under gravity
	for (auto& drop : m_RainDrops)
	{
		drop.Radius = min(m_MaxRadius, drop.Radius + 0.5f * drop.Mass * deltaTime);
		drop.BlendFactor = min(1.0f, drop.BlendFactor + deltaTime);

		drop.Velocity += drop.Mass * m_Gravity * deltaTime;
		drop.Position.y += drop.Velocity * deltaTime;

		// Wrap around
		if (drop.Position.y < m_FloorHeight)
		{
			drop.Velocity = 0.0f;
			drop.Position.y = m_CloudHeight;

			drop.Radius = 0.01f;
			drop.BlendFactor = 0.0f;
		}

		// Create the drop
		editList.AddEdit(SDFEdit::CreateSphere(drop.Position, drop.Radius, SDF_OP_SMOOTH_UNION, m_RainDropBlend * drop.BlendFactor));
	}

	return editList;
}

void RainDemo::DisplayGUI()
{
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(255, 255, 0)));
	ImGui::Text("Stats");
	ImGui::PopStyleColor();

	ImGui::Separator();

	ImGui::Text("Edit Count: %d", GetEditCount());

	ImGui::Separator();

	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(255, 255, 0)));
	ImGui::Text("Controls");
	ImGui::PopStyleColor();

	ImGui::Separator();

	ImGui::SliderFloat("Gravity", &m_Gravity, -75.0f, -5.0f);
	ImGui::SliderFloat("Rain Blending", &m_RainDropBlend, 0.0f, 1.0f);
	ImGui::SliderFloat("Cloud Blending", &m_CloudBlend, 0.0f, 1.0f);

	ImGui::Separator();
}


FractalDemo::FractalDemo()
{
	for (UINT x = 0; x < m_FractalGridSize; x++)
	for (UINT y = 0; y < m_FractalGridSize; y++)
	for (UINT z = 0; z < m_FractalGridSize; z++)
	{
		const auto fx = static_cast<float>(x);
		const auto fy = static_cast<float>(y);
		const auto fz = static_cast<float>(z);
		const auto fGridSize = static_cast<float>(m_FractalGridSize);

		m_FractalData.push_back({});
		auto& fractalData = m_FractalData.back();
		fractalData.Rotation = XMFLOAT3{
			Random::Float(0.0f, 360.0f),
			Random::Float(0.0f, 360.0f),
			Random::Float(0.0f, 360.0f)
		};
		fractalData.DeltaRotation = XMFLOAT3{
			Random::Float(-2.0f, 2.0f),
			Random::Float(-2.0f, 2.0f),
			Random::Float(-2.0f, 2.0f)
		};
		fractalData.Position = XMVECTOR{
			(fx - 0.5f * fGridSize + 0.5f),
			(fy - 0.5f * fGridSize + 0.5f),
			(fz - 0.5f * fGridSize + 0.5f),
		};
	}
}

SDFEditList FractalDemo::BuildEditList(float deltaTime)
{
	SDFEditList editList(m_FractalGridSize * m_FractalGridSize * m_FractalGridSize, 12.0f);

	for (UINT x = 0; x < m_FractalGridSize; x++)
	for (UINT y = 0; y < m_FractalGridSize; y++)
	for (UINT z = 0; z < m_FractalGridSize; z++)
	{
		const UINT i = (z * m_FractalGridSize + y) * m_FractalGridSize + x;
		auto& data = m_FractalData.at(i);

		data.Rotation.x += deltaTime * data.DeltaRotation.x;
		data.Rotation.y += deltaTime * data.DeltaRotation.y;
		data.Rotation.z += deltaTime * data.DeltaRotation.z;

		Transform t;
		t.SetTranslation(m_Spacing * data.Position);
		t.SetPitch(data.Rotation.x);
		t.SetYaw(data.Rotation.y);
		t.SetRoll(data.Rotation.z);

		editList.AddEdit(SDFEdit::CreateFractal(t, SDF_OP_SMOOTH_UNION, m_Blending));
	}

	return editList;
}

void FractalDemo::DisplayGUI()
{
	ImGui::DragFloat("Spacing", &m_Spacing, 0.01f);
	ImGui::SliderFloat("Blending", &m_Blending, 0.0f, 1.0f);
}



TestDemo::TestDemo()
{
	
}

SDFEditList TestDemo::BuildEditList(float deltaTime)
{
	SDFEditList editList(1024, 10.0f);

	editList.AddEdit(SDFEdit::CreateBox({}, { 1.0f, 1.0f, 1.0f }));

	return editList;
}

void TestDemo::DisplayGUI()
{
	
}
