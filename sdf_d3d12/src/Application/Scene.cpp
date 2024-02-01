#include "pch.h"
#include "Scene.h"

#include "SDF/SDFEditList.h"
#include "imgui.h"

#include "Framework/Math.h"


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
		m_SDFFactory = std::make_unique<SDFFactorySync>();
		//m_SDFFactoryAsync = std::make_unique<SDFFactoryAsync>();

		// Create SDF objects
		m_Object = std::make_unique<SDFObject>(0.25f, 65536);
		m_TorusObject = std::make_unique<SDFObject>(0.0625f, 65536);
		m_SphereObject = std::make_unique<SDFObject>(0.0625f, 65536);


		/*
		{
			SDFEditList editList(4);
			editList.AddEdit(SDFEdit::CreateBox(
				{ 0.0f, -0.25f, 0.0f },
				{ 0.4f, 0.4f, 0.4f }));
			editList.AddEdit(SDFEdit::CreateOctahedron({ 0.0f, -0.25f, 0.0f }, 0.6f));
			editList.AddEdit(SDFEdit::CreateSphere({ 0.0f, 0.25f, 0.0f }, 0.4f, SDFOperation::Subtraction));

			// Add a torus on top
			Transform torusTransform(0.0f, 0.25f, 0.0f);
			torusTransform.SetPitch(XMConvertToRadians(90.0f));
			editList.AddEdit(SDFEdit::CreateTorus(torusTransform, 0.25f, 0.05f, SDFOperation::SmoothUnion, 0.3f));
			m_SDFFactory->BakeSDFSynchronous(m_TorusObject.get(), editList);
		}
		*/

		{
			SDFEditList torusEditList(4);
			// Create torus object edit list
			torusEditList.AddEdit(SDFEdit::CreateTorus({}, 0.85f, 0.05f));

			Transform torusTransform;
			torusTransform.SetPitch(XMConvertToRadians(90.0f));

			torusEditList.AddEdit(SDFEdit::CreateTorus(torusTransform, 0.85f, 0.05f, SDFOperation::SmoothUnion, 0.1f));

			torusTransform.SetPitch(XMConvertToRadians(0.0f));
			torusTransform.SetRoll(XMConvertToRadians(90.0f));

			torusEditList.AddEdit(SDFEdit::CreateTorus(torusTransform, 0.85f, 0.05f, SDFOperation::SmoothUnion, 0.1f));

			torusEditList.AddEdit(SDFEdit::CreateOctahedron({}, 0.7f));
			
			//m_SDFFactory->BakeSDFSynchronous(m_TorusObject.get(), torusEditList, true);
		}
		{
			// Create sphere object by adding and then subtracting a bunch of spheres
			constexpr UINT spheres = 256;

			SDFEditList sphereEditList(spheres);

			for (UINT i = 0; i < spheres; i++)
			{
				SDFOperation op = SDFOperation::Union;
				if (i > 0)
					op = i % 2 == 0 ? SDFOperation::SmoothSubtraction : SDFOperation::SmoothUnion;

				const float radius = Random::Float(0.1f, 0.4f);
				sphereEditList.AddEdit(SDFEdit::CreateSphere(
					{
							Random::Float(-0.8f + radius, 0.8f - radius),
							Random::Float(-0.8f + radius, 0.8f - radius),
							Random::Float(-0.8f + radius, 0.8f - radius)
					}, radius, op, 0.3f));
			}

			// Bake the primitives into the SDF object
			//m_SDFFactory->BakeSDFSynchronous(m_SphereObject.get(), sphereEditList, true);
		}
		{
			for (UINT i = 0; i < m_SphereCount; i++)
			{
				m_SphereData.push_back({});
				SphereData& sphereData = m_SphereData.at(i);
				sphereData.scale = {
					Random::Float(-0.7f, 0.7f),
					Random::Float(-0.7f, 0.7f),
					Random::Float(-0.7f, 0.7f)
				};
				sphereData.speed = {
					Random::Float(0.5f, 2.0f),
					Random::Float(0.5f, 2.0f),
					Random::Float(0.5f, 2.0f)
				};
			}

			BuildEditList(0.0f);
		}
	}

	{
		// Construct scene geometry
		m_SceneGeometry.push_back({ L"Dynamic" });
		//m_SceneGeometry.push_back({ L"Torus" });
		//m_SceneGeometry.push_back({ L"Spheres" });

		auto& dynamicGeometry = m_SceneGeometry.at(0);
		//auto& torusGeometry = m_SceneGeometry.at(1);
		//auto& spheresGeometry = m_SceneGeometry.at(2);

		dynamicGeometry.GeometryInstances.push_back(m_Object.get());
		//torusGeometry.GeometryInstances.push_back(m_TorusObject.get());
		//spheresGeometry.GeometryInstances.push_back(m_SphereObject.get());
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
		for (UINT z = 0; z < s_InstanceGridDims; z++)
		{
			for (UINT y = 0; y < s_InstanceGridDims; y++)
			{
				for (UINT x = 0; x < s_InstanceGridDims; x++)
				{
					const UINT index = (z * s_InstanceGridDims + y) * s_InstanceGridDims + x;

					auto rotation = XMMatrixRotationRollPitchYaw(
						XMConvertToRadians(Random::Float(360.0f)),
						XMConvertToRadians(Random::Float(360.0f)),
						XMConvertToRadians(Random::Float(360.0f))
					);
					m_InstanceRotations[index] = rotation;
					m_InstanceRotationDeltas[index] = {
						Random::Float(-0.2f, 0.2f),
						Random::Float(-0.2f, 0.2f),
						Random::Float(-0.2f, 0.2f)
					};


					m_InstanceTranslation[index] = {
						Random::Float(-1.0f, 1.0f),
						Random::Float(-1.0f, 1.0f),
						Random::Float(-1.0f, 1.0f)
					};

					const auto translation = XMMatrixTranslation(
						x * s_InstanceSpacing + m_InstanceTranslation[index].x,
						y * s_InstanceSpacing + m_InstanceTranslation[index].y,
						z * s_InstanceSpacing + m_InstanceTranslation[index].z 
					);

					const auto& geoName = m_SceneGeometry.at(index % m_SceneGeometry.size()).Name;
					m_AccelerationStructure->AddBottomLevelASInstance(geoName, rotation * translation, 1);
				}
			}
		}

		// Init top level AS
		m_AccelerationStructure->InitializeTopLevelAS(buildFlags, true, true, L"Top Level Acceleration Structure");
	}

	CheckSDFGeometryUpdates(m_Object.get());
}


void Scene::OnUpdate(float deltaTime)
{
	// Manipulate objects in the scene
	if (m_RotateInstances)
	{
		for (UINT z = 0; z < s_InstanceGridDims; z++)
		for (UINT y = 0; y < s_InstanceGridDims; y++)
		for (UINT x = 0; x < s_InstanceGridDims; x++)
		{
			const UINT index = (z * s_InstanceGridDims + y) * s_InstanceGridDims + x;

			// Update rotation
			auto rotation = m_InstanceRotations[index];
			auto d = m_InstanceRotationDeltas[index];
			rotation = rotation * XMMatrixRotationRollPitchYaw(d.x * deltaTime, d.y * deltaTime, d.z * deltaTime);
			m_InstanceRotations[index] = rotation;

			const auto translation = XMMatrixTranslation(
				x * s_InstanceSpacing + m_InstanceTranslation[index].x,
				y * s_InstanceSpacing + m_InstanceTranslation[index].y,
				z * s_InstanceSpacing + m_InstanceTranslation[index].z
			);

			m_AccelerationStructure->SetInstanceTransform(index, rotation * translation);
		}
	}

	if (m_Rebuild)
	{
		BuildEditList(deltaTime);
		m_Rebuild = false;
	}
}

void Scene::OnRender()
{
	// Check if the geometry should have its resources flipped
	//CheckSDFGeometryUpdates(m_TorusObject.get());
	//CheckSDFGeometryUpdates(m_SphereObject.get());
	CheckSDFGeometryUpdates(m_Object.get());

	UpdateAccelerationStructure();
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

		ImGui::Checkbox("Rotate Instances", &m_RotateInstances);
		//ImGui::Checkbox("Rebuild", &m_Rebuild);
		ImGui::Separator();
		ImGui::SliderFloat("Blend", &m_SphereBlend, 0.0f, 0.5f);
		ImGui::Separator();
		m_Rebuild = ImGui::Button("Rebuild Once");

		ImGui::Separator();

		DisplaySDFObjectDebugInfo("Torus Object", m_TorusObject.get());
		DisplaySDFObjectDebugInfo("Sphere Object", m_SphereObject.get());
		DisplaySDFObjectDebugInfo("Sphere Object", m_Object.get());

		DisplayAccelerationStructureDebugInfo();


		ImGui::End();
	}

	return open;
}


void Scene::BuildEditList(float deltaTime)
{
	static float t = 1;
	t += deltaTime;

	SDFEditList editList(m_SphereCount + 1);

	editList.Reset();
	editList.AddEdit(SDFEdit::CreateBoxFrame({}, { 1.0f, 1.0f, 1.0f }, 0.05f));

	for (UINT i = 0; i < m_SphereCount; i++)
	{
		Transform transform;
		transform.SetTranslation(
			{
				m_SphereData.at(i).scale.x * cosf(m_SphereData.at(i).speed.x * t),
				m_SphereData.at(i).scale.y * cosf(m_SphereData.at(i).speed.y * t),
				m_SphereData.at(i).scale.z * cosf(m_SphereData.at(i).speed.z * t)
			});
		editList.AddEdit(SDFEdit::CreateSphere(transform, 0.05f, SDFOperation::SmoothUnion, m_SphereBlend));
	}

	m_SDFFactory->BakeSDFSynchronous(m_Object.get(), editList, true);
}


void Scene::CheckSDFGeometryUpdates(SDFObject* object)
{
	// Check if the new object resources have been computed
	if (object->GetResourcesState(SDFObject::RESOURCES_WRITE) == SDFObject::COMPUTED)
	{
		// if WRITE resources have been COMPUTED

		// set READ resources to SWITCHING
		//ASSERT(object->GetResourcesState(SDFObject::RESOURCES_READ) == SDFObject::RENDERING || object->GetResourcesState(SDFObject::RESOURCES_READ) == SDFObject::COMPUTED, "Invalid flip!");
		object->SetResourceState(SDFObject::RESOURCES_READ, SDFObject::READY_COMPUTE);
		// flip READ and WRITE resources
		object->FlipResources();
	}

	object->SetResourceState(SDFObject::RESOURCES_READ, SDFObject::RENDERING);
}



void Scene::UpdateAccelerationStructure()
{
	// Update geometry
	for (const auto& geo : m_SceneGeometry)
		m_AccelerationStructure->UpdateBottomLevelASGeometry(geo);

	// Rebuild
	m_AccelerationStructure->Build();
}


void Scene::DisplaySDFObjectDebugInfo(const char* name, const SDFObject* object) const
{
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(255, 255, 0)));
	ImGui::Text(name);
	ImGui::PopStyleColor();

	ImGui::Text("Brick Count: %d", object->GetBrickCount());

	const auto brickPoolSize = object->GetBrickPoolDimensions();
	ImGui::Text("Brick Pool Size (bricks): %d, %d, %d", brickPoolSize.x, brickPoolSize.y, brickPoolSize.z);
	ImGui::Text("Brick Pool Size (KB): %d", object->GetBrickPoolSizeBytes() / 1024);

	const float poolUsage = 100.0f * (static_cast<float>(object->GetBrickCount()) / static_cast<float>(object->GetBrickPoolCapacity()));
	ImGui::Text("Brick Pool Usage: %.1f", poolUsage);

	ImGui::Text("Total Size (KB): %d", object->GetTotalMemoryUsageBytes() / 1024);

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
