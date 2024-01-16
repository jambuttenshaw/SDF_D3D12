#include "pch.h"
#include "Scene.h"

#include "Renderer/D3DGraphicsContext.h"


Scene::Scene()
{
	// Build SDF Object
	{
		// Create SDF factory
		m_SDFFactory = std::make_unique<SDFFactory>();

		// Create an SDF object
		m_SDFObject = std::make_unique<SDFObject>(128);
		//m_SDFObject = std::make_unique<SDFObject>(256, 256, 256);

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

		m_SDFObject->AddPrimitive(SDFPrimitive::CreateSphere({ 0.0f, 0.0f, 0.0f }, 0.5f));
		m_SDFObject->AddPrimitive(SDFPrimitive::CreateSphere({ 0.3f, -0.4f, 0.1f }, 0.3f, SDFOperation::SmoothUnion, 0.2f));
		m_SDFObject->AddPrimitive(SDFPrimitive::CreateSphere({ -0.2f, 0.3f, -0.4f }, 0.3f, SDFOperation::SmoothUnion, 0.2f));
		m_SDFObject->AddPrimitive(SDFPrimitive::CreateSphere({ 0.1f, 0.0f, 0.2f }, 0.25f, SDFOperation::SmoothSubtraction, 0.3f));

		// Bake the primitives into the SDF object
		m_SDFFactory->BakeSDFSynchronous(m_SDFObject.get());
	}

	{
		// Construct scene geometry
		m_SceneGeometry.push_back({ L"AABB Geometry" });
		auto& aabbGeometry = m_SceneGeometry.at(0);

		// Construct geometry
		constexpr UINT dims = 8;
		constexpr UINT aabbCount = dims * dims * dims;
		constexpr float halfSize = 1.5f;
		AABBGeometry geometry{ aabbCount };
		for (float z = 0; z < dims; z++)
		for (float y = 0; y < dims; y++)
		for (float x = 0; x < dims; x++)
		{
			float d = 1.0f / dims;
			XMFLOAT3 uvwMin { d * x, d * y, d * z };
			XMFLOAT3 uvwMax { uvwMin.x + d, uvwMin.y + d, uvwMin.z + d };

			float s = halfSize / dims;
			geometry.AddAABB(
				{
					2 * (x - 0.5f * dims + 0.5f) * s,
					2 * (y - 0.5f * dims + 0.5f) * s,
					2 * (z - 0.5f * dims + 0.5f) * s
				},
				{ s, s, s }, 
				uvwMin, 
				uvwMax);
		}

		// Use move semantics to place geometry into the vector
		aabbGeometry.Geometry.push_back(std::move(geometry));

		// Construct a geometry instance
		// Note: 'geometry' is no longer valid here as it has been moved from
		aabbGeometry.GeometryInstances.push_back({ aabbGeometry.Geometry.back(), D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE, m_SDFObject->GetSRV(), m_SDFObject->GetResolution() });
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

		srand(0);
		std::wstring geometryName = L"AABB Geometry";
		for (UINT z = 0; z < s_InstanceGridDims; z++)
		{
			for (UINT y = 0; y < s_InstanceGridDims; y++)
			{
				for (UINT x = 0; x < s_InstanceGridDims; x++)
				{
					const UINT index = (z * s_InstanceGridDims + y) * s_InstanceGridDims + x;

					auto m = XMMatrixRotationRollPitchYaw(
						XMConvertToRadians(rand() % 360),
						XMConvertToRadians(rand() % 360),
						XMConvertToRadians(rand() % 360)
					);
					m_InstanceRotations[index] = m;
					m_InstanceRotationDeltas[index] = {
						static_cast<float>(rand() % 4 - 2),
						static_cast<float>(rand() % 4 - 2),
						static_cast<float>(rand() % 4 - 2)
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

	for (UINT z = 0; z < s_InstanceGridDims; z++)
	{
		for (UINT y = 0; y < s_InstanceGridDims; y++)
		{
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
	}
}

void Scene::OnRender()
{
	UpdateAccelerationStructure();
}


void Scene::UpdateAccelerationStructure()
{
	m_AccelerationStructure->Build();
}

