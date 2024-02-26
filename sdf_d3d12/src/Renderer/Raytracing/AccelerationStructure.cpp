#include "pch.h"
#include "AccelerationStructure.h"

#include "SDF/SDFObject.h"

#include "pix3.h"
#include "Renderer/Profiling/GPUProfiler.h"


void AccelerationStructure::AllocateResource()
{
	const std::wstring name = m_Name + L"BLAS";

	constexpr D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	m_AccelerationStructure.Allocate(
		g_D3DGraphicsContext->GetDevice(),
		m_PrebuildInfo.ResultDataMaxSizeInBytes,
		initialResourceState,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		name.c_str()
	);
	m_IsBuilt = false;
}

void AccelerationStructure::AllocateScratchResource()
{
	const std::wstring name = m_Name + L"BLAS Scratch";

	m_ScratchResource.Allocate(
		g_D3DGraphicsContext->GetDevice(),
		m_PrebuildInfo.ScratchDataSizeInBytes,
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		name.c_str()
	);
	m_IsBuilt = false;
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

	m_CachedGeometryDesc.resize(D3DGraphicsContext::GetBackBufferCount());

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
	ASSERT(m_PrebuildInfo.ResultDataMaxSizeInBytes <= m_AccelerationStructure.GetResource()->GetDesc().Width, "Acceleration structure buffer is too small.");
	ASSERT(m_PrebuildInfo.ScratchDataSizeInBytes <= m_ScratchResource.GetResource()->GetDesc().Width, "Scratch buffer is too small.");

	const auto commandList = g_D3DGraphicsContext->GetDXRCommandList();
	PIXBeginEvent(commandList, PIX_COLOR_INDEX(64), "Build BLAS");

	const auto current = g_D3DGraphicsContext->GetCurrentBackBuffer();
	m_CachedGeometryDesc[current] = m_GeometryDesc;

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
		bottomLevelInputs.NumDescs = 1;
		bottomLevelInputs.pGeometryDescs = &m_CachedGeometryDesc[current];

		bottomLevelBuildDesc.ScratchAccelerationStructureData = m_ScratchResource.GetAddress();
		bottomLevelBuildDesc.DestAccelerationStructureData = m_AccelerationStructure.GetAddress();
	}


	if (!m_IsBuilt)
	{
		const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_ScratchResource.GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		commandList->ResourceBarrier(1, &barrier);
	}
	commandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);
	PIXEndEvent(commandList);

	m_IsDirty = false;
	m_IsBuilt = true;
}

void BottomLevelAccelerationStructure::UpdateGeometry(const BottomLevelAccelerationStructureGeometry& geometry)
{
	const UINT64 prevAABBs = m_GeometryDesc.AABBs.AABBCount;

	// Rebuild geometry descriptions
	BuildGeometryDescs(geometry);

	// Prebuild info will need recomputed
	ComputePrebuildInfo();

	// Check if the required acceleration structure size has increased
	const UINT64 accelerationStructureWidth = m_AccelerationStructure.GetResource()->GetDesc().Width;
	if (m_PrebuildInfo.ResultDataMaxSizeInBytes > accelerationStructureWidth)
	{
		LOG_TRACE("BLAS: Acceleration structure resize required. ({} bytes to {} bytes)", accelerationStructureWidth, m_PrebuildInfo.ResultDataMaxSizeInBytes);

		// Schedule current acceleration structure resource for release
		ComPtr<IUnknown> accelerationStructure;
		accelerationStructure.Attach(m_AccelerationStructure.TransferResource());
		g_D3DGraphicsContext->DeferRelease(accelerationStructure);

		// Allocate a new acceleration structure
		AllocateResource();
	}

	const UINT64 scratchWidth = m_ScratchResource.GetResource()->GetDesc().Width;
	if (m_PrebuildInfo.ScratchDataSizeInBytes > scratchWidth)
	{
		LOG_TRACE("BLAS: Scratch resize required. ({} bytes to {} bytes)", scratchWidth, m_PrebuildInfo.ScratchDataSizeInBytes);

		// Schedule current scratch resource for release
		ComPtr<IUnknown> scratch;
		scratch.Attach(m_ScratchResource.TransferResource());
		g_D3DGraphicsContext->DeferRelease(scratch);
		
		// Allocate a new scratch resource
		AllocateScratchResource();
	}

	// Acceleration structure will require at least an update
	m_IsDirty = true;
	{
		// Rebuild if number of AABBs has changed
		m_IsBuilt &= prevAABBs == geometry.Geometry->GetBrickCount(SDFObject::RESOURCES_READ);
	}
}

void BottomLevelAccelerationStructure::BuildGeometryDescs(const BottomLevelAccelerationStructureGeometry& geometry)
{
	m_GeometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS;
	m_GeometryDesc.Flags = geometry.Geometry->GetGeometryFlags();
	m_GeometryDesc.AABBs.AABBCount = geometry.Geometry->GetBrickCount(SDFObject::RESOURCES_READ);
	m_GeometryDesc.AABBs.AABBs.StartAddress = geometry.Geometry->GetAABBBufferAddress(SDFObject::RESOURCES_READ);
	m_GeometryDesc.AABBs.AABBs.StrideInBytes = geometry.Geometry->GetAABBBufferStride(SDFObject::RESOURCES_READ);
}

void BottomLevelAccelerationStructure::ComputePrebuildInfo()
{
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS bottomLevelInputs = {};
	bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	bottomLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	bottomLevelInputs.Flags = m_BuildFlags;
	bottomLevelInputs.NumDescs = 1;
	bottomLevelInputs.pGeometryDescs = &m_GeometryDesc;

	g_D3DGraphicsContext->GetDXRDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &m_PrebuildInfo);
	ASSERT(m_PrebuildInfo.ResultDataMaxSizeInBytes > 0, "Failed to get acceleration structure prebuild info.");
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
	ASSERT(m_PrebuildInfo.ResultDataMaxSizeInBytes <= m_AccelerationStructure.GetResource()->GetDesc().Width, "Scratch buffer is too small.");
	ASSERT(m_PrebuildInfo.ScratchDataSizeInBytes <= m_ScratchResource.GetResource()->GetDesc().Width, "Scratch buffer is too small.");

	const auto commandList = g_D3DGraphicsContext->GetDXRCommandList();
	PIXBeginEvent(commandList, PIX_COLOR_INDEX(63), "Build TLAS");


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

	commandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
	PIXEndEvent(commandList);

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
	, m_InstanceMetadata(numBottomLevelInstances)
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
	ASSERT(m_BottomLevelASNames.find(geometry.Name) == m_BottomLevelASNames.end(), "A bottom level acceleration structure with that name already exists");

	const size_t index = m_BottomLevelAS.size();
	m_BottomLevelASNames[geometry.Name] = index;

	m_BottomLevelAS.emplace_back();
	m_BottomLevelAS.back().Initialize(buildFlags, geometry, allowUpdate, updateOnBuild);
}

void RaytracingAccelerationStructureManager::UpdateBottomLevelASGeometry(const BottomLevelAccelerationStructureGeometry& geometry)
{
	ASSERT(m_BottomLevelASNames.find(geometry.Name) != m_BottomLevelASNames.end(), "BLAS does not exist.");

	auto& bottomLevelAS = m_BottomLevelAS.at(m_BottomLevelASNames.at(geometry.Name));
	bottomLevelAS.UpdateGeometry(geometry);

	if (!bottomLevelAS.IsBuilt())
	{
		// If the acceleration structure is not built; then a re-allocation likely has occurred
		// All instances should have their acceleration structure pointer amended
		const size_t blasIndex = m_BottomLevelASNames.at(bottomLevelAS.GetName());
		for (size_t i = 0; i < m_NumBottomLevelInstances; i++)
		{
			const auto& metadata = m_InstanceMetadata.at(i);
			if (metadata.BLASIndex == blasIndex)
			{
				auto& instanceDesc = m_BottomLevelInstanceDescsStaging.at(i);
				instanceDesc.AccelerationStructure = bottomLevelAS.GetResource()->GetGPUVirtualAddress();
			}
		}
	}
}


UINT RaytracingAccelerationStructureManager::AddBottomLevelASInstance(const std::wstring& bottomLevelASName, const XMMATRIX& transform, BYTE instanceMask)
{
	ASSERT(m_NumBottomLevelInstances < m_BottomLevelInstanceDescsStaging.size(), "Not enough instance desc buffer size.");
	ASSERT(m_BottomLevelASNames.find(bottomLevelASName) != m_BottomLevelASNames.end(), "Bottom level AS does not exist.");

	const UINT instanceIndex = m_NumBottomLevelInstances++;
	const auto& bottomLevelAS = m_BottomLevelAS.at(m_BottomLevelASNames.at(bottomLevelASName));

	// Populate the next element in the staging buffer
	D3D12_RAYTRACING_INSTANCE_DESC& instanceDesc = m_BottomLevelInstanceDescsStaging.at(instanceIndex);
	instanceDesc.InstanceMask = instanceMask;
	instanceDesc.InstanceContributionToHitGroupIndex = bottomLevelAS.GetInstanceContributionToHitGroupIndex();
	instanceDesc.AccelerationStructure = bottomLevelAS.GetResource()->GetGPUVirtualAddress();
	XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(instanceDesc.Transform), transform);

	auto& instanceMetadata = m_InstanceMetadata.at(instanceIndex);
	instanceMetadata.BLASIndex = m_BottomLevelASNames.at(bottomLevelASName);

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

	PIXBeginEvent(commandList, PIX_COLOR_INDEX(62), "Build Acceleration Structure");
	PROFILE_DIRECT_PUSH_RANGE("Build AS", commandList);

	// Copy staging buffer to GPU
	m_BottomLevelInstanceDescs.at(frame).CopyElements(0, m_NumBottomLevelInstances, m_BottomLevelInstanceDescsStaging.data());

	// Build all bottom level AS
	{
		// Group all resource barriers together since each BLAS has its own scratch resource
		std::vector<CD3DX12_RESOURCE_BARRIER> barriers;
		barriers.reserve(m_BottomLevelAS.size());

		for (auto& bottomLevelAS : m_BottomLevelAS)
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

	PROFILE_DIRECT_POP_RANGE(commandList);
	PIXEndEvent(commandList);
}

void RaytracingAccelerationStructureManager::SetInstanceTransform(UINT instanceIndex, const XMMATRIX& transform)
{
	ASSERT(instanceIndex < m_NumBottomLevelInstances, "Instance index out of bounds.");

	auto& instanceDesc = m_BottomLevelInstanceDescsStaging.at(instanceIndex);
	XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(instanceDesc.Transform), transform);
}


void RaytracingAccelerationStructureManager::SetBLASInstanceContributionToHitGroup(const std::wstring& blas, UINT instanceContributionToHitGroupIndex)
{
	//NOT_IMPLEMENTED; // TODO: This function is horribly written and should not be used until it is amended
	// This must be done through the AS manager as the instance buffer requires updating

	// Make sure BLAS exists
	ASSERT(m_BottomLevelASNames.find(blas) != m_BottomLevelASNames.end(), "BLAS doesn't exist!");

	m_BottomLevelAS.at(m_BottomLevelASNames.at(blas)).SetInstanceContributionToHitGroupIndex(instanceContributionToHitGroupIndex);
	const D3D12_GPU_VIRTUAL_ADDRESS blasAddress = m_BottomLevelAS.at(m_BottomLevelASNames.at(blas)).GetResource()->GetGPUVirtualAddress();

	for (auto& instanceDesc : m_BottomLevelInstanceDescsStaging)
	{
		// find all instances that point to the specified blas
		if (instanceDesc.AccelerationStructure == blasAddress)
		{
			instanceDesc.InstanceContributionToHitGroupIndex = instanceContributionToHitGroupIndex;
		}
	}
}

