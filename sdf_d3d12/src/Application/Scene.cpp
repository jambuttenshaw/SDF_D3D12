#include "pch.h"
#include "Scene.h"

#include "imgui.h"
#include "Framework/Math.h"
#include "Renderer/D3DGraphicsContext.h"


Scene::Scene()
{
	// Build SDF Object
	{
		// Create SDF factory 
		m_SDFFactory = std::make_unique<SDFFactory>();

		// Create an SDF object
		m_SDFObject = std::make_unique<SDFObject>(128, 8, 8);

		/*
		m_SDFObject->AddPrimitive(SDFPrimitive::CreateBox(
			{ 0.0f, -0.25f, 0.0f },
			{ 0.4f, 0.4f, 0.4f }));
		m_SDFObject->AddPrimitive(SDFPrimitive::CreateOctahedron({ 0.0f, -0.25f, 0.0f }, 0.6f));
		m_SDFObject->AddPrimitive(SDFPrimitive::CreateSphere({ 0.0f, 0.25f, 0.0f }, 0.4f, SDFOperation::Subtraction));

		// Add a torus on top
		Transform torusTransform(0.0f, 0.25f, 0.0f);
		torusTransform.SetPitch(XMConvertToRadians(90.0f));
		m_SDFObject->AddPrimitive(SDFPrimitive::CreateTorus(torusTransform, 0.25f, 0.05f, SDFOperation::SmoothUnion, 0.3f));
		*/
		/*
		m_SDFObject->AddPrimitive(SDFPrimitive::CreateSphere({ 0.0f, 0.0f, 0.0f }, 0.5f));
		m_SDFObject->AddPrimitive(SDFPrimitive::CreateSphere({ 0.3f, -0.4f, 0.1f }, 0.3f, SDFOperation::SmoothUnion, 0.2f));
		m_SDFObject->AddPrimitive(SDFPrimitive::CreateSphere({ -0.2f, 0.3f, -0.4f }, 0.3f, SDFOperation::SmoothUnion, 0.2f));
		m_SDFObject->AddPrimitive(SDFPrimitive::CreateSphere({ 0.1f, 0.0f, 0.2f }, 0.25f, SDFOperation::SmoothSubtraction, 0.3f));
		*/
		
		m_SDFObject->AddPrimitive(SDFPrimitive::CreateOctahedron({}, 0.75f));
		m_SDFObject->AddPrimitive(SDFPrimitive::CreateTorus({}, 0.9f, 0.05f));

		Transform torusTransform;
		torusTransform.SetPitch(XMConvertToRadians(90.0f));

		m_SDFObject->AddPrimitive(SDFPrimitive::CreateTorus(torusTransform, 0.9f, 0.05f, SDFOperation::SmoothUnion, 0.1f));

		torusTransform.SetPitch(XMConvertToRadians(0.0f));
		torusTransform.SetRoll(XMConvertToRadians(90.0f));

		m_SDFObject->AddPrimitive(SDFPrimitive::CreateTorus(torusTransform, 0.9f, 0.05f, SDFOperation::SmoothUnion, 0.1f));


		// Bake the primitives into the SDF object
		m_SDFFactory->BakeSDFSynchronous(m_SDFObject.get());
	}

	{
		// Construct scene geometry
		m_SceneGeometry.push_back({ L"AABB Geometry" });
		auto& aabbGeometry = m_SceneGeometry.at(0);

		// Construct a geometry instance
		aabbGeometry.GeometryInstances.push_back({ *m_SDFObject.get(), D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE, m_SDFObject->GetVolumeSRV(), m_SDFObject->GetVolumeResolution(), m_SDFObject->GetVolumeStride() });
	}

	{
		// Set up acceleration structure
		constexpr D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		m_AccelerationStructure = std::make_unique<RaytracingAccelerationStructureManager>(s_InstanceCount, D3DGraphicsContext::GetBackBufferCount());

		for (auto& geometry : m_SceneGeometry)
		{
			m_AccelerationStructure->AddBottomLevelAS(buildFlags, geometry, false, false);
		}

		// Create instances of BLAS

		std::wstring geometryName = L"AABB Geometry";
		for (UINT z = 0; z < s_InstanceGridDims; z++)
		{
			for (UINT y = 0; y < s_InstanceGridDims; y++)
			{
				for (UINT x = 0; x < s_InstanceGridDims; x++)
				{
					const UINT index = (z * s_InstanceGridDims + y) * s_InstanceGridDims + x;

					auto m = XMMatrixRotationRollPitchYaw(
						XMConvertToRadians(Random::Float(360.0f)),
						XMConvertToRadians(Random::Float(360.0f)),
						XMConvertToRadians(Random::Float(360.0f))
					);
					m_InstanceRotations[index] = m;
					m_InstanceRotationDeltas[index] = {
						Random::Float(-0.2f, 0.2f),
						Random::Float(-0.2f, 0.2f),
						Random::Float(-0.2f, 0.2f)
					};

					m *= XMMatrixTranslation(x * s_InstanceSpacing, y * s_InstanceSpacing, z * s_InstanceSpacing);
					m_AccelerationStructure->AddBottomLevelASInstance(geometryName, m, 1);
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
			auto m = m_InstanceRotations[index];
			auto d = m_InstanceRotationDeltas[index];
			m = m * XMMatrixRotationRollPitchYaw(d.x * deltaTime, d.y * deltaTime, d.z * deltaTime);
			m_InstanceRotations[index] = m;

			const auto t = XMMatrixTranslation(x * s_InstanceSpacing, y * s_InstanceSpacing, z * s_InstanceSpacing);

			m_AccelerationStructure->SetInstanceTransform(index, m * t);
		}
	}


	// ImGui

	ImGui::Begin("Scene");
	{
		ImGui::Text("Scene Info"); 
		ImGui::Separator();

		ImGui::Text("Instance Count: %d", s_InstanceCount);
		ImGui::Text("AABBs Per Instance: %d", m_SDFObject->GetAABBCount());

		const float aabbCull = 100.0f * (1.0f - static_cast<float>(m_SDFObject->GetAABBCount()) / static_cast<float>(m_SDFObject->GetMaxAABBCount()));
		ImGui::Text("AABB Cull: %.1f", aabbCull);

		ImGui::Separator();

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

