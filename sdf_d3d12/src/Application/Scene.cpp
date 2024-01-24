#include "pch.h"
#include "Scene.h"

#include "imgui.h"

#include "Framework/Math.h"


Scene::Scene()
	: m_TorusEditList(4)
	, m_SphereEditList(64)
{
	// Build SDF Object
	{
		// Create SDF factory 
		m_SDFFactory = std::make_unique<SDFFactory>();

		// Create an SDF object
		m_TorusObject = std::make_unique<SDFObject>(0.125f, 4096);
		m_SphereObject = std::make_unique<SDFObject>(0.125f, 4096);

		/*
		m_SDFObject->AddPrimitive(SDFEdit::CreateBox(
			{ 0.0f, -0.25f, 0.0f },
			{ 0.4f, 0.4f, 0.4f }));
		m_SDFObject->AddPrimitive(SDFEdit::CreateOctahedron({ 0.0f, -0.25f, 0.0f }, 0.6f));
		m_SDFObject->AddPrimitive(SDFEdit::CreateSphere({ 0.0f, 0.25f, 0.0f }, 0.4f, SDFOperation::Subtraction));

		// Add a torus on top
		Transform torusTransform(0.0f, 0.25f, 0.0f);
		torusTransform.SetPitch(XMConvertToRadians(90.0f));
		m_SDFObject->AddPrimitive(SDFEdit::CreateTorus(torusTransform, 0.25f, 0.05f, SDFOperation::SmoothUnion, 0.3f));
		*/

		
		{
			// Create torus object edit list
			m_TorusEditList.AddEdit(SDFEdit::CreateTorus({}, 0.85f, 0.05f));

			Transform torusTransform;
			torusTransform.SetPitch(XMConvertToRadians(90.0f));

			m_TorusEditList.AddEdit(SDFEdit::CreateTorus(torusTransform, 0.85f, 0.05f, SDFOperation::SmoothUnion, 0.1f));

			torusTransform.SetPitch(XMConvertToRadians(0.0f));
			torusTransform.SetRoll(XMConvertToRadians(90.0f));

			m_TorusEditList.AddEdit(SDFEdit::CreateTorus(torusTransform, 0.85f, 0.05f, SDFOperation::SmoothUnion, 0.1f));

			m_TorusEditList.AddEdit(SDFEdit::CreateOctahedron({}, 0.7f));
			
			m_SDFFactory->BakeSDFSynchronous(m_TorusObject.get(), m_TorusEditList);
		}
		{
			// Create sphere object by adding and then subtracting a bunch of spheres
			constexpr UINT addSpheres = 16;
			constexpr int subtractSpheres = 48;

			for (UINT i = 0; i < addSpheres; i++)
			{
				const float radius = Random::Float(0.3f, 0.5f);
				m_SphereEditList.AddEdit(SDFEdit::CreateSphere(
					{
							Random::Float(-0.9f + radius, 0.9f - radius),
							Random::Float(-0.9f + radius, 0.9f - radius),
							Random::Float(-0.9f + radius, 0.9f - radius)
					}, 
					radius, i > 0 ? SDFOperation::SmoothUnion : SDFOperation::Union, 0.05f));
			}

			for (UINT i = 0; i < subtractSpheres; i++)
			{
				const float radius = Random::Float(0.1f, 0.3f);
				m_SphereEditList.AddEdit(SDFEdit::CreateSphere(
					{
						Random::Float(-0.99f + radius, 0.99f - radius),
						Random::Float(-0.99f + radius, 0.99f - radius),
						Random::Float(-0.99f + radius, 0.99f - radius)
					},
					radius, SDFOperation::SmoothSubtraction, 0.2f));
			}

			// Bake the primitives into the SDF object
			m_SDFFactory->BakeSDFSynchronous(m_SphereObject.get(), m_SphereEditList);
		}
	}

	{
		// Construct scene geometry
		m_SceneGeometry.push_back({ L"Torus" });
		m_SceneGeometry.push_back({ L"Spheres" });

		auto& torusGeometry = m_SceneGeometry.at(0);
		auto& spheresGeometry = m_SceneGeometry.at(1);

		torusGeometry.GeometryInstances.push_back({
			*m_TorusObject.get(),
			D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE,
			m_TorusObject->GetBrickPoolSRV()
			});
		spheresGeometry.GeometryInstances.push_back({
			*m_SphereObject.get(),
			D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE,
			m_SphereObject->GetBrickPoolSRV()
		});
	}

	{
		// Set up acceleration structure
		constexpr D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		m_AccelerationStructure = std::make_unique<RaytracingAccelerationStructureManager>(s_InstanceCount);

		for (const auto& geometry : m_SceneGeometry)
		{
			m_AccelerationStructure->AddBottomLevelAS(buildFlags, geometry, false, false);
		}

		// Create instances of BLAS

		const std::wstring torusGeometry = L"Torus";
		const std::wstring spheresGeometry = L"Spheres";
		for (UINT z = 0; z < s_InstanceGridDims; z++)
		{
			for (UINT y = 0; y < s_InstanceGridDims; y++)
			{
				for (UINT x = 0; x < s_InstanceGridDims; x++)
				{
					const UINT index = (z * s_InstanceGridDims + y) * s_InstanceGridDims + x;
					//const auto& geometryName = x + y - z % 2 ? torusGeometry : spheresGeometry;
					const auto& geometryName = torusGeometry;

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

					m_AccelerationStructure->AddBottomLevelASInstance(geometryName, rotation * translation, 1);
				}
			}
		}

		// Init top level AS
		m_AccelerationStructure->InitializeTopLevelAS(buildFlags, false, false, L"Top Level Acceleration Structure");
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


	// ImGui

	ImGui::Begin("Scene");
	{

		ImGui::Text("Scene Info"); 
		ImGui::Separator();

		ImGui::Text("Instance Count: %d", s_InstanceCount);
		ImGui::Separator();

		DebugInfo("Torus Object", m_TorusObject.get());
		DebugInfo("Sphere Object", m_SphereObject.get());

		ImGui::Text("Scene Controls");
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
	m_AccelerationStructure->Build();
}


void Scene::DebugInfo(const char* name, const SDFObject* object) const
{
	ImGui::Text(name);

	ImGui::Text("Brick count: %d", object->GetAABBCount());

	const float aabbCull = 100.0f * (1.0f - static_cast<float>(object->GetAABBCount()) / static_cast<float>(object->GetBrickBufferCapacity()));
	ImGui::Text("Brick Cull: %.1f", aabbCull);

	const auto brickPoolSize = object->GetBrickPoolDimensions();
	ImGui::Text("Brick Pool Size: %d, %d, %d", brickPoolSize.x, brickPoolSize.y, brickPoolSize.z);

	const float poolUsage = 100.0f * (static_cast<float>(object->GetAABBCount()) / static_cast<float>(object->GetBrickPoolCapacity()));
	ImGui::Text("Brick Pool Usage: %.1f", poolUsage);

	ImGui::Separator();
}

