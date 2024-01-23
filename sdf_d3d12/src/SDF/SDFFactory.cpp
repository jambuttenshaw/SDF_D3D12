#include "pch.h"
#include "SDFFactory.h"

#include "Renderer/D3DGraphicsContext.h"
#include "Renderer/Buffer/ReadbackBuffer.h"
#include "Renderer/Hlsl/ComputeHlslCompat.h"


#include "SDFObject.h"
#include "SDFEditList.h"


// Pipeline signatures
namespace AABBBuilderComputeRootSignature
{
	enum Value
	{
		BuildParameterSlot = 0,
		EditListSlot,
		CounterResourceSlot,
		AABBBufferSlot,
		AABBPrimitiveDataSlot,
		Count
	};
}

namespace BrickBuildComputeRootSignature
{
	enum Value
	{
		BuildParameterSlot = 0,
		EditListSlot,
		AABBPrimitiveDataSlot,
		OutputVolumeSlot,
		Count
	};
}





SDFFactory::SDFFactory()
{
	const auto device = g_D3DGraphicsContext->GetDevice();

	// Create command queue, allocator, and list
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	THROW_IF_FAIL(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_CommandQueue)));

	THROW_IF_FAIL(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_CommandAllocator)));
	THROW_IF_FAIL(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_CommandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_CommandList)));

	THROW_IF_FAIL(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence)));
	m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (!m_FenceEvent)
	{
		LOG_FATAL("Failed to create fence event.");
	}
	m_FenceValue = 1;


	// Create compute pipelines
	InitializePipelines();

	m_CounterResource.Allocate(device, L"AABB Counter");
	m_CounterResourceUAV = g_D3DGraphicsContext->GetSRVHeap()->Allocate(1);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = 1;
	uavDesc.Buffer.StructureByteStride = 0;
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

	device->CreateUnorderedAccessView(m_CounterResource.GetResource(), nullptr, &uavDesc, m_CounterResourceUAV.GetCPUHandle());
}

SDFFactory::~SDFFactory()
{
	m_CounterResourceUAV.Free();

	CloseHandle(m_FenceEvent);
}


void SDFFactory::BakeSDFSynchronous(SDFObject* object, const SDFEditList& editList)
{
	const auto device = g_D3DGraphicsContext->GetDevice();

	// Step 1: Setup constant buffer data and counter temporary resources
	SDFBuilderConstantBuffer buildParamsBuffer;
	UploadBuffer<UINT32> counterUpload;
	ReadbackBuffer<UINT32> counterReadback;
	{
		// Copy edit list
		editList.CopyStagingToGPU();

		// Populate build params
		buildParamsBuffer.EvalSpace_MinBoundary = { -1.0f, -1.0f, -1.0f, 0.0f };
		buildParamsBuffer.EvalSpace_MaxBoundary = { 1.0f,  1.0f,  1.0f, 0.0f };
		// TODO: Hard-code 16 bricks per axis
		buildParamsBuffer.EvalSpace_BrickSize = 0.125f; // Eval space domain / BRICK_SIZE

		const auto bricksPerAxis = (buildParamsBuffer.EvalSpace_MaxBoundary - buildParamsBuffer.EvalSpace_MinBoundary) / buildParamsBuffer.EvalSpace_BrickSize;
		XMStoreUInt3(&buildParamsBuffer.EvalSpace_BricksPerAxis, bricksPerAxis);

		buildParamsBuffer.BrickPool_BrickCapacityPerAxis = object->GetBrickCapacityPerAxis();

		buildParamsBuffer.SDFEditCount = editList.GetEditCount();

		// Setup counter temporary resources
		counterUpload.Allocate(device, 1, 0, L"Counter upload");
		counterReadback.Allocate(device, 1, 0, L"Counter Readback");
		// Copy 0 into the counter
		counterUpload.CopyElement(0, 0);
	}

	// TODO: Temporary Step
	// Fill the volume with 1's
	ComPtr<ID3D12Resource> copyVolume;
	{
		const UINT width = object->GetBrickCapacityPerAxis().x * SDF_BRICK_SIZE_IN_VOXELS;
		const UINT height = object->GetBrickCapacityPerAxis().y * SDF_BRICK_SIZE_IN_VOXELS;
		const UINT depth = object->GetBrickCapacityPerAxis().z * SDF_BRICK_SIZE_IN_VOXELS;
		const UINT64 bufferSize = static_cast<UINT64>(width) * height * depth * sizeof(BYTE);

		const auto uploadHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		const auto desc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
		THROW_IF_FAIL(device->CreateCommittedResource(
			&uploadHeap,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&copyVolume)));

		// Initialize a vector with all 1's
		const std::vector<BYTE> volumeData(width * width * width, 0x7F);	

		// Copy this into the copy volume
		BYTE* pVolume;
		THROW_IF_FAIL(copyVolume->Map(0, nullptr, reinterpret_cast<void**>(&pVolume)));
		memcpy(pVolume, volumeData.data(), volumeData.size());
		copyVolume->Unmap(0, nullptr);

		// Copy this resource into the volume texture
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(object->GetVolumeResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
		m_CommandList->ResourceBarrier(1, &barrier);

		D3D12_TEXTURE_COPY_LOCATION dest;
		dest.pResource = object->GetVolumeResource();
		dest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dest.SubresourceIndex = 0;

		D3D12_TEXTURE_COPY_LOCATION src;
		src.pResource = copyVolume.Get();
		src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		src.PlacedFootprint.Offset = 0;
		src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8_UINT;
		src.PlacedFootprint.Footprint.Width = width;
		src.PlacedFootprint.Footprint.Height = height;
		src.PlacedFootprint.Footprint.Depth = depth;
		src.PlacedFootprint.Footprint.RowPitch = sizeof(BYTE) * width;

		m_CommandList->CopyTextureRegion(&dest, 0, 0, 0, &src, nullptr);

		barrier = CD3DX12_RESOURCE_BARRIER::Transition(object->GetVolumeResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		m_CommandList->ResourceBarrier(1, &barrier);
	}

	// Step 2: Build command list to execute AABB builder compute shader
	{
		ID3D12DescriptorHeap* ppDescriptorHeaps[] = { g_D3DGraphicsContext->GetSRVHeap()->GetHeap() };
		m_CommandList->SetDescriptorHeaps(_countof(ppDescriptorHeaps), ppDescriptorHeaps);

		// Copy the initial counter value into the counter
		m_CounterResource.SetValue(m_CommandList.Get(), counterUpload.GetResource());

		// Setup pipeline
		m_BrickBuilderPipeline->Bind(m_CommandList.Get());

		// Set resources
		m_CommandList->SetComputeRoot32BitConstants(AABBBuilderComputeRootSignature::BuildParameterSlot, SizeOfInUint32(buildParamsBuffer), &buildParamsBuffer, 0);
		m_CommandList->SetComputeRootShaderResourceView(AABBBuilderComputeRootSignature::EditListSlot, editList.GetEditBufferAddress());
		m_CommandList->SetComputeRootDescriptorTable(AABBBuilderComputeRootSignature::CounterResourceSlot, m_CounterResourceUAV.GetGPUHandle());
		m_CommandList->SetComputeRootDescriptorTable(AABBBuilderComputeRootSignature::AABBBufferSlot, object->GetAABBBufferUAV());
		m_CommandList->SetComputeRootDescriptorTable(AABBBuilderComputeRootSignature::AABBPrimitiveDataSlot, object->GetPrimitiveDataBufferUAV());

		// Calculate number of thread groups
		
		const UINT threadGroupX = (buildParamsBuffer.EvalSpace_BricksPerAxis.x + AABB_BUILD_NUM_THREADS_PER_GROUP - 1) / AABB_BUILD_NUM_THREADS_PER_GROUP;
		const UINT threadGroupY = (buildParamsBuffer.EvalSpace_BricksPerAxis.y + AABB_BUILD_NUM_THREADS_PER_GROUP - 1) / AABB_BUILD_NUM_THREADS_PER_GROUP;
		const UINT threadGroupZ = (buildParamsBuffer.EvalSpace_BricksPerAxis.z + AABB_BUILD_NUM_THREADS_PER_GROUP - 1) / AABB_BUILD_NUM_THREADS_PER_GROUP;

		// Dispatch
		m_CommandList->Dispatch(threadGroupX, threadGroupY, threadGroupZ);

		// Copy counter value to a readback resource
		m_CounterResource.ReadValue(m_CommandList.Get(), counterReadback.GetResource());
	}

	// Step 3: Execute command list and wait until completion
	{
		Flush();

		THROW_IF_FAIL(m_CommandAllocator->Reset());
		THROW_IF_FAIL(m_CommandList->Reset(m_CommandAllocator.Get(), nullptr));
	}

	// Step 4: Read counter value
	{
		// It is only save to read the counter value after the GPU has finished its work
		const UINT brickCount = counterReadback.ReadElement(0);
		object->SetBrickCount(brickCount);
	}

	// Step 5: Build command list to execute SDF baker compute shader
	{
		ID3D12DescriptorHeap* ppDescriptorHeaps[] = { g_D3DGraphicsContext->GetSRVHeap()->GetHeap() };
		m_CommandList->SetDescriptorHeaps(_countof(ppDescriptorHeaps), ppDescriptorHeaps);

		// Scene texture must be in unordered access state
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(object->GetVolumeResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		m_CommandList->ResourceBarrier(1, &barrier);

		// Set pipeline state
		m_BrickEvaluatorPipeline->Bind(m_CommandList.Get());

		// Set resource views
		m_CommandList->SetComputeRoot32BitConstants(BrickBuildComputeRootSignature::BuildParameterSlot, SizeOfInUint32(buildParamsBuffer), &buildParamsBuffer, 0);
		m_CommandList->SetComputeRootShaderResourceView(BrickBuildComputeRootSignature::EditListSlot, editList.GetEditBufferAddress());
		m_CommandList->SetComputeRootShaderResourceView(BrickBuildComputeRootSignature::AABBPrimitiveDataSlot, object->GetPrimitiveDataBufferAddress());
		m_CommandList->SetComputeRootDescriptorTable(BrickBuildComputeRootSignature::OutputVolumeSlot, object->GetVolumeUAV());

		// Execute one group for each AABB
		const UINT threadGroupX = object->GetAABBCount();

		// Dispatch
		m_CommandList->Dispatch(threadGroupX, 1, 1);

		// Volume resource will now be read from in the subsequent stages
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(object->GetVolumeResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		m_CommandList->ResourceBarrier(1, &barrier);
	}

	// Step 6: Execute command list and wait until completion
	{
		Flush();
	}

	// Step 7: Clean up
	{
		THROW_IF_FAIL(m_CommandAllocator->Reset());
		THROW_IF_FAIL(m_CommandList->Reset(m_CommandAllocator.Get(), nullptr));
	}
}


void SDFFactory::InitializePipelines()
{
	// Create pipeline to build array of AABB geometry to fit the volume texture
	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[3];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);
		ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2);

		CD3DX12_ROOT_PARAMETER1 rootParameters[AABBBuilderComputeRootSignature::Count];
		rootParameters[AABBBuilderComputeRootSignature::BuildParameterSlot].InitAsConstants(SizeOfInUint32(SDFBuilderConstantBuffer), 0);
		rootParameters[AABBBuilderComputeRootSignature::EditListSlot].InitAsShaderResourceView(0);
		rootParameters[AABBBuilderComputeRootSignature::CounterResourceSlot].InitAsDescriptorTable(1, &ranges[0]);
		rootParameters[AABBBuilderComputeRootSignature::AABBBufferSlot].InitAsDescriptorTable(1, &ranges[1]);
		rootParameters[AABBBuilderComputeRootSignature::AABBPrimitiveDataSlot].InitAsDescriptorTable(1, &ranges[2]);

		D3DComputePipelineDesc desc;
		desc.NumRootParameters = ARRAYSIZE(rootParameters);
		desc.RootParameters = rootParameters;
		desc.Shader = L"assets/shaders/compute/brick_builder.hlsl";
		desc.EntryPoint = L"main";
		desc.Defines = nullptr;

		m_BrickBuilderPipeline = std::make_unique<D3DComputePipeline>(&desc);
	}

	// Create pipeline that is used to bake SDFs into 3D textures
	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);

		CD3DX12_ROOT_PARAMETER1 rootParameters[BrickBuildComputeRootSignature::Count];
		rootParameters[BrickBuildComputeRootSignature::BuildParameterSlot].InitAsConstants(SizeOfInUint32(SDFBuilderConstantBuffer), 0);
		rootParameters[BrickBuildComputeRootSignature::EditListSlot].InitAsShaderResourceView(0);
		rootParameters[BrickBuildComputeRootSignature::AABBPrimitiveDataSlot].InitAsShaderResourceView(1);
		rootParameters[BrickBuildComputeRootSignature::OutputVolumeSlot].InitAsDescriptorTable(1, &ranges[0]);

		D3DComputePipelineDesc desc;
		desc.NumRootParameters = ARRAYSIZE(rootParameters);
		desc.RootParameters = rootParameters;
		desc.Shader = L"assets/shaders/compute/brick_evaluator.hlsl";
		desc.EntryPoint = L"main";
		desc.Defines = nullptr;

		m_BrickEvaluatorPipeline = std::make_unique<D3DComputePipeline>(&desc);
	}
}


void SDFFactory::Flush()
{
	THROW_IF_FAIL(m_CommandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
	m_CommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	const uint64_t fence = m_FenceValue++;
	m_CommandQueue->Signal(m_Fence.Get(), fence);
	if (m_Fence->GetCompletedValue() < fence)								// block until async compute has completed using a fence
	{
		m_Fence->SetEventOnCompletion(fence, m_FenceEvent);
		WaitForSingleObject(m_FenceEvent, INFINITE);
	}
}
