#include "pch.h"
#include "Scene.h"

#include "SDF/SDFEditList.h"
#include "imgui.h"

#include "Framework/Math.h"

#include "pix3.h"
#include "Renderer/D3DDebugTools.h"


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
	// Build SDF Object
	{
		// Create SDF factory 
		m_Factory = std::make_unique<SDFFactoryHierarchicalAsync>();

		// Create SDF objects
		m_BlobObject = std::make_unique<SDFObject>(0.05f, 64000);
		{
			Random::Seed(0);
			for (UINT i = 0; i < m_SphereCount; i++)
			{
				m_SphereData.push_back({});
				SphereData& sphereData = m_SphereData.at(i);
				sphereData.scale = {
					Random::Float(-1.0f, 1.0f),
					Random::Float(-1.0f, 1.0f),
					Random::Float(-1.0f, 1.0f)	
				};
				sphereData.speed = {
					Random::Float(-1.0f, 1.0f),
					Random::Float(-1.0f, 1.0f),
					Random::Float(-1.0f, 1.0f)
				};
			}

			BuildEditList(0.0f, false);
		}
	}

	{
		// Construct scene geometry
		m_SceneGeometry.push_back({ L"Blobs", m_BlobObject.get()});

		CheckSDFGeometryUpdates();
	}

	{
		// Set up acceleration structure
		constexpr D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		m_AccelerationStructure = std::make_unique<RaytracingAccelerationStructureManager>(s_InstanceCount);

		for (const auto& geometry : m_SceneGeometry)
		{
			m_AccelerationStructure->AddBottomLevelAS(buildFlags, geometry, true, true);
		}

		// Create instances of BLAS
		for (UINT i = 0; i < s_InstanceCount; i++)
		{
				auto rotation = XMMatrixRotationRollPitchYaw(
					XMConvertToRadians(Random::Float(360.0f)),
					XMConvertToRadians(Random::Float(360.0f)),
					XMConvertToRadians(Random::Float(360.0f))
				);
				m_InstanceRotations[i] = rotation;
				m_InstanceRotationDeltas[i] = {
					Random::Float(-0.2f, 0.2f),
					Random::Float(-0.2f, 0.2f),
					Random::Float(-0.2f, 0.2f)
				};

				const auto& geoName = m_SceneGeometry.at(i % m_SceneGeometry.size()).Name;
				m_AccelerationStructure->AddBottomLevelASInstance(geoName, rotation, 1);
		}

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

	// Manipulate objects in the scene
	if (m_RotateInstances)
	{
		for (UINT i = 0; i < s_InstanceCount; i++)
		{
			// Update rotation
			auto rotation = m_InstanceRotations[i];
			auto d = m_InstanceRotationDeltas[i];
			rotation = rotation * XMMatrixRotationRollPitchYaw(d.x * deltaTime, d.y * deltaTime, d.z * deltaTime);
			m_InstanceRotations[i] = rotation;

			m_AccelerationStructure->SetInstanceTransform(i, rotation);
		}
	}

	if (m_Rebuild)
	{
		BuildEditList(deltaTime, m_AsyncConstruction);
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

void Scene::PostRender()
{
	// Rendering has completed
	// It's important that this is set so that a flip can be triggered next frame, if an async factory has been working on the write resources
	for (auto&[name, geometry] : m_SceneGeometry)
		geometry->SetResourceState(SDFObject::RESOURCES_READ, SDFObject::RENDERED);
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

		ImGui::Text("Instance Count: %d", s_InstanceCount);

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
			static bool checkbox = false;
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

		ImGui::DragFloat("Time Scale", &m_TimeScale, 0.01f);
		ImGui::SliderFloat("Sphere Blend", &m_SphereBlend, 0.0f, 0.5f);

		ImGui::Separator();

		for (auto&[name, geometry] : m_SceneGeometry)
			DisplaySDFObjectDebugInfo(name.c_str(), geometry);
		DisplayAccelerationStructureDebugInfo();


		ImGui::End();
	}

	return open;
}


void Scene::BuildEditList(float deltaTime, bool async)
{
	PIXBeginEvent(PIX_COLOR_INDEX(10), L"Build Edit List");

	static float t = 1.0f;
	t += deltaTime * m_TimeScale;

	SDFEditList editList(m_SphereCount);

	for (UINT i = 0; i < m_SphereCount; i++)
	{
		Transform transform;
		transform.SetTranslation(
			{
				m_SphereData.at(i).scale.x * cosf(m_SphereData.at(i).speed.x * t),
				m_SphereData.at(i).scale.y * cosf(m_SphereData.at(i).speed.y * t),
				m_SphereData.at(i).scale.z * cosf(m_SphereData.at(i).speed.z * t)
			});
		editList.AddEdit(SDFEdit::CreateSphere(transform, 0.05f, i == 0 ? SDFOperation::Union : SDFOperation::SmoothUnion, m_SphereBlend));
	}

	if (async)
	{
		m_Factory->BakeSDFAsync(m_BlobObject.get(), editList);
	}
	else
	{
		m_Factory->BakeSDFSync(m_BlobObject.get(), editList);
	}

	PIXEndEvent();
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
