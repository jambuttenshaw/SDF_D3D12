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
	: m_EditList(16)
{
	// Build SDF Object
	{
		// Create SDF factory 
		m_SDFFactory = std::make_unique<SDFFactory>();

		// Create an SDF object
		//m_TorusObject = std::make_unique<SDFObject>(0.05f, 65536);
		//m_SphereObject = std::make_unique<SDFObject>(0.05f, 65536);
		m_Object = std::make_unique<SDFObject>(0.05f, 65536);


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

		/*
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
			
			m_SDFFactory->BakeSDFSynchronous(m_TorusObject.get(), torusEditList);
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
			m_SDFFactory->BakeSDFSynchronous(m_SphereObject.get(), sphereEditList);
		}
		*/
		{
			const auto directQueue = g_D3DGraphicsContext->GetDirectCommandQueue();
			const auto computeQueue = g_D3DGraphicsContext->GetComputeCommandQueue();

			m_EditList.Reset();
			m_EditList.AddEdit(SDFEdit::CreateTorus({}, 0.75f, 0.2f));

			Transform transform;
			m_EditList.AddEdit(SDFEdit::CreateOctahedron(transform, 0.3f));

			transform.SetTranslation({ 0.6f, 0.0f, 0.0f });
			m_EditList.AddEdit(SDFEdit::CreateSphere(transform, 0.1f));

			computeQueue->InsertWaitForQueue(directQueue);
			m_SDFFactory->BakeSDFSynchronous(m_Object.get(), m_EditList);
			directQueue->InsertWaitForQueue(computeQueue);
		}
	}

	/*
	{
		// Construct scene geometry
		m_SceneGeometry.push_back({ L"Torus" });
		m_SceneGeometry.push_back({ L"Spheres" });

		auto& torusGeometry = m_SceneGeometry.at(0);
		auto& spheresGeometry = m_SceneGeometry.at(1);

		torusGeometry.GeometryInstances.push_back(m_TorusObject.get());
		spheresGeometry.GeometryInstances.push_back(m_SphereObject.get());
	}
	*/
	{
		// Construct scene geometry
		m_SceneGeometry.push_back({ L"Sphere" });
		auto& sphereGeometry = m_SceneGeometry.at(0);
		sphereGeometry.GeometryInstances.push_back(m_Object.get());
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

		const std::wstring sphereGeometry = L"Sphere";
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

					m_AccelerationStructure->AddBottomLevelASInstance(sphereGeometry, rotation * translation, 1);
				}
			}
		}

		// Init top level AS
		m_AccelerationStructure->InitializeTopLevelAS(buildFlags, true, true, L"Top Level Acceleration Structure");
	}
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

	static bool firstFrame = true;
	if (!firstFrame)
	{
		const auto directQueue = g_D3DGraphicsContext->GetDirectCommandQueue();
		const auto computeQueue = g_D3DGraphicsContext->GetComputeCommandQueue();

		static float t = 0;
		t += 0.1f * deltaTime;

		m_EditList.Reset();
		m_EditList.AddEdit(SDFEdit::CreateTorus({}, 0.75f, 0.2f));

		Transform transform;
		transform.SetYaw(t);
		m_EditList.AddEdit(SDFEdit::CreateOctahedron(transform, 0.3f));

		transform.SetTranslation({ 0.6f * sinf(t), 0.0f, 0.0f });
		m_EditList.AddEdit(SDFEdit::CreateSphere(transform, 0.1f));

		computeQueue->InsertWaitForQueue(directQueue);
		m_SDFFactory->BakeSDFSynchronous(m_Object.get(), m_EditList);
		directQueue->InsertWaitForQueue(computeQueue);
	}
	else
	{
		firstFrame = false;
	}

	ImGui::Begin("Scene");
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(255, 255, 0)));
		ImGui::Text("Scene");
		ImGui::PopStyleColor();

		ImGui::Separator();

		ImGui::Text("Instance Count: %d", s_InstanceCount);
		ImGui::Separator();

		//DisplaySDFObjectDebugInfo("Torus Object", m_TorusObject.get());
		//DisplaySDFObjectDebugInfo("Sphere Object", m_SphereObject.get());
		DisplaySDFObjectDebugInfo("Sphere Object", m_Object.get());

		DisplayAccelerationStructureDebugInfo();

		ImGui::Separator();

		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(255, 255, 0)));
		ImGui::Text("Controls");
		ImGui::PopStyleColor();

		ImGui::Separator();

		ImGui::Checkbox("Rotate Instances", &m_RotateInstances);
	}
	ImGui::End();

}

void Scene::OnRender()
{
	UpdateAccelerationStructure();
}


void Scene::UpdateAccelerationStructure()
{
	// Update geometry
	m_AccelerationStructure->UpdateBottomLevelASGeometry(m_SceneGeometry.at(0));

	// Rebuild
	m_AccelerationStructure->Build();
}


void Scene::DisplaySDFObjectDebugInfo(const char* name, const SDFObject* object) const
{
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(255, 255, 0)));
	ImGui::Text(name);
	ImGui::PopStyleColor();

	ImGui::Text("Brick Count: %d", object->GetBrickCount());

	const float aabbCull = 100.0f * (1.0f - static_cast<float>(object->GetBrickCount()) / static_cast<float>(object->GetBrickBufferCapacity()));
	ImGui::Text("Brick Cull: %.1f", aabbCull);

	const auto brickPoolSize = object->GetBrickPoolDimensions();
	ImGui::Text("Brick Pool Size (bricks): %d, %d, %d", brickPoolSize.x, brickPoolSize.y, brickPoolSize.z);
	ImGui::Text("Brick Pool Capacity: %d", object->GetBrickPoolCapacity());
	ImGui::Text("Brick Pool Size (KB): %d", object->GetBrickPoolSizeBytes() / 1024);

	const float poolUsage = 100.0f * (static_cast<float>(object->GetBrickCount()) / static_cast<float>(object->GetBrickPoolCapacity()));
	ImGui::Text("Brick Pool Usage: %.1f", poolUsage);

	ImGui::Text("AABB Buffer Size (KB): %d", object->GetAABBBufferSizeBytes() / 1024);
	ImGui::Text("Brick Buffer Size (KB): %d", object->GetBrickBufferSizeBytes() / 1024);

	ImGui::Separator();
}

void Scene::DisplayAccelerationStructureDebugInfo() const
{
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(255, 255, 0)));
	ImGui::Text("Acceleration Structure");
	ImGui::PopStyleColor();

	ImGui::Separator();
	{
		ImGui::Text("Top Level");

		const auto& tlas = m_AccelerationStructure->GetTopLevelAccelerationStructure();
		ImGui::Text("Size: %d KB", tlas.GetResourceSize() / 1024);
	}


	for (const auto& blasGeometry : m_SceneGeometry)
	{
		ImGui::Separator();

		auto name = wstring_to_utf8(blasGeometry.Name);
		ImGui::Text("Bottom Level - %s", name.c_str());

		const auto& blas = m_AccelerationStructure->GetBottomLevelAccelerationStructure(blasGeometry.Name);
		ImGui::Text("Size: %d KB", blas.GetResourceSize() / 1024);
	}
}
