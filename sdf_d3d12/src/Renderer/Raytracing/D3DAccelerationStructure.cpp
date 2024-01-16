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
	m_Name = geometry.Name;

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
	m_CachedGeometryDescs[m_CurrentID] = m_GeometryDescs;

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
	D3D12_RAYTRACING_GEOMETRY_DESC geometryDescTemplate = {};
	geometryDescTemplate.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS;

	m_GeometryDescs.reserve(geometry.GeometryInstances.size());

	for (const auto& geometryInstance : geometry.GeometryInstances)
	{
		m_GeometryDescs.push_back(geometryDescTemplate);
		auto& desc = m_GeometryDescs.back();

		desc.Flags = geometryInstance.GetFlags();
		desc.AABBs.AABBCount = geometryInstance.GetAABBCount();
		desc.AABBs.AABBs = geometryInstance.GetAABBBuffer();
	}
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
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& topLevelInputs = topLevelBuildDesc.Inputs;
	{
		topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
		topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		topLevelInputs.Flags = m_BuildFlags;
		if (m_IsBuilt && m_AllowUpdate && m_UpdateOnBuild)
		{
			topLevelInputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
			topLevelBuildDesc.SourceAccelerationStructureData = m_AccelerationStructure.GetAddress();
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
	, m_BottomLevelInstanceDescsStaging(numBottomLevelInstances, D3D12_RAYTRACING_INSTANCE_DESC{})
{
}

void RaytracingAccelerationStructureManager::AddBottomLevelAS(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags, BottomLevelAccelerationStructureGeometry& geometry, bool allowUpdate, bool updateOnBuild)
{
	ASSERT(m_BottomLevelAS.find(geometry.Name) == m_BottomLevelAS.end(), "A bottom level acceleration structure with that name already exists");

	auto& bottomLevelAS = m_BottomLevelAS[geometry.Name];
	bottomLevelAS.Initialize(buildFlags, geometry, allowUpdate, updateOnBuild);

	m_ScratchResourceSize = max(bottomLevelAS.GetRequiredScratchSize(), m_ScratchResourceSize);
}

UINT RaytracingAccelerationStructureManager::AddBottomLevelASInstance(const std::wstring& bottomLevelASName, const XMMATRIX& transform, BYTE instanceMask)
{
	ASSERT(m_NumBottomLevelInstances < m_BottomLevelInstanceDescs.GetElementCount(), "Not enough instance desc buffer size.");
	ASSERT(m_BottomLevelAS.find(bottomLevelASName) != m_BottomLevelAS.end(), "Bottom level AS does not exist.");

	const UINT instanceIndex = m_NumBottomLevelInstances++;
	const auto& bottomLevelAS = m_BottomLevelAS.at(bottomLevelASName);

	// Populate the next element in the staging buffer
	D3D12_RAYTRACING_INSTANCE_DESC& instanceDesc = m_BottomLevelInstanceDescsStaging.at(instanceIndex);
	instanceDesc.InstanceMask = instanceMask;
	instanceDesc.InstanceContributionToHitGroupIndex = bottomLevelAS.GetInstanceContributionToHitGroupIndex();
	instanceDesc.AccelerationStructure = bottomLevelAS.GetResource()->GetGPUVirtualAddress();
	XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(instanceDesc.Transform), transform);

	return instanceIndex;
}

void RaytracingAccelerationStructureManager::InitializeTopLevelAS(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags, bool allowUpdate, bool updateOnBuild, const wchar_t* resourceName)
{
	m_TopLevelAS.Initialize(m_NumBottomLevelInstances, buildFlags, allowUpdate, updateOnBuild, resourceName);

	m_ScratchResourceSize = max(m_TopLevelAS.GetRequiredScratchSize(), m_ScratchResourceSize);

	// allocate buffer for scratch resource
	m_ScratchResource.Allocate(g_D3DGraphicsContext->GetDevice(), m_ScratchResourceSize, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"Acceleration Structure Scratch Resource");
}

void RaytracingAccelerationStructureManager::Build(bool forceBuild)
{
	const auto commandList = g_D3DGraphicsContext->GetCommandList();

	// Copy staging buffer to GPU
	m_BottomLevelInstanceDescs.CopyElements(0, m_NumBottomLevelInstances, g_D3DGraphicsContext->GetCurrentBackBuffer(), m_BottomLevelInstanceDescsStaging.data());

	// Build all bottom level AS
	{
		for (auto&[name, bottomLevelAS] : m_BottomLevelAS)
		{
			if (forceBuild || bottomLevelAS.IsDirty())
			{
				bottomLevelAS.Build(m_ScratchResource.GetResource());

				// Since a single scratch resource is reused, put a barrier in-between each call.
				// TODO: PERFORMANCE tip: use separate scratch memory per BLAS build to allow a GPU driver to overlap build calls.
				const auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(bottomLevelAS.GetResource());
				commandList->ResourceBarrier(1, &barrier);
			}
		}
	}

	// Build the top level AS
	{
		m_TopLevelAS.Build(m_NumBottomLevelInstances,
			m_BottomLevelInstanceDescs.GetAddressOfElement(0, g_D3DGraphicsContext->GetCurrentBackBuffer()),
			m_ScratchResource.GetResource());

		const auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(m_TopLevelAS.GetResource());
		commandList->ResourceBarrier(1, &barrier);
	}
}

void RaytracingAccelerationStructureManager::SetInstanceTransform(UINT instanceIndex, const XMMATRIX& transform)
{
	ASSERT(instanceIndex < m_NumBottomLevelInstances, "Instance index out of bounds.");

	auto& instanceDesc = m_BottomLevelInstanceDescsStaging.at(instanceIndex);
	XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(instanceDesc.Transform), transform);
}


void RaytracingAccelerationStructureManager::SetBLASInstanceContributionToHitGroup(const std::wstring& blas, UINT instanceContributionToHitGroupIndex)
{
	// This must be done through the AS manager as the instance buffer requires updating

	// Make sure BLAS exists
	ASSERT(m_BottomLevelAS.find(blas) != m_BottomLevelAS.end(), "BLAS doesn't exist!");

	m_BottomLevelAS.at(blas).SetInstanceContributionToHitGroupIndex(instanceContributionToHitGroupIndex);
	D3D12_GPU_VIRTUAL_ADDRESS blasAddress = m_BottomLevelAS.at(blas).GetResource()->GetGPUVirtualAddress();

	for (auto& instanceDesc : m_BottomLevelInstanceDescsStaging)
	{
		// find all instances that point to the specified blas
		if (instanceDesc.AccelerationStructure == blasAddress)
		{
			instanceDesc.InstanceContributionToHitGroupIndex = instanceContributionToHitGroupIndex;
		}
	}
}

