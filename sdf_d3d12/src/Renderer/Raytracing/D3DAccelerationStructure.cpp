#include "pch.h"
#include "D3DAccelerationStructure.h"

#include "../D3DGraphicsContext.h"


void AccelerationStructure::AllocateResource()
{
	const D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	m_AccelerationStructure.Allocate(g_D3DGraphicsContext->GetDevice(), m_PrebuildInfo.ResultDataMaxSizeInBytes, initialResourceState, m_Name.c_str());
}


void BottomLevelAccelerationStructure::Initialize(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags, BottomLevelAccelerationStructureGeometry& geometry, bool allowUpdate, bool updateOnBuild)
{
	m_AllowUpdate = allowUpdate;
	m_UpdateOnBuild = updateOnBuild;

	m_BuildFlags = buildFlags;
	m_Name = geometry.GetName();

	if (m_AllowUpdate)
	{
		m_BuildFlags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
	}

	m_CachedGeometryDescs.resize(D3DGraphicsContext::GetBackBufferCount());

	BuildGeometryDescs(geometry);
	ComputePrebuildInfo();
	AllocateResource();

	m_IsDirty = true;
	m_IsBuilt = false;
}

// The caller must add a UAV barrier before using the resource.
void BottomLevelAccelerationStructure::Build(ID3D12Resource* scratch)
{
	ASSERT(m_PrebuildInfo.ScratchDataSizeInBytes <= scratch->GetDesc().Width, "Scratch buffer is too small.");

	m_CurrentID = g_D3DGraphicsContext->GetCurrentBackBuffer();
	m_CachedGeometryDescs[m_CurrentID].clear();
	m_CachedGeometryDescs[m_CurrentID].resize(m_GeometryDescs.size());
	std::copy(m_CachedGeometryDescs[m_CurrentID].begin(), m_CachedGeometryDescs[m_CurrentID].end(), m_GeometryDescs.begin());

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& bottomLevelInputs = bottomLevelBuildDesc.Inputs;
	{
		bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		bottomLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		bottomLevelInputs.Flags = m_BuildFlags;
		if (m_IsBuilt && m_AllowUpdate && m_UpdateOnBuild)
		{
			bottomLevelInputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
			bottomLevelBuildDesc.SourceAccelerationStructureData = m_AccelerationStructure.GetAddress();
		}
		bottomLevelInputs.NumDescs = static_cast<UINT>(m_CachedGeometryDescs[m_CurrentID].size());
		bottomLevelInputs.pGeometryDescs = m_CachedGeometryDescs[m_CurrentID].data();

		bottomLevelBuildDesc.ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress();
		bottomLevelBuildDesc.DestAccelerationStructureData = m_AccelerationStructure.GetAddress();
	}

	g_D3DGraphicsContext->GetDXRCommandList()->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);

	m_IsDirty = false;
	m_IsBuilt = true;
}

void BottomLevelAccelerationStructure::BuildGeometryDescs(BottomLevelAccelerationStructureGeometry& geometry)
{
	NOT_IMPLEMENTED;

	D3D12_RAYTRACING_GEOMETRY_DESC geometryDescTemplate;
	geometryDescTemplate.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS;
	geometryDescTemplate.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
}

void BottomLevelAccelerationStructure::ComputePrebuildInfo()
{
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS bottomLevelInputs = {};
	bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	bottomLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	bottomLevelInputs.Flags = m_BuildFlags;
	bottomLevelInputs.NumDescs = static_cast<UINT>(m_GeometryDescs.size());
	bottomLevelInputs.pGeometryDescs = m_GeometryDescs.data();

	g_D3DGraphicsContext->GetDXRDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &m_PrebuildInfo);
	ASSERT(m_PrebuildInfo.ResultDataMaxSizeInBytes > 0, "Failed to get acceleration structure prebuild info.");
}



void TopLevelAccelerationStructure::Initialize(UINT numBottomLevelASInstanceDescs, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags, bool allowUpdate, bool updateOnBuild, const wchar_t* resourceName)
{
	m_AllowUpdate = allowUpdate;
	m_UpdateOnBuild = updateOnBuild;
	m_BuildFlags = buildFlags;
	m_Name = resourceName;

	if (m_AllowUpdate)
	{
		m_BuildFlags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
	}

	ComputePrebuildInfo(numBottomLevelASInstanceDescs);
	AllocateResource();

	m_IsDirty = true;
	m_IsBuilt = false;
}

void TopLevelAccelerationStructure::Build(UINT numInstanceDescs, D3D12_GPU_VIRTUAL_ADDRESS instanceDescs, ID3D12Resource* scratch)
{
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc;
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& topLevelInputs = topLevelBuildDesc.Inputs;
	{
		topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
		topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		topLevelInputs.Flags = m_BuildFlags;
		if (m_IsBuilt && m_AllowUpdate && m_UpdateOnBuild)
		{
			topLevelInputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
		}
		topLevelInputs.NumDescs = numInstanceDescs;
		topLevelInputs.InstanceDescs = instanceDescs;

		topLevelBuildDesc.ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress();
		topLevelBuildDesc.DestAccelerationStructureData = m_AccelerationStructure.GetAddress();
	}

	g_D3DGraphicsContext->GetDXRCommandList()->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);

	m_IsDirty = false;
	m_IsBuilt = true;
}

void TopLevelAccelerationStructure::ComputePrebuildInfo(UINT numBottomLevelASInstanceDescs)
{
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS topLevelInputs;
	topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	topLevelInputs.Flags = m_BuildFlags;
	topLevelInputs.NumDescs = numBottomLevelASInstanceDescs;

	g_D3DGraphicsContext->GetDXRDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &m_PrebuildInfo);
	ASSERT(m_PrebuildInfo.ResultDataMaxSizeInBytes > 0, "Failed to get acceleration structure prebuild info.");
}


RaytracingAccelerationStructureManager::RaytracingAccelerationStructureManager(UINT numBottomLevelInstances, UINT frameCount)
	: m_BottomLevelInstanceDescs(
		g_D3DGraphicsContext->GetDevice(),
		numBottomLevelInstances,
		frameCount, 
		16, 
		L"Bottom Level Acceleration Structure Instance Descs")
{
}

void RaytracingAccelerationStructureManager::AddBottomLevelAS()
{
	
}

void RaytracingAccelerationStructureManager::AddBottomLevelASInstance()
{
	
}

void RaytracingAccelerationStructureManager::InitializeTopLevelAS()
{
	
}

void RaytracingAccelerationStructureManager::Build()
{
	
}
