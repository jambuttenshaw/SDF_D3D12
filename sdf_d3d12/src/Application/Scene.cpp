#include "pch.h"
#include "Scene.h"

#include "SDF/SDFEditList.h"
#include "imgui.h"

#include "Framework/Math.h"

#include "pix3.h"
#include "Renderer/D3DDebugTools.h"

#include "Demos.h"


// For debug output
// convert wstring to UTF-8 string
#pragma warning( push )
#pragma warning( disable : 4244)
std::string wstring_to_utf8(const std::wstring& str)
{
	return { str.begin(), str.end() };
}
#pragma warning( pop ) 


Scene::Scene()
{
	m_CurrentPipelineName = m_EnableEditCulling ? L"Default" : L"NoEditCulling";

	// Build SDF Object
	{
		// Create SDF factory 
		m_Factory = std::make_unique<SDFFactoryHierarchicalAsync>();

		// Create SDF objects
		m_BlobObject = std::make_unique<SDFObject>(0.1f, 200'000);

		BuildEditList(0.0f, false);
	}

	{
		// Construct scene geometry
		m_SceneGeometry.push_back({ L"Blobs", m_BlobObject.get()});

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

		m_InstanceRotation = XMMatrixRotationRollPitchYaw(
			XMConvertToRadians(Random::Float(-10.0f, 10.0f)),
			XMConvertToRadians(Random::Float(360.0f)),
			0.0f
		);
		m_InstanceRotationDelta = {
			0.0f,
			0.2f,
			0.0f
		};

		const auto& geoName = m_SceneGeometry.at(0).Name;
		m_AccelerationStructure->AddBottomLevelASInstance(geoName, m_InstanceRotation, 1);

		// Init top level AS
		m_AccelerationStructure->InitializeTopLevelAS(buildFlags, true, true, L"Top Level Acceleration Structure");
	}
}

Scene::~Scene()
{
	// SDFFactory should be deleted first
	m_Factory.reset();
}


void Scene::OnUpdate(float deltaTime)
{
	PIXBeginEvent(PIX_COLOR_INDEX(9), L"Scene Update");

	deltaTime *= !m_Paused;

	// Manipulate objects in the scene
	if (m_RotateInstances)
	{
		// Update rotation
		m_InstanceRotation = m_InstanceRotation * XMMatrixRotationRollPitchYaw(
			m_InstanceRotationDelta.x * deltaTime,
			m_InstanceRotationDelta.y * deltaTime, 
			m_InstanceRotationDelta.z * deltaTime);

		m_AccelerationStructure->SetInstanceTransform(0, m_InstanceRotation);
	}

	if (m_Rebuild)
	{
		BuildEditList(deltaTime * m_TimeScale, m_AsyncConstruction);
	}

	PIXEndEvent();
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
		ImGui::MenuItem("Drops Demo", nullptr, &m_DisplayDemoGui);

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

		if (ImGui::Button("PIX Capture"))
		{
			D3DDebugTools::PIXGPUCaptureFrame(1);
		}

		ImGui::Separator();

		ImGui::Checkbox("Rotate Instances", &m_RotateInstances);
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


		ImGui::End();
	}

	if (m_DisplayDemoGui)
	{
		m_DisplayDemoGui = DropsDemo::Get().DisplayGUI();
	}

	return open;
}


void Scene::BuildEditList(float deltaTime, bool async)
{
	const SDFEditList editList = DropsDemo::Get().BuildEditList(deltaTime);

	if (async)
	{
		m_Factory->BakeSDFAsync(m_CurrentPipelineName, m_BlobObject.get(), editList);
	}
	else
	{
		m_Factory->BakeSDFSync(m_CurrentPipelineName, m_BlobObject.get(), editList);
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
	ImGui::Text("Brick Pool Size (KB): %d", object->GetBrickPoolSizeBytes(SDFObject::RESOURCES_READ) / 1024);

	const float poolUsage = 100.0f * (static_cast<float>(object->GetBrickCount(SDFObject::RESOURCES_READ)) / static_cast<float>(object->GetBrickPoolCapacity(SDFObject::RESOURCES_READ)));
	ImGui::Text("Brick Pool Usage: %.1f", poolUsage);

	const auto sizeKB = object->GetTotalMemoryUsageBytes() / 1024;
	if (sizeKB > 10'000)
		ImGui::Text("Total Size (MB): %d", sizeKB / 1024);
	else
		ImGui::Text("Total Size (KB): %d", sizeKB);

	ImGui::Separator();
}

void Scene::DisplayAccelerationStructureDebugInfo() const
{
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(255, 255, 0)));
	ImGui::Text("Acceleration Structure");
	ImGui::PopStyleColor();

	ImGui::Separator();
	{
		const auto& tlas = m_AccelerationStructure->GetTopLevelAccelerationStructure();
		ImGui::Text("Top Level Size: %d KB", tlas.GetResourcesSize() / 1024);
	}

	for (const auto& blasGeometry : m_SceneGeometry)
	{
		auto name = wstring_to_utf8(blasGeometry.Name);
		const auto& blas = m_AccelerationStructure->GetBottomLevelAccelerationStructure(blasGeometry.Name);
		ImGui::Text("Bottom Level (%s) Size: %d KB", name.c_str(), blas.GetResourcesSize() / 1024);
	}
}
