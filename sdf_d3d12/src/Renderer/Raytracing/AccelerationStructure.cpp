#include "pch.h"
#include "AccelerationStructure.h"

#include "SDF/SDFObject.h"


void AccelerationStructure::AllocateResource()
{
	LOG_TRACE("Acceleration Structure: Allocating resource ({} Bytes)", m_PrebuildInfo.ResultDataMaxSizeInBytes);

	constexpr D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	m_AccelerationStructure.Allocate(
		g_D3DGraphicsContext->GetDevice(),
		m_PrebuildInfo.ResultDataMaxSizeInBytes,
		initialResourceState,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		m_Name.c_str()
	);
}

void AccelerationStructure::AllocateScratchResource()
{
	LOG_TRACE("Acceleration Structure: Allocating scratch ({} Bytes)", m_PrebuildInfo.ScratchDataSizeInBytes);

	m_ScratchResource.Allocate(
		g_D3DGraphicsContext->GetDevice(),
		m_PrebuildInfo.ScratchDataSizeInBytes,
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		L"Acceleration Structure Scratch Resource"
	);
}


// BOTTOM LEVEL ACCELERATION STRUCTURE


void BottomLevelAccelerationStructure::Initialize(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags, const BottomLevelAccelerationStructureGeometry& geometry, bool allowUpdate, bool updateOnBuild)
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
	AllocateScratchResource();

	m_IsDirty = true;
	m_IsBuilt = false;
}

// The caller must add a UAV barrier before using the resource.
void BottomLevelAccelerationStructure::Build()
{
	ASSERT(m_PrebuildInfo.ScratchDataSizeInBytes <= m_ScratchResource.GetResource()->GetDesc().Width, "Scratch buffer is too small.");

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
			bottomLevelBuildDesc.SourceAccelerationStructureData = m_PreviousAccelerationStructure ? 
				m_PreviousAccelerationStructure->GetGPUVirtualAddress() :
				m_AccelerationStructure.GetAddress();
		}
		bottomLevelInputs.NumDescs = static_cast<UINT>(m_CachedGeometryDescs[m_CurrentID].size());
		bottomLevelInputs.pGeometryDescs = m_CachedGeometryDescs[m_CurrentID].data();

		bottomLevelBuildDesc.ScratchAccelerationStructureData = m_ScratchResource.GetAddress();
		bottomLevelBuildDesc.DestAccelerationStructureData = m_AccelerationStructure.GetAddress();
	}

	g_D3DGraphicsContext->GetDXRCommandList()->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);

	// Schedule previous resources to be released next frame
	if (m_PreviousAccelerationStructure)
	{
		g_D3DGraphicsContext->DeferRelease(m_PreviousAccelerationStructure);
		m_PreviousAccelerationStructure = nullptr;
	}

	m_IsDirty = false;
	m_IsBuilt = true;
}

void BottomLevelAccelerationStructure::UpdateGeometry(const BottomLevelAccelerationStructureGeometry& geometry)
{
	if (!m_AllowUpdate)
	{
		LOG_WARN("Cannot update geometry for BLAS that does not allow update.");
		return;
	}

	LOG_TRACE("BLAS: Performing geometry update.");

	// Rebuild geometry descriptions
	m_GeometryDescs.clear();
	BuildGeometryDescs(geometry);

	// Prebuild info will need recomputed
	const auto prevPrebuildInfo = m_PrebuildInfo;
	ComputePrebuildInfo();

	// Check if the required acceleration structure size has increased
	if (m_PrebuildInfo.ResultDataMaxSizeInBytes > prevPrebuildInfo.ResultDataMaxSizeInBytes)
	{
		LOG_INFO("BLAS: Buffer resize required.");

		// Keep hold of the previous acceleration structure
		m_PreviousAccelerationStructure.Attach(m_AccelerationStructure.TransferResource());
		// Allocate a new acceleration structure
		AllocateResource();
	}

	if (m_PrebuildInfo.ScratchDataSizeInBytes > prevPrebuildInfo.ScratchDataSizeInBytes)
	{
		LOG_INFO("BLAS: Scratch resize required.");

		// Schedule current scratch resource for release
		ComPtr<IUnknown> scratch;
		scratch.Attach(m_ScratchResource.TransferResource());
		g_D3DGraphicsContext->DeferRelease(scratch);

		// Allocate a new scratch resource
		AllocateScratchResource();
	}

	// Acceleration structure will require a rebuild
	m_IsDirty = true;
}

void BottomLevelAccelerationStructure::BuildGeometryDescs(const BottomLevelAccelerationStructureGeometry& geometry)
{
	D3D12_RAYTRACING_GEOMETRY_DESC geometryDescTemplate = {};
	geometryDescTemplate.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS;

	m_GeometryDescs.reserve(geometry.GeometryInstances.size());

	for (const auto& geometryInstance : geometry.GeometryInstances)
	{
		m_GeometryDescs.push_back(geometryDescTemplate);
		auto& desc = m_GeometryDescs.back();

		desc.Flags = geometryInstance->GetGeometryFlags();
		desc.AABBs.AABBCount = geometryInstance->GetBrickCount();
		desc.AABBs.AABBs.StartAddress = geometryInstance->GetAABBBufferAddress();
		desc.AABBs.AABBs.StrideInBytes = geometryInstance->GetAABBBufferStride();
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

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo;
	g_D3DGraphicsContext->GetDXRDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &prebuildInfo);
	ASSERT(prebuildInfo.ResultDataMaxSizeInBytes > 0, "Failed to get acceleration structure prebuild info.");

	if (m_PrebuildInfo.ResultDataMaxSizeInBytes > 0)
	{
		m_PrebuildInfo.ResultDataMaxSizeInBytes = max(m_PrebuildInfo.ResultDataMaxSizeInBytes, prebuildInfo.ResultDataMaxSizeInBytes);
		m_PrebuildInfo.ScratchDataSizeInBytes = max(m_PrebuildInfo.ScratchDataSizeInBytes, prebuildInfo.ScratchDataSizeInBytes);
		m_PrebuildInfo.UpdateScratchDataSizeInBytes = max(m_PrebuildInfo.UpdateScratchDataSizeInBytes, prebuildInfo.UpdateScratchDataSizeInBytes);
	}
	else
	{
		m_PrebuildInfo = prebuildInfo;
	}
}


// TOP LEVEL ACCELERATION STRUCTURE


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
	AllocateScratchResource();

	m_IsDirty = true;
	m_IsBuilt = false;
}

void TopLevelAccelerationStructure::Build(UINT numInstanceDescs, D3D12_GPU_VIRTUAL_ADDRESS instanceDescs)
{
	ASSERT(m_PrebuildInfo.ScratchDataSizeInBytes <= m_ScratchResource.GetResource()->GetDesc().Width, "Scratch buffer is too small.");

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

		topLevelBuildDesc.ScratchAccelerationStructureData = m_ScratchResource.GetAddress();
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


//// RAYTRACING ACCELERATION STRUCTURE MANAGER ////


RaytracingAccelerationStructureManager::RaytracingAccelerationStructureManager(UINT numBottomLevelInstances)
	: m_BottomLevelInstanceDescsStaging(numBottomLevelInstances, D3D12_RAYTRACING_INSTANCE_DESC{})
{
	for (auto& instanceDescs : m_BottomLevelInstanceDescs)
	{
		instanceDescs.Allocate(
			g_D3DGraphicsContext->GetDevice(),
			numBottomLevelInstances,
			16,
			L"Bottom Level Acceleration Structure Instance Descs");
	}
}

void RaytracingAccelerationStructureManager::AddBottomLevelAS(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags, const BottomLevelAccelerationStructureGeometry& geometry, bool allowUpdate, bool updateOnBuild)
{
	ASSERT(m_BottomLevelAS.find(geometry.Name) == m_BottomLevelAS.end(), "A bottom level acceleration structure with that name already exists");

	auto& bottomLevelAS = m_BottomLevelAS[geometry.Name];
	bottomLevelAS.Initialize(buildFlags, geometry, allowUpdate, updateOnBuild);
}

void RaytracingAccelerationStructureManager::UpdateBottomLevelASGeometry(const BottomLevelAccelerationStructureGeometry& geometry)
{
	ASSERT(m_BottomLevelAS.find(geometry.Name) != m_BottomLevelAS.end(), "BLAS does not exist.");

	auto& bottomLevelAS = m_BottomLevelAS.at(geometry.Name);
	bottomLevelAS.UpdateGeometry(geometry);
}


UINT RaytracingAccelerationStructureManager::AddBottomLevelASInstance(const std::wstring& bottomLevelASName, const XMMATRIX& transform, BYTE instanceMask)
{
	ASSERT(m_NumBottomLevelInstances < m_BottomLevelInstanceDescsStaging.size(), "Not enough instance desc buffer size.");
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
}

void RaytracingAccelerationStructureManager::Build(bool forceBuild)
{
	const auto commandList = g_D3DGraphicsContext->GetCommandList();
	const auto frame = g_D3DGraphicsContext->GetCurrentBackBuffer();

	// Copy staging buffer to GPU
	m_BottomLevelInstanceDescs.at(frame).CopyElements(0, m_NumBottomLevelInstances, m_BottomLevelInstanceDescsStaging.data());

	// Build all bottom level AS
	{
		// Group all resource barriers together since each BLAS has its own scratch resource
		std::vector<CD3DX12_RESOURCE_BARRIER> barriers;
		barriers.reserve(m_BottomLevelAS.size());

		for (auto&[name, bottomLevelAS] : m_BottomLevelAS)
		{
			if (forceBuild || bottomLevelAS.IsDirty())
			{
				bottomLevelAS.Build();
				barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(bottomLevelAS.GetResource()));
			}
		}
		// Submit barriers
		if (!barriers.empty())
		{
			commandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
		}
	}

	// Build the top level AS
	{
		m_TopLevelAS.Build(m_NumBottomLevelInstances,
			m_BottomLevelInstanceDescs.at(frame).GetAddressOfElement(0));

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
	const D3D12_GPU_VIRTUAL_ADDRESS blasAddress = m_BottomLevelAS.at(blas).GetResource()->GetGPUVirtualAddress();

	for (auto& instanceDesc : m_BottomLevelInstanceDescsStaging)
	{
		// find all instances that point to the specified blas
		if (instanceDesc.AccelerationStructure == blasAddress)
		{
			instanceDesc.InstanceContributionToHitGroupIndex = instanceContributionToHitGroupIndex;
		}
	}
}

