#include "pch.h"
#include "Scene.h"

#include "SDF/SDFEditList.h"
#include "imgui.h"

#include "Framework/Math.h"

#include "D3DApplication.h"
#include "Input/InputManager.h"

#include "pix3.h"

#include "Demos.h"


// For debug output
// convert wstring to UTF-8 string
#pragma warning( push )
#pragma warning( disable : 4244)
static std::string wstring_to_utf8(const std::wstring& str)
{
	return { str.begin(), str.end() };
}
#pragma warning( pop ) 


Scene::Scene(D3DApplication* application, const std::string& demoName, float brickSize)
	: m_Application(application)
{
	ASSERT(m_Application, "Invalid app.");

	m_CurrentDemo = BaseDemo::GetDemoFromName(demoName);
	ASSERT(m_CurrentDemo, "Failed to load current demo.");
		
	m_CurrentPipelineName = m_EnableEditCulling ? L"Default" : L"NoEditCulling";

	// Build SDF Object
	{
		// Create SDF factory 
		m_Factory = std::make_unique<SDFFactoryHierarchicalAsync>();

		// Create SDF objects
		m_Object = std::make_unique<SDFObject>(brickSize, 500'000);

		BuildEditList(0.0f, false);
	}

	{
		// Construct scene geometry
		m_SceneGeometry.push_back({ L"Blobs", m_Object.get()});

		CheckSDFGeometryUpdates();
	}

	{
		// Set up acceleration structure
		constexpr D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		m_AccelerationStructure = std::make_unique<RaytracingAccelerationStructureManager>(1);

		for (const auto& geometry : m_SceneGeometry)
		{
			m_AccelerationStructure->AddBottomLevelAS(buildFlags, geometry, true, true);
		}

		const auto& geoName = m_SceneGeometry.at(0).Name;
		m_AccelerationStructure->AddBottomLevelASInstance(geoName, XMMatrixIdentity(), 1);

		// Init top level AS
		m_AccelerationStructure->InitializeTopLevelAS(buildFlags, true, true, L"Top Level Acceleration Structure");
	}

	auto SetupMaterial = [this](UINT mat, UINT slot, const XMFLOAT3& albedo, float roughness, float metalness)
	{
			const auto pMat = m_Application->GetMaterialManager()->GetMaterial(mat);
			pMat->SetAlbedo(albedo);
			pMat->SetRoughness(roughness);
			pMat->SetMetalness(metalness);
			m_Object->SetMaterial(pMat, slot);
	};

	SetupMaterial(0, 0, { 0.0f, 0.6f, 0.9f }, 0.4f, 0.0f);
	SetupMaterial(1, 1, { 0.9f, 0.4f, 0.8f }, 0.4f, 0.0f);
	SetupMaterial(2, 2, { 0.5f, 1.0f, 0.3f }, 0.4f, 0.0f);
	SetupMaterial(3, 3, { 1.0f, 1.0f, 1.0f }, 0.4f, 0.0f);
}

Scene::~Scene()
{
	// SDFFactory should be deleted first
	m_Factory.reset();
}


void Scene::Reset(const std::string& demoName, float brickSize)
{
	m_CurrentDemo = BaseDemo::GetDemoFromName(demoName);
	ASSERT(m_CurrentDemo, "Failed to load current demo.");

	m_Object->SetNextRebuildBrickSize(brickSize);
	m_Rebuild = true;
}


void Scene::OnUpdate(float deltaTime)
{
	PIXBeginEvent(PIX_COLOR_INDEX(9), L"Scene Update");


	// Space toggles pausing
	const auto inputManager = m_Application->GetInputManager();
	if (inputManager->IsKeyPressed(KEY_SPACE))
	{
		m_Paused = !m_Paused;
	}

	bool rebuildOnce = false;
	float timeDirection = 1.0f;
	if (m_Paused)
	{
		if (inputManager->IsKeyPressed(KEY_RIGHT))
		{
			rebuildOnce = true;
		}
		else if (inputManager->IsKeyPressed(KEY_LEFT))
		{
			rebuildOnce = true;
			timeDirection = -1.0f;
		}
	}

	deltaTime = deltaTime * m_TimeScale * timeDirection * static_cast<float>(!m_Paused || rebuildOnce);

	if (m_Rebuild || rebuildOnce)
	{
		BuildEditList(deltaTime, m_AsyncConstruction);
	}

	PIXEndEvent();

	ReportCounters();
}

void Scene::PreRender()
{
	PIXBeginEvent(PIX_COLOR_INDEX(6), L"Scene Render");

	// Check if the geometry should have its resources flipped
	CheckSDFGeometryUpdates();

	UpdateAccelerationStructure();

	PIXEndEvent();
}


void Scene::ImGuiSceneMenu()
{
	if (ImGui::BeginMenu("Scene"))
	{
		ImGui::MenuItem("Display Demo", nullptr, &m_DisplayDemoGui);

		ImGui::EndMenu();
	}
}

bool Scene::ImGuiSceneInfo()
{
	bool open = true;
	if (ImGui::Begin("Scene", &open))
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(255, 255, 0)));
		ImGui::Text("Scene");
		ImGui::PopStyleColor();

		ImGui::Separator();

		ImGui::Text("Instance Count: %d", 1);

		ImGui::Separator();

		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(255, 255, 0)));
		ImGui::Text("Controls");
		ImGui::PopStyleColor();

		ImGui::Separator();

		{
			static bool checkbox = m_Rebuild;
			m_Rebuild = ImGui::Button("Rebuild Once");
			ImGui::Checkbox("Rebuild", &checkbox);
			m_Rebuild |= checkbox;
		}
		ImGui::Checkbox("Async Construction", &m_AsyncConstruction);
		ImGui::Text("Async Builds/Sec: %.1f", m_AsyncConstruction ? m_Factory->GetAsyncBuildsPerSecond() : 0.0f);
		{
			int maxIterations = static_cast<int>(m_Factory->GetMaxBrickBuildIterations());
			if (ImGui::InputInt("Max Brick Build Iterations", &maxIterations))
			{
				if (maxIterations < -1)
					maxIterations = -1;
				m_Factory->SetMaxBrickBuildIterations(maxIterations);
			}
		}

		ImGui::Separator();

		float brickSize = m_Object->GetNextRebuildBrickSize();
		if (ImGui::SliderFloat("Brick Size", &brickSize, 0.025f, 1.0f))
		{
			m_Object->SetNextRebuildBrickSize(brickSize);
		}

		ImGui::Separator();

		if (ImGui::Checkbox("Edit Culling", &m_EnableEditCulling))
		{
			m_CurrentPipelineName = m_EnableEditCulling ? L"Default" : L"NoEditCulling";
		}

		ImGui::Separator();

		ImGui::DragFloat("Time Scale", &m_TimeScale, 0.01f);
		ImGui::Checkbox("Paused", &m_Paused);

		ImGui::Separator();

		for (auto&[name, geometry] : m_SceneGeometry)
			DisplaySDFObjectDebugInfo(name.c_str(), geometry);
		DisplayAccelerationStructureDebugInfo();


	}
	ImGui::End();

	if (ImGui::Begin("Demo", &m_DisplayDemoGui))
	{
		static char demoName[128];
		ImGui::InputText("Demo", demoName, 128);
		if (ImGui::Button("Change Demo"))
		{
			if (const auto demo = BaseDemo::GetDemoFromName(demoName))
				m_CurrentDemo = demo;
		}
		ImGui::Separator();

		m_CurrentDemo->DisplayGUI();
	}
	ImGui::End();

	return open;
}


UINT Scene::GetCurrentBrickCount() const
{
	return m_Object->GetBrickCount(SDFObject::RESOURCES_READ);
}

UINT Scene::GetDemoEditCount() const
{
	return m_CurrentDemo ? m_CurrentDemo->GetEditCount() : 0;
}



void Scene::BuildEditList(float deltaTime, bool async)
{
	const SDFEditList editList = m_CurrentDemo->BuildEditList(deltaTime);

	if (async)
	{
		m_Factory->BakeSDFAsync(m_CurrentPipelineName, m_Object.get(), editList);
	}
	else
	{
		m_Factory->BakeSDFSync(m_CurrentPipelineName, m_Object.get(), editList);
	}
}


void Scene::ReportCounters() const
{
	for (auto& [name, object] : m_SceneGeometry)
	{
		// Report object counters
		std::wstring counterName = name + L" Brick Count";
		PIXReportCounter(counterName.c_str(), static_cast<float>(object->GetBrickCount(SDFObject::RESOURCES_READ)));
	}
}


void Scene::CheckSDFGeometryUpdates()
{
	for (auto&[name, object] : m_SceneGeometry)
	{
		// Check if the new object resources have been computed
		if (object->GetResourcesState(SDFObject::RESOURCES_WRITE) == SDFObject::COMPUTED)
		{
			// if WRITE resources have been COMPUTED

			// set READ resources to SWITCHING
			object->SetResourceState(SDFObject::RESOURCES_READ, SDFObject::SWITCHING);

			// flip READ and WRITE resources
			PIXSetMarker(PIX_COLOR_INDEX(23), L"Flip resources");
			object->FlipResources();
		}
		else if (object->GetResourcesState(SDFObject::RESOURCES_WRITE) == SDFObject::SWITCHING)
		{
			object->SetResourceState(SDFObject::RESOURCES_WRITE, SDFObject::READY_COMPUTE);
		}

		// This object is about to be rendered - set the state appropriately
		object->SetResourceState(SDFObject::RESOURCES_READ, SDFObject::RENDERING);
	}
}



void Scene::UpdateAccelerationStructure()
{
	// Update geometry
	for (const auto& geo : m_SceneGeometry)
		m_AccelerationStructure->UpdateBottomLevelASGeometry(geo);

	// Rebuild
	m_AccelerationStructure->Build();
}


void Scene::DisplaySDFObjectDebugInfo(const wchar_t* name, const SDFObject* object) const
{
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(255, 255, 0)));
	ImGui::Text(wstring_to_utf8(name).c_str());
	ImGui::PopStyleColor();

	ImGui::Text("Brick Count: %d", object->GetBrickCount(SDFObject::RESOURCES_READ));

	const auto brickPoolSize = object->GetBrickPoolDimensions(SDFObject::RESOURCES_READ);
	ImGui::Text("Brick Pool Size (bricks): %d, %d, %d", brickPoolSize.x, brickPoolSize.y, brickPoolSize.z);
	const float poolUsage = 100.0f * (static_cast<float>(object->GetBrickCount(SDFObject::RESOURCES_READ)) / static_cast<float>(object->GetBrickPoolCapacity(SDFObject::RESOURCES_READ)));
	ImGui::Text("Brick Pool Usage: %.1f", poolUsage);

	ImGui::Separator();

	// Memory usage
	auto DisplaySize = [](const char* label, UINT64 sizeKB)
	{
			if (sizeKB > 10'000)
				ImGui::Text("%s Size (MB): %d", label, sizeKB / 1024);
			else
				ImGui::Text("%s Size (KB): %d", label, sizeKB);
	};

	DisplaySize("Brick Pool", object->GetBrickPoolSizeBytes() / 1024);
	DisplaySize("Brick Buffer", object->GetBrickBufferSizeBytes() / 1024);
	DisplaySize("AABB Buffer", object->GetAABBBufferSizeBytes() / 1024);
	DisplaySize("Index Buffer", object->GetIndexBufferSizeBytes() / 1024);

	ImGui::Separator();

	DisplaySize("Total", object->GetTotalMemoryUsageBytes() / 1024);

	ImGui::Separator();
}

void Scene::DisplayAccelerationStructureDebugInfo() const
{
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(255, 255, 0)));
	ImGui::Text("Acceleration Structure");
	ImGui::PopStyleColor();

	auto DisplaySize = [](const char* label, UINT64 sizeKB)
	{
		if (sizeKB > 10'000)
			ImGui::Text("%s Size (MB): %d", label, sizeKB / 1024);
		else
			ImGui::Text("%s Size (KB): %d", label, sizeKB);
	};

	ImGui::Separator();
	{
		const auto& tlas = m_AccelerationStructure->GetTopLevelAccelerationStructure();
		DisplaySize("Top Level AS", tlas.GetResourcesSize() / 1024);
	}

	for (const auto& blasGeometry : m_SceneGeometry)
	{
		const auto& blas = m_AccelerationStructure->GetBottomLevelAccelerationStructure(blasGeometry.Name);
		DisplaySize("Bottom Level AS", blas.GetResourcesSize() / 1024);
	}
}
