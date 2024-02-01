#include "pch.h"
#include "SDFFactory.h"

#include "Renderer/D3DGraphicsContext.h"
#include "Renderer/Buffer/ReadbackBuffer.h"

#include "Renderer/Hlsl/ComputeHlslCompat.h"

#include "SDF/SDFObject.h"
#include "SDF/SDFEditList.h"

#include "pix3.h"


SDFFactory::SDFFactory()
{
	const auto device = g_D3DGraphicsContext->GetDevice();

	THROW_IF_FAIL(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&m_CommandAllocator)));
	m_CommandAllocator->SetName(L"SDFFactory Command Allocator");
	THROW_IF_FAIL(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, m_CommandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_CommandList)));
	m_CommandList->SetName(L"SDFFactory Command List");

	THROW_IF_FAIL(m_CommandList->Close());

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
}

void SDFFactory::InitializePipelines()
{
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
		rootParameters[AABBBuilderComputeRootSignature::BrickBufferSlot].InitAsDescriptorTable(1, &ranges[2]);

		D3DComputePipelineDesc desc;
		desc.NumRootParameters = ARRAYSIZE(rootParameters);
		desc.RootParameters = rootParameters;
		desc.Shader = L"assets/shaders/compute/brick_builder.hlsl";
		desc.EntryPoint = L"main";
		desc.Defines = nullptr;

		m_BrickBuilderPipeline = std::make_unique<D3DComputePipeline>(&desc);
	}

	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);

		CD3DX12_ROOT_PARAMETER1 rootParameters[BrickBuildComputeRootSignature::Count];
		rootParameters[BrickBuildComputeRootSignature::BuildParameterSlot].InitAsConstants(SizeOfInUint32(SDFBuilderConstantBuffer), 0);
		rootParameters[BrickBuildComputeRootSignature::EditListSlot].InitAsShaderResourceView(0);
		rootParameters[BrickBuildComputeRootSignature::BrickBufferSlot].InitAsShaderResourceView(1);
		rootParameters[BrickBuildComputeRootSignature::BrickPoolSlot].InitAsDescriptorTable(1, &ranges[0]);

		D3DComputePipelineDesc desc;
		desc.NumRootParameters = ARRAYSIZE(rootParameters);
		desc.RootParameters = rootParameters;
		desc.Shader = L"assets/shaders/compute/brick_evaluator.hlsl";
		desc.EntryPoint = L"main";
		desc.Defines = nullptr;

		m_BrickEvaluatorPipeline = std::make_unique<D3DComputePipeline>(&desc);
	}
}


void SDFFactory::PerformSDFBake_CPUBlocking(SDFObject* object, const SDFEditList& editList)
{
	const auto device = g_D3DGraphicsContext->GetDevice();
	const auto computeQueue = g_D3DGraphicsContext->GetComputeCommandQueue();

	THROW_IF_FAIL(m_CommandAllocator->Reset());
	THROW_IF_FAIL(m_CommandList->Reset(m_CommandAllocator.Get(), nullptr));

	PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_DEFAULT, "SDF Bake");

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
		buildParamsBuffer.EvalSpace_BrickSize = object->GetBrickSize();

		const auto bricksPerAxis = (buildParamsBuffer.EvalSpace_MaxBoundary - buildParamsBuffer.EvalSpace_MinBoundary) / buildParamsBuffer.EvalSpace_BrickSize;
		XMStoreUInt3(&buildParamsBuffer.EvalSpace_BricksPerAxis, bricksPerAxis);

		// Make sure the object is large enough to store this many bricks
		const UINT requiredCapacity = buildParamsBuffer.EvalSpace_BricksPerAxis.x * buildParamsBuffer.EvalSpace_BricksPerAxis.y * buildParamsBuffer.EvalSpace_BricksPerAxis.z;
		ASSERT(requiredCapacity <= object->GetBrickBufferCapacity(), "Object does not have large enough brick capacity and buffer overflow is likely!");

		// Brick capacity has not been determined until brick builder has executed
		buildParamsBuffer.BrickPool_BrickCapacityPerAxis = XMUINT3{ 0, 0, 0 };

		// Helpful values that can be calculated once on the CPU
		XMFLOAT3 evalSpaceRange;
		XMStoreFloat3(&evalSpaceRange, buildParamsBuffer.EvalSpace_MaxBoundary - buildParamsBuffer.EvalSpace_MinBoundary);
		buildParamsBuffer.EvalSpace_VoxelsPerUnit = static_cast<float>(buildParamsBuffer.EvalSpace_BricksPerAxis.x * SDF_BRICK_SIZE_VOXELS) / evalSpaceRange.x;

		buildParamsBuffer.SDFEditCount = editList.GetEditCount();

		// Setup counter temporary resources
		counterUpload.Allocate(device, 1, 0, L"Counter upload");
		counterReadback.Allocate(device, 1, 0, L"Counter Readback");
		// Copy 0 into the counter
		counterUpload.CopyElement(0, 0);
	}

	// Step 2: Build command list to execute AABB builder compute shader
	{
		PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_DEFAULT, "Brick Builder");

		ID3D12DescriptorHeap* ppDescriptorHeaps[] = { g_D3DGraphicsContext->GetSRVHeap()->GetHeap() };
		m_CommandList->SetDescriptorHeaps(_countof(ppDescriptorHeaps), ppDescriptorHeaps);

		// Copy the initial counter value into the counter
		m_CounterResource.SetValue(m_CommandList.Get(), counterUpload.GetResource());

		{
			D3D12_RESOURCE_BARRIER barriers[] =
			{
				CD3DX12_RESOURCE_BARRIER::Transition(object->GetAABBBuffer(SDFObject::RESOURCES_WRITE), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
				CD3DX12_RESOURCE_BARRIER::Transition(object->GetBrickBuffer(SDFObject::RESOURCES_WRITE), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			};
			m_CommandList->ResourceBarrier(ARRAYSIZE(barriers), barriers);
		}

		// Setup pipeline
		m_BrickBuilderPipeline->Bind(m_CommandList.Get());

		// Set resources
		m_CommandList->SetComputeRoot32BitConstants(AABBBuilderComputeRootSignature::BuildParameterSlot, SizeOfInUint32(buildParamsBuffer), &buildParamsBuffer, 0);
		m_CommandList->SetComputeRootShaderResourceView(AABBBuilderComputeRootSignature::EditListSlot, editList.GetEditBufferAddress());
		m_CommandList->SetComputeRootDescriptorTable(AABBBuilderComputeRootSignature::CounterResourceSlot, m_CounterResourceUAV.GetGPUHandle());
		m_CommandList->SetComputeRootDescriptorTable(AABBBuilderComputeRootSignature::AABBBufferSlot, object->GetAABBBufferUAV(SDFObject::RESOURCES_WRITE));
		m_CommandList->SetComputeRootDescriptorTable(AABBBuilderComputeRootSignature::BrickBufferSlot, object->GetBrickBufferUAV(SDFObject::RESOURCES_WRITE));

		// Calculate number of thread groups

		const UINT threadGroupX = (buildParamsBuffer.EvalSpace_BricksPerAxis.x + AABB_BUILD_NUM_THREADS_PER_GROUP - 1) / AABB_BUILD_NUM_THREADS_PER_GROUP;
		const UINT threadGroupY = (buildParamsBuffer.EvalSpace_BricksPerAxis.y + AABB_BUILD_NUM_THREADS_PER_GROUP - 1) / AABB_BUILD_NUM_THREADS_PER_GROUP;
		const UINT threadGroupZ = (buildParamsBuffer.EvalSpace_BricksPerAxis.z + AABB_BUILD_NUM_THREADS_PER_GROUP - 1) / AABB_BUILD_NUM_THREADS_PER_GROUP;

		// Dispatch
		LOG_TRACE("Building bricks with {}x{}x{} thread groups...", threadGroupX, threadGroupY, threadGroupZ);
		m_CommandList->Dispatch(threadGroupX, threadGroupY, threadGroupZ);

		// Copy counter value to a readback resource
		m_CounterResource.ReadValue(m_CommandList.Get(), counterReadback.GetResource());
	}

	// Step 3: Execute command list and wait until completion
	{
		PIXEndEvent(m_CommandList.Get());

		THROW_IF_FAIL(m_CommandList->Close());
		ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
		const auto fenceValue = computeQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		// CPU wait until this work has been complete before continuing
		computeQueue->WaitForFenceCPUBlocking(fenceValue);

		THROW_IF_FAIL(m_CommandAllocator->Reset());
		THROW_IF_FAIL(m_CommandList->Reset(m_CommandAllocator.Get(), nullptr));
	}

	// Step 4: Read counter value
	{
		// It is only save to read the counter value after the GPU has finished its work
		const UINT brickCount = counterReadback.ReadElement(0);
		object->AllocateOptimalBrickPool(brickCount, SDFObject::RESOURCES_WRITE);

		// Update build data required for the next stage
		buildParamsBuffer.BrickPool_BrickCapacityPerAxis = object->GetBrickPoolDimensions(SDFObject::RESOURCES_WRITE);
	}

	// Step 5: Build command list to execute SDF baker compute shader
	{
		PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_DEFAULT, "Brick Evaluator");

		ID3D12DescriptorHeap* ppDescriptorHeaps[] = { g_D3DGraphicsContext->GetSRVHeap()->GetHeap() };
		m_CommandList->SetDescriptorHeaps(_countof(ppDescriptorHeaps), ppDescriptorHeaps);

		// Scene texture must be in unordered access state
		{
			D3D12_RESOURCE_BARRIER barriers[] =
			{
				CD3DX12_RESOURCE_BARRIER::Transition(object->GetBrickPool(SDFObject::RESOURCES_WRITE), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
				CD3DX12_RESOURCE_BARRIER::Transition(object->GetAABBBuffer(SDFObject::RESOURCES_WRITE), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
				CD3DX12_RESOURCE_BARRIER::Transition(object->GetBrickBuffer(SDFObject::RESOURCES_WRITE), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
			};
			m_CommandList->ResourceBarrier(ARRAYSIZE(barriers), barriers);
		}

		// Set pipeline state
		m_BrickEvaluatorPipeline->Bind(m_CommandList.Get());

		// Set resource views
		m_CommandList->SetComputeRoot32BitConstants(BrickBuildComputeRootSignature::BuildParameterSlot, SizeOfInUint32(buildParamsBuffer), &buildParamsBuffer, 0);
		m_CommandList->SetComputeRootShaderResourceView(BrickBuildComputeRootSignature::EditListSlot, editList.GetEditBufferAddress());
		m_CommandList->SetComputeRootShaderResourceView(BrickBuildComputeRootSignature::BrickBufferSlot, object->GetBrickBufferAddress(SDFObject::RESOURCES_WRITE));
		m_CommandList->SetComputeRootDescriptorTable(BrickBuildComputeRootSignature::BrickPoolSlot, object->GetBrickPoolUAV(SDFObject::RESOURCES_WRITE));

		// Execute one group for each AABB
		const UINT threadGroupX = object->GetBrickCount(SDFObject::RESOURCES_WRITE);

		// Dispatch
		LOG_TRACE("Evaluating bricks with {} thread groups...", threadGroupX);
		m_CommandList->Dispatch(threadGroupX, 1, 1);

		// Brick pool resource will now be read from in the subsequent stages
		{
			auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(object->GetBrickPool(SDFObject::RESOURCES_WRITE), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			m_CommandList->ResourceBarrier(1, &barrier);
		}
	}

	// End all events before closing the command list
	PIXEndEvent(m_CommandList.Get());
	PIXEndEvent(m_CommandList.Get());

	// Step 6: Execute command list and wait until completion
	{
		THROW_IF_FAIL(m_CommandList->Close());
		ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
		m_PreviousBakeFence = computeQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	}
}

