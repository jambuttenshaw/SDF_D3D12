#include "pch.h"
#include "Raytracer.h"

#include "Renderer/D3DGraphicsContext.h"

#include "Application/Scene.h"
#include "RaytracingSceneDefines.h"
#include "Renderer/D3DShaderCompiler.h"
#include "Renderer/ShaderTable.h"

#include "Framework/PickingQueryInterface.h"

#include "pix3.h"
#include "Renderer/Profiling/GPUProfiler.h"


const wchar_t* Raytracer::c_HitGroupName[] = {
	L"SDFHitGroup",
	L"SDFHitGroup_ShadowRay"
};
const wchar_t* Raytracer::c_RaygenShaderName = L"PrimaryRaygenShader";
const wchar_t* Raytracer::c_IntersectionShaderName = L"SDFIntersectionShader";
const wchar_t* Raytracer::c_ClosestHitShaderName = L"SDFClosestHitShader";
const wchar_t* Raytracer::c_MissShaderName[] = 
{
	L"PrimaryMissShader",
	L"ShadowMissShader"
};


void PopulateLocalRootParams(const SDFObject* object, LocalRootSignatureParams::RootArguments& rootArgs)
{
	rootArgs.brickProperties.BrickSize = object->GetBrickSize(SDFObject::RESOURCES_READ);
	rootArgs.brickProperties.BrickCount = object->GetBrickCount(SDFObject::RESOURCES_READ);

	rootArgs.brickProperties.BrickPoolDimensions = object->GetBrickPoolResolution(SDFObject::RESOURCES_READ);
	const XMVECTOR uvwPerBrick = XMVECTOR({ 1.0f, 1.0f, 1.0f, 1.0f }) / XMLoadUInt3(&rootArgs.brickProperties.BrickPoolDimensions);
	XMStoreFloat3(&rootArgs.brickProperties.UVWPerVoxel, uvwPerBrick);

	rootArgs.brickPoolSRV = object->GetDescriptor(SDFObject::RESOURCES_READ, SDFObject::POOL_SRV);
	rootArgs.brickBuffer = object->GetBrickBufferAddress(SDFObject::RESOURCES_READ);

	memcpy(&rootArgs.materialTable, object->GetMaterialTablePtr(), object->GetMaterialTableSize());
}



Raytracer::Raytracer()
{
	LOG_INFO("Initializing raytracer...");
	// Create resources

	// Create root signatures for the shaders.
	CreateRootSignatures();

	// Create a raytracing pipeline state object which defines the binding of shaders, state and resources to be used during raytracing.
	CreateRaytracingPipelineStateObject();

	// Create an output 2D texture to store the raytracing result to.
	CreateRaytracingOutputResource();

	CreateSamplers();

	LOG_INFO("Raytracing initialization complete.");
}

Raytracer::~Raytracer()
{
	m_RaytracingOutputDescriptor.Free();
	m_Samplers.Free();
}


void Raytracer::Setup(const Scene& scene)
{
	m_Scene = &scene;

	BuildShaderTables();
}


void Raytracer::DoRaytracing(const RaytracingParams& params) const
{
	ASSERT(m_Scene, "No scene to raytrace!");

	// Update shader tables
	UpdateHitGroupShaderTable();
	m_HitGroupShaderTable->CopyStagingToGPU();
	m_MissShaderTable->CopyStagingToGPU();
	m_RayGenShaderTable->CopyStagingToGPU();

	const auto commandList = g_D3DGraphicsContext->GetCommandList();
	const auto dxrCommandList = g_D3DGraphicsContext->GetDXRCommandList();

	PIXBeginEvent(commandList, PIX_COLOR_INDEX(7), "Do Raytracing");
	PROFILE_DIRECT_PUSH_RANGE("Do Raytracing", commandList);

	// Perform raytracing commands 
	commandList->SetComputeRootSignature(m_RaytracingGlobalRootSignature.Get());

	// Bind the heaps, acceleration structure and dispatch rays.    
	commandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams::OutputViewSlot, m_RaytracingOutputDescriptor.GetGPUHandle());

	commandList->SetComputeRootShaderResourceView(GlobalRootSignatureParams::AccelerationStructureSlot, m_Scene->GetRaytracingAccelerationStructure()->GetTopLevelAccelerationStructureAddress());

	commandList->SetComputeRootConstantBufferView(GlobalRootSignatureParams::PassBufferSlot, g_D3DGraphicsContext->GetPassCBAddress());

	commandList->SetComputeRootShaderResourceView(GlobalRootSignatureParams::MaterialBufferSlot, params.MaterialBuffer);

	commandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams::GlobalLightingSRVSlot, params.GlobalLightingSRVTable);
	commandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams::GlobalLightingSamplerSlot, params.GlobalLightingSamplerTable);

	commandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams::VolumeSamplerSlot, m_Samplers.GetGPUHandle(m_CurrentSampler));

	commandList->SetComputeRootConstantBufferView(GlobalRootSignatureParams::PickingParamsSlot, params.PickingInterface->GetPickingParamsBuffer());
	commandList->SetComputeRootUnorderedAccessView(GlobalRootSignatureParams::PickingOutputSlot, params.PickingInterface->GetPickingOutputBuffer());


	dxrCommandList->SetPipelineState1(m_DXRStateObject.Get());

	D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
	dispatchDesc.HitGroupTable.StartAddress = m_HitGroupShaderTable->GetAddress();
	dispatchDesc.HitGroupTable.SizeInBytes = m_HitGroupShaderTable->GetSize();
	dispatchDesc.HitGroupTable.StrideInBytes = m_HitGroupShaderTable->GetStride();

	dispatchDesc.MissShaderTable.StartAddress = m_MissShaderTable->GetAddress();
	dispatchDesc.MissShaderTable.SizeInBytes = m_MissShaderTable->GetSize();
	dispatchDesc.MissShaderTable.StrideInBytes = m_MissShaderTable->GetStride();

	dispatchDesc.RayGenerationShaderRecord.StartAddress = m_RayGenShaderTable->GetAddress();
	dispatchDesc.RayGenerationShaderRecord.SizeInBytes = m_RayGenShaderTable->GetStride(); // size of one element

	dispatchDesc.Width = g_D3DGraphicsContext->GetClientWidth();
	dispatchDesc.Height = g_D3DGraphicsContext->GetClientHeight();
	dispatchDesc.Depth = 1;

	dxrCommandList->DispatchRays(&dispatchDesc);

	PROFILE_DIRECT_POP_RANGE(commandList);
	PIXEndEvent(commandList);
}



void Raytracer::Resize()
{
	m_RaytracingOutput.Reset();
	m_RaytracingOutputDescriptor.Free();

	CreateRaytracingOutputResource();
}

void Raytracer::SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* rootSig) const
{
	ComPtr<ID3DBlob> blob;
	ComPtr<ID3DBlob> error;

	THROW_IF_FAIL(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error));
	THROW_IF_FAIL(g_D3DGraphicsContext->GetDevice()->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(*rootSig))));
}

void Raytracer::CreateRootSignatures()
{
	// Global Root Signature
	// This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
	{
		CD3DX12_DESCRIPTOR_RANGE ranges[4];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 2);
		ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 2, 0);
		ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 2);

		CD3DX12_ROOT_PARAMETER rootParameters[GlobalRootSignatureParams::Count];

		rootParameters[GlobalRootSignatureParams::OutputViewSlot].InitAsDescriptorTable(1, &ranges[0]);

		rootParameters[GlobalRootSignatureParams::AccelerationStructureSlot].InitAsShaderResourceView(0);

		rootParameters[GlobalRootSignatureParams::PassBufferSlot].InitAsConstantBufferView(0);

		rootParameters[GlobalRootSignatureParams::MaterialBufferSlot].InitAsShaderResourceView(1);

		rootParameters[GlobalRootSignatureParams::GlobalLightingSRVSlot].InitAsDescriptorTable(1, &ranges[1]);
		rootParameters[GlobalRootSignatureParams::GlobalLightingSamplerSlot].InitAsDescriptorTable(1, &ranges[2]);

		rootParameters[GlobalRootSignatureParams::VolumeSamplerSlot].InitAsDescriptorTable(1, &ranges[3]);

		rootParameters[GlobalRootSignatureParams::PickingParamsSlot].InitAsConstantBufferView(1);
		rootParameters[GlobalRootSignatureParams::PickingOutputSlot].InitAsUnorderedAccessView(1);

		CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters, 0, nullptr);
		SerializeAndCreateRaytracingRootSignature(globalRootSignatureDesc, &m_RaytracingGlobalRootSignature);
	}


	// Local Root Signature
	// This root signature is only used by the hit group
	{
		// brick pool descriptor
		CD3DX12_DESCRIPTOR_RANGE SRVDescriptor;
		SRVDescriptor.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 1);

		CD3DX12_ROOT_PARAMETER rootParameters[LocalRootSignatureParams::Count];
		rootParameters[LocalRootSignatureParams::BrickPropertiesBuffer].InitAsConstants(SizeOfInUint32(BrickPropertiesConstantBuffer), 0, 1);
		rootParameters[LocalRootSignatureParams::BrickPoolSlot].InitAsDescriptorTable(1, &SRVDescriptor);
		rootParameters[LocalRootSignatureParams::BrickBufferSlot].InitAsShaderResourceView(1, 1);
		rootParameters[LocalRootSignatureParams::MaterialTableSlot].InitAsConstants(SizeOfInUint32(XMUINT4), 1, 1);

		CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);
		SerializeAndCreateRaytracingRootSignature(localRootSignatureDesc, &m_RaytracingLocalRootSignature);
	}
}



void Raytracer::CreateRaytracingPipelineStateObject()
{
	LOG_INFO("Create pipeline state object...");

	CD3DX12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

	// DXIL library
	const auto lib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();

	ComPtr<IDxcBlob> blob;
	THROW_IF_FAIL(D3DShaderCompiler::CompileFromFile(L"assets/shaders/raytracing/raytracing.hlsl", L"main", L"lib", {}, &blob));
	D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE(blob->GetBufferPointer(), blob->GetBufferSize());

	lib->SetDXILLibrary(&libdxil);
	// Define which shader exports to surface from the library.
	// If no shader exports are defined for a DXIL library subobject, all shaders will be surfaced.
	// In this sample, this could be omitted for convenience since the sample uses all shaders in the library. 
	{
		lib->DefineExport(c_RaygenShaderName);
		lib->DefineExport(c_IntersectionShaderName);
		lib->DefineExport(c_ClosestHitShaderName);
		for (const auto& missShader : c_MissShaderName)
			lib->DefineExport(missShader);
	}

	// Create hit groups
	// There will be separate hit groups for primary and shadow rays
	for (UINT rayType = 0; rayType < RayType_Count; ++rayType)
	{
		const auto hitGroup = raytracingPipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
		hitGroup->SetIntersectionShaderImport(c_IntersectionShaderName);
		if (rayType == RayType_Primary)
			hitGroup->SetClosestHitShaderImport(c_ClosestHitShaderName);
		hitGroup->SetHitGroupExport(c_HitGroupName[rayType]);
		hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE);
	}


	// Create and associate a local root signature with the hit group
	const auto localRootSig = raytracingPipeline.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
	localRootSig->SetRootSignature(m_RaytracingLocalRootSignature.Get());

	const auto localRootSigAssociation = raytracingPipeline.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
	localRootSigAssociation->SetSubobjectToAssociate(*localRootSig);
	localRootSigAssociation->AddExport(c_HitGroupName[RayType_Primary]);


	// Shader config
	// Defines the maximum sizes in bytes for the ray payload and attribute structure.
	const auto shaderConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
	constexpr UINT payloadSize = max(sizeof(RadianceRayPayload), sizeof(ShadowRayPayload));
	constexpr UINT attributeSize = sizeof(SDFIntersectAttrib);
	shaderConfig->Config(payloadSize, attributeSize);

	// Global root signature
	// This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
	const auto globalRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	globalRootSignature->SetRootSignature(m_RaytracingGlobalRootSignature.Get());

	// Pipeline config
	// Defines the maximum TraceRay() recursion depth.
	const auto pipelineConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
	constexpr UINT maxRecursionDepth = MAX_RAY_RECURSION_DEPTH; 
	pipelineConfig->Config(maxRecursionDepth);

	// Create the state object.
	THROW_IF_FAIL(g_D3DGraphicsContext->GetDXRDevice()->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&m_DXRStateObject)));

	LOG_INFO("Pipeline state object created.");
}

// Create 2D output texture for raytracing.
void Raytracer::CreateRaytracingOutputResource()
{
	const auto device = g_D3DGraphicsContext->GetDevice();

	// Create the output resource. The dimensions and format should match the swap-chain.
	const auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	const auto uavDesc = CD3DX12_RESOURCE_DESC::Tex2D(g_D3DGraphicsContext->GetBackBufferFormat(), 
		g_D3DGraphicsContext->GetClientWidth(), g_D3DGraphicsContext->GetClientHeight(),
		1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	THROW_IF_FAIL(device->CreateCommittedResource(
		&defaultHeapProperties,
		D3D12_HEAP_FLAG_NONE, 
		&uavDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 
		nullptr, 
		IID_PPV_ARGS(&m_RaytracingOutput)));
	m_RaytracingOutput->SetName(L"Raytracing output");

	m_RaytracingOutputDescriptor = g_D3DGraphicsContext->GetSRVHeap()->Allocate(1);

	device->CreateUnorderedAccessView(m_RaytracingOutput.Get(), nullptr, nullptr, m_RaytracingOutputDescriptor.GetCPUHandle());
}

void Raytracer::CreateSamplers()
{
	// Allocate descriptors to contain the samplers
	m_Samplers = g_D3DGraphicsContext->GetSamplerHeap()->Allocate(2);
	ASSERT(m_Samplers.IsValid(), "Failed to allocate descriptors!");

	// Create a static sampler
	D3D12_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.MipLODBias = 0;
	samplerDesc.MaxAnisotropy = 0;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	samplerDesc.BorderColor[0] = 0.0f;
	samplerDesc.BorderColor[1] = 0.0f;
	samplerDesc.BorderColor[2] = 0.0f;
	samplerDesc.BorderColor[3] = 1;
	samplerDesc.MinLOD = 0.0f;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	g_D3DGraphicsContext->GetDevice()->CreateSampler(&samplerDesc, m_Samplers.GetCPUHandle(0));

	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	g_D3DGraphicsContext->GetDevice()->CreateSampler(&samplerDesc, m_Samplers.GetCPUHandle(1));
}


// Build shader tables.
// This encapsulates all shader records - shaders and the arguments for their local root signatures.
void Raytracer::BuildShaderTables()
{
	ASSERT(m_Scene, "No scene to raytrace!");
	LOG_INFO("Building shader tables...");

	const auto device = g_D3DGraphicsContext->GetDevice();

	void* rayGenShaderIdentifier;
	void* missShaderIdentifiers[RayType_Count];
	void* hitGroupShaderIdentifiers[RayType_Count];

	constexpr UINT shaderIDSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

	{
		// Get shader identifiers.
		ComPtr<ID3D12StateObjectProperties> stateObjectProperties;
		THROW_IF_FAIL(m_DXRStateObject.As(&stateObjectProperties));

		rayGenShaderIdentifier = stateObjectProperties->GetShaderIdentifier(c_RaygenShaderName);
		for (UINT rayType = 0; rayType < RayType_Count; ++rayType)
			missShaderIdentifiers[rayType] = stateObjectProperties->GetShaderIdentifier(c_MissShaderName[rayType]);
		for (UINT rayType = 0; rayType < RayType_Count; ++rayType)
			hitGroupShaderIdentifiers[rayType] = stateObjectProperties->GetShaderIdentifier(c_HitGroupName[rayType]);
	}

	/*************--------- Shader table layout -------*******************
	| --------------------------------------------------------------------
	| Shader table - HitGroupShaderTable:
	| [0] : SDFHitGroup
	| [1] : SDFHitGroup_ShadowRay
	| --------------------------------------------------------------------
	**********************************************************************/

	// Ray gen shader table
	{
		UINT numShaderRecords = 1;
		UINT shaderRecordSize = shaderIDSize;

		m_RayGenShaderTable = std::make_unique<ShaderTable>(device, numShaderRecords, shaderRecordSize, L"RayGenShaderTable");
		m_RayGenShaderTable->AddRecord(ShaderRecord{ rayGenShaderIdentifier, shaderIDSize });
	}

	// Miss shader table
	{
		UINT numShaderRecords = RayType_Count;
		UINT shaderRecordSize = shaderIDSize;

		m_MissShaderTable = std::make_unique<ShaderTable>(device, numShaderRecords, shaderRecordSize, L"MissShaderTable");
		for (const auto& missGroupID : missShaderIdentifiers)
		{
			m_MissShaderTable->AddRecord(ShaderRecord{ missGroupID, shaderIDSize});
		}
	}

	// Hit group shader table

	// Construct an entry for each geometry instance
	{
		const auto& bottomLevelASGeometries = m_Scene->GetAllGeometries();
		const auto accelerationStructure = m_Scene->GetRaytracingAccelerationStructure();

		UINT numShaderRecords = 0;
		for (auto& bottomLevelASGeometry : bottomLevelASGeometries)
		{
			// one type of geometry per BLAS
			numShaderRecords += RayType_Count;
		}

		UINT shaderRecordSize = shaderIDSize + sizeof(LocalRootSignatureParams::RootArguments);
		m_HitGroupShaderTable = std::make_unique<ShaderTable>(device, numShaderRecords, shaderRecordSize, L"HitGroupShaderTable");

		for (auto& bottomLevelASGeometry : bottomLevelASGeometries)
		{
			const UINT shaderRecordOffset = m_HitGroupShaderTable->GetNumRecords();
			accelerationStructure->SetBLASInstanceContributionToHitGroup(bottomLevelASGeometry.Name, shaderRecordOffset);

			const auto geometryInstance = bottomLevelASGeometry.Geometry;
			geometryInstance->SetShaderRecordOffset(m_HitGroupShaderTable->GetNumRecords());

			LocalRootSignatureParams::RootArguments rootArgs;
			PopulateLocalRootParams(geometryInstance, rootArgs);

			for (const auto& hitGroupID: hitGroupShaderIdentifiers)
			{
				m_HitGroupShaderTable->AddRecord(ShaderRecord{
					hitGroupID,
					shaderIDSize,
					&rootArgs,
					sizeof(rootArgs)
				});
			}

			geometryInstance->ResetLocalArgsDirty();
		}
	}

	LOG_INFO("Shader tables construction complete.");
}

void Raytracer::UpdateHitGroupShaderTable() const
{
	void* hitGroupShaderIdentifiers[RayType_Count];
	constexpr UINT shaderIDSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

	{
		ComPtr<ID3D12StateObjectProperties> stateObjectProperties;
		THROW_IF_FAIL(m_DXRStateObject.As(&stateObjectProperties));

		for (UINT rayType = 0; rayType < RayType_Count; ++rayType)
			hitGroupShaderIdentifiers[rayType] = stateObjectProperties->GetShaderIdentifier(c_HitGroupName[rayType]);
	}

	// Update the entry for each geometry instance
	for (auto& bottomLevelASGeometry : m_Scene->GetAllGeometries())
	{
		const auto geometryInstance = bottomLevelASGeometry.Geometry;
		if (geometryInstance->IsLocalArgsDirty())
		{
			LocalRootSignatureParams::RootArguments rootArgs;
			PopulateLocalRootParams(geometryInstance, rootArgs);

			for (UINT rayType = 0; rayType < RayType_Count; ++rayType)
			{
				m_HitGroupShaderTable->UpdateRecord(geometryInstance->GetShaderRecordOffset() + rayType, 
					ShaderRecord{
						hitGroupShaderIdentifiers[rayType],
						shaderIDSize,
						&rootArgs,
						sizeof(rootArgs)
					});
			}

			geometryInstance->ResetLocalArgsDirty();
		}
	}
}
