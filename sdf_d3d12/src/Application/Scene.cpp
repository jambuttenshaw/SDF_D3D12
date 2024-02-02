#include "pch.h"
#include "Scene.h"

#include "SDF/SDFEditList.h"
#include "imgui.h"

#include "Framework/Math.h"

#include "pix3.h"


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
		m_SDFFactoryAsync = std::make_unique<SDFFactoryAsync>();

		// Create SDF objects
		m_DynamicObject = std::make_unique<SDFObject>(0.0625f, 65536);
		m_TorusObject = std::make_unique<SDFObject>(0.0625f, 65536);
		m_SphereObject = std::make_unique<SDFObject>(0.0625f, 65536);
		m_OctahedronObject = std::make_unique<SDFObject>(0.0625f, 65536);


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
			m_SDFFactory->BakeSDFSync(m_TorusObject.get(), editList);
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
			
			m_SDFFactoryAsync->BakeSDFSync(m_TorusObject.get(), std::move(torusEditList));
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
			m_SDFFactoryAsync->BakeSDFSync(m_SphereObject.get(), std::move(sphereEditList));
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

			BuildEditList(0.0f, false);
		}
		{
			for (UINT i = 0; i < m_OctahedronCount; i++)
			{
				m_OctahedronData.push_back({});
				OctahedronData& data = m_OctahedronData.back();
				data.offset = {
					Random::Float(-0.7f, 0.7f),
					0,
					Random::Float(-0.7f, 0.7f)
				};
				data.range = Random::Float(0.3f, 0.8f);
				data.speed = Random::Float(0.5f, 2.0f);
				data.scale = Random::Float(0.01f, 0.1f);
			}
			BuildEditList2(0.0f, false);
		}
	}

	{
		// Construct scene geometry
		m_SceneGeometry.push_back({ L"Dynamic", m_DynamicObject.get()});
		m_SceneGeometry.push_back({ L"Torus", m_TorusObject.get() });
		m_SceneGeometry.push_back({ L"Spheres", m_SphereObject.get() });
		m_SceneGeometry.push_back({ L"Octahedron", m_OctahedronObject.get() });
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

	CheckSDFGeometryUpdates();
}

Scene::~Scene()
{
	// SDFFactory should be deleted first
	m_SDFFactoryAsync.reset();
}


void Scene::OnUpdate(float deltaTime)
{
	PIXBeginEvent(PIX_COLOR_INDEX(9), L"Scene Update");

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
		BuildEditList(deltaTime, m_AsyncConstruction);
		BuildEditList2(deltaTime, m_AsyncConstruction);
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

		ImGui::Checkbox("Rotate Instances", &m_RotateInstances);
		ImGui::Separator();

		{
			static bool checkbox = false;
			m_Rebuild = ImGui::Button("Rebuild Once");
			ImGui::Checkbox("Rebuild", &checkbox);
			m_Rebuild |= checkbox;
		}
		ImGui::Checkbox("Async Construction", &m_AsyncConstruction);
		ImGui::Text("Async FPS: %.1f", m_AsyncConstruction ? m_SDFFactoryAsync->GetAsyncFPS() : 0.0f);

		ImGui::Separator();

		ImGui::SliderFloat("Sphere Blend", &m_SphereBlend, 0.0f, 0.5f);
		ImGui::SliderFloat("Oct Blend", &m_OctahedronBlend, 0.0f, 0.5f);

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
	t += deltaTime;

	SDFEditList editList(m_SphereCount + 1);

	editList.Reset();
	editList.AddEdit(SDFEdit::CreateBoxFrame({}, { 1.0f, 1.0f, 1.0f }, 0.025f));

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

	if (async)
	{
		m_SDFFactoryAsync->BakeSDFAsync(m_DynamicObject.get(), std::move(editList));
	}
	else
	{
		m_SDFFactoryAsync->BakeSDFSync(m_DynamicObject.get(), std::move(editList));
	}

	PIXEndEvent();
}

void Scene::BuildEditList2(float deltaTime, bool async)
{
	PIXBeginEvent(PIX_COLOR_INDEX(31), L"Build Edit List 2");

	static float t = 0.0f;
	t += deltaTime;

	SDFEditList editList(m_OctahedronCount + 2);

	editList.Reset();
	editList.AddEdit(SDFEdit::CreateBox({}, { 0.75f, 0.05f, 0.75f }));

	for (UINT i = 0; i < m_OctahedronCount; i++)
	{
		Transform transform;
		const OctahedronData& data = m_OctahedronData.at(i);
		transform.SetTranslation({
			data.offset.x,
			data.offset.y + data.range * sinf(t * data.speed),
			data.offset.z,
		});
		
		editList.AddEdit(SDFEdit::CreateOctahedron(transform, data.scale, SDFOperation::SmoothUnion, m_OctahedronBlend));
	}

	const float r = 0.3f + 0.2f * sinf(t);
	editList.AddEdit(SDFEdit::CreateSphere({}, r, SDFOperation::SmoothSubtraction, m_OctahedronBlend));

	if (async)
	{
		m_SDFFactoryAsync->BakeSDFAsync(m_OctahedronObject.get(), std::move(editList));
	}
	else
	{
		m_SDFFactoryAsync->BakeSDFSync(m_OctahedronObject.get(), std::move(editList));
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
