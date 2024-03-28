#include "pch.h"
#include "DemoScene.h"

#include "Demos.h"
#include "imgui.h"


DemoScene::DemoScene(D3DApplication* application)
	: Scene(application, 1)
{
	LoadDemo("drops", 0.1f);

	m_Geometry = std::make_unique<SDFObject>(m_BrickSize, 500'000);

	// Build geometry
	m_Application->GetSDFFactory()->BakeSDFSync(L"Default", m_Geometry.get(), m_CurrentDemo->BuildEditList(0.0f));
	m_Geometry->FlipResources();

	AddGeometry(L"DemoGeometry", m_Geometry.get());
	CreateGeometryInstance(L"DemoGeometry");

	auto SetupMaterial = [this](UINT mat, UINT slot, const XMFLOAT3& albedo, float roughness, float metalness, float reflectance)
		{
			const auto pMat = m_Application->GetMaterialManager()->GetMaterial(mat);
			pMat->SetAlbedo(albedo);
			pMat->SetRoughness(roughness);
			pMat->SetMetalness(metalness);
			pMat->SetReflectance(reflectance);
			m_Geometry->SetMaterial(pMat, slot);
		};

	SetupMaterial(0, 0, { 0.0f, 0.0f, 0.0f }, 0.1f, 0.0f, 0.0f);
	SetupMaterial(1, 1, { 1.0f, 1.0f, 1.0f }, 0.1f, 0.0f, 0.0f);
	SetupMaterial(2, 2, { 1.0f, 0.5f, 0.0f }, 0.1f, 0.0f, 0.0f);
	SetupMaterial(3, 3, { 0.0f, 0.9f, 0.9f }, 0.05f, 1.0f, 1.0f);
}

void DemoScene::OnUpdate(float deltaTime)
{
	if (m_Rebuild)
	{
		SDFFactoryHierarchicalAsync* factory = m_Application->GetSDFFactory();
		if (m_AsyncConstruction)
			factory->BakeSDFAsync(L"Default", m_Geometry.get(), m_CurrentDemo->BuildEditList(deltaTime));
		else
			factory->BakeSDFSync(L"Default", m_Geometry.get(), m_CurrentDemo->BuildEditList(deltaTime));
	}
}

bool DemoScene::DisplayGui()
{
	bool open = true;

	if (ImGui::Begin("Scene Controls", &open))
	{
		if (ImGui::SliderFloat("Brick Size", &m_BrickSize, 0.0625f, 1.0f))
		{
			m_Geometry->SetNextRebuildBrickSize(m_BrickSize);
		}

		ImGui::Separator();

		ImGui::Checkbox("Rebuild", &m_Rebuild);
		ImGui::Checkbox("Async Build", &m_AsyncConstruction);

		ImGui::Separator();

		static char demoName[128];
		ImGui::InputText("Demo", demoName, 128);
		if (ImGui::Button("Change Demo"))
		{
			if (const auto demo = BaseDemo::GetDemoFromName(demoName))
				m_CurrentDemo = demo;
		}

		ImGui::Separator();
	}
	ImGui::End();

	return open;
}

void DemoScene::LoadDemo(const std::string& name, float brickSize)
{
	m_CurrentDemo = BaseDemo::GetDemoFromName(name);
	m_BrickSize = brickSize;

	if (m_Geometry)
	{
		m_Geometry->SetNextRebuildBrickSize(m_BrickSize);
	}
}