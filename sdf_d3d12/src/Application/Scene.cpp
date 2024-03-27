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


Scene::Scene(D3DApplication* application, UINT maxGeometryInstances)
	: m_Application(application)
	, m_MaxGeometryInstances(maxGeometryInstances)
{
	ASSERT(m_Application, "Invalid app.");

	{
		// Set up acceleration structure
		constexpr D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		m_AccelerationStructure = std::make_unique<RaytracingAccelerationStructureManager>(m_MaxGeometryInstances);

		// Init top level AS
		m_AccelerationStructure->InitializeTopLevelAS(buildFlags, true, true, L"Top Level Acceleration Structure");

		// As there is a fixed max of instances, reserving them in advance ensures the vector will never re-allocate
		m_GeometryInstances.reserve(m_MaxGeometryInstances);
	}
}


void Scene::PreRender()
{
	PIXBeginEvent(PIX_COLOR_INDEX(6), L"Scene Render");

	// Check if the geometry should have its resources flipped
	CheckSDFGeometryUpdates();

	UpdateAccelerationStructure();

	PIXEndEvent();
}


void Scene::AddGeometry(const std::wstring& name, SDFObject* geometry)
{
	constexpr D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	m_AccelerationStructure->AddBottomLevelAS(flags, name, geometry, true, true);

	m_Geometries.push_back(geometry);
}


SDFGeometryInstance* Scene::CreateGeometryInstance(const std::wstring& geometryName)
{
	UINT instanceID = m_AccelerationStructure->AddBottomLevelASInstance(geometryName, XMMatrixIdentity(), 1);
	size_t blasIndex = m_AccelerationStructure->GetBottomLevelAccelerationStructureIndex(geometryName);

	m_GeometryInstances.emplace_back(instanceID, blasIndex);

	return &m_GeometryInstances.back();
}


void Scene::CheckSDFGeometryUpdates() const
{
	for (const auto& object : m_Geometries)
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
			m_AccelerationStructure->GetBottomLevelAccelerationStructure(object).UpdateGeometry();
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
	m_AccelerationStructure->UpdateInstanceDescs(m_GeometryInstances);

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

	for (const auto& blasGeometry : m_Geometries)
	{
		//const auto& blas = m_AccelerationStructure->GetBottomLevelAccelerationStructure(blasGeometry);
		//DisplaySize("Bottom Level AS", blas.GetResourcesSize() / 1024);
	}
}
