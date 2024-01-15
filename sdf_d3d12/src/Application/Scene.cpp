#include "pch.h"
#include "Scene.h"

#include "Renderer/D3DGraphicsContext.h"


Scene::Scene()
{
	// Construct scene geometry
	m_SceneGeometry.push_back({ L"AABB Geometry" });

	// Construct a geometry instance
	AABBGeometryInstance geometryInstance{ 1, D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE };
	geometryInstance.AddAABB({ 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f });
	m_SceneGeometry.at(0).GeometryInstances.push_back(geometryInstance);

	// Set up acceleration structure

	constexpr D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	m_AccelerationStructure = std::make_unique<RaytracingAccelerationStructureManager>(1, D3DGraphicsContext::GetBackBufferCount());

	for (auto& geometry : m_SceneGeometry)
	{
		m_AccelerationStructure->AddBottomLevelAS(buildFlags, geometry, false, false);
	}

	// Create one instance of each bottom level AS
	for (const auto& geometry : m_SceneGeometry)
	{
		m_AccelerationStructure->AddBottomLevelASInstance(geometry.Name, UINT_MAX, XMMatrixIdentity(), 1);
	}
	// Init top level AS
	m_AccelerationStructure->InitializeTopLevelAS(buildFlags, false, false, L"Top Level Acceleration Structure");
}


void Scene::OnUpdate(float deltaTime)
{
	// Manipulate objects in the scene
}

void Scene::OnRender()
{
	UpdateAccelerationStructure();
}


void Scene::UpdateAccelerationStructure()
{
	m_AccelerationStructure->Build();
}

