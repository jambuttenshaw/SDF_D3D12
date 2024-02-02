#include "pch.h"
#include "SDFFactory.h"

#include "Renderer/D3DGraphicsContext.h"
#include "Renderer/Buffer/ReadbackBuffer.h"

#include "Renderer/Hlsl/ComputeHlslCompat.h"

#include "SDF/SDFObject.h"
#include "SDF/SDFEditList.h"

#include "pix3.h"


SDFFactory::SDFFactory()
	: m_CounterResource(s_MaxPipelinedObjects)
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
}


void SDFFactory::BakeSDFSync(SDFObject* object, SDFEditList&& editList)
{
	// Check object is not being constructed anywhere else
	// It's okay if the resource is in the switching state - we're going to wait on the render queue anyway
	const auto state = object->GetResourcesState(SDFObject::RESOURCES_WRITE);
	if (!(state == SDFObject::READY_COMPUTE || state == SDFObject::SWITCHING))
	{
		LOG_TRACE("Object in use by async bake - sync bake cannot be performed.");
		return;
	}

	LOG_TRACE("-----SDF Factory Synchronous Bake Begin--------");
	PIXBeginEvent(PIX_COLOR_INDEX(12), L"SDF Bake Synchronous");

	object->SetResourceState(SDFObject::RESOURCES_WRITE, SDFObject::COMPUTING);

	const auto directQueue = g_D3DGraphicsContext->GetDirectCommandQueue();
	const auto computeQueue = g_D3DGraphicsContext->GetComputeCommandQueue();

	// Make sure the last bake to have occurred has completed
	PIXBeginEvent(PIX_COLOR_INDEX(14), L"Wait for previous bake");
	computeQueue->WaitForFenceCPUBlocking(m_PreviousBakeFence);
	PIXEndEvent();
	// Make the compute queue wait until the render queue has finished its work
	computeQueue->InsertWaitForQueue(directQueue);

	PerformSDFBake_CPUBlocking(object, editList);

	// The rendering queue should wait until these operations have completed
	directQueue->InsertWaitForQueue(computeQueue);

	// The CPU can optionally wait until the operations have completed too
	PIXBeginEvent(PIX_COLOR_INDEX(13), L"Wait for bake completion");
	computeQueue->WaitForFenceCPUBlocking(m_PreviousBakeFence);
	PIXEndEvent();

	object->SetResourceState(SDFObject::RESOURCES_WRITE, SDFObject::COMPUTED);

	PIXEndEvent();
	LOG_TRACE("-----SDF Factory Synchronous Bake Complete-----");
}



void SDFFactory::InitializePipelines()
{
	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2);

		CD3DX12_ROOT_PARAMETER1 rootParameters[AABBBuilderComputeRootSignature::Count];
		rootParameters[AABBBuilderComputeRootSignature::BuildParameterSlot].InitAsConstants(SizeOfInUint32(SDFBuilderConstantBuffer), 0);
		rootParameters[AABBBuilderComputeRootSignature::EditListSlot].InitAsShaderResourceView(0);
		rootParameters[AABBBuilderComputeRootSignature::CounterResourceSlot].InitAsUnorderedAccessView(0);
		rootParameters[AABBBuilderComputeRootSignature::AABBBufferSlot].InitAsDescriptorTable(1, &ranges[0]);
		rootParameters[AABBBuilderComputeRootSignature::BrickBufferSlot].InitAsDescriptorTable(1, &ranges[1]);

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

	PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(15), "SDF Bake");

	// Step 1: Setup constant buffer data and counter temporary resources
	SDFBuilderConstantBuffer buildParamsBuffer;
	UploadBuffer<UINT32> counterUpload;
	ReadbackBuffer<UINT32> counterReadback;
	{
		PIXBeginEvent(PIX_COLOR_INDEX(15), L"Bake setup");
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
		counterUpload.Allocate(device, m_CounterResource.GetNumCounters(), 0, L"Counter upload");
		counterReadback.Allocate(device, m_CounterResource.GetNumCounters(), 0, L"Counter Readback");
		// Copy 0 into the counter
		counterUpload.CopyElement(0, 0);
		PIXEndEvent();
	}

	// Step 2: Build command list to execute AABB builder compute shader
	{
		PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(8), "Brick Builder");

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
		m_CommandList->SetComputeRootUnorderedAccessView(AABBBuilderComputeRootSignature::CounterResourceSlot, m_CounterResource.GetAddress());
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
		PIXEndEvent(m_CommandList.Get());
	}

	// Step 3: Execute command list and wait until completion
	{
		THROW_IF_FAIL(m_CommandList->Close());
		ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
		const auto fenceValue = computeQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		// CPU wait until this work has been complete before continuing
		PIXBeginEvent(PIX_COLOR_INDEX(17), L"Wait for first dispatch");
		computeQueue->WaitForFenceCPUBlocking(fenceValue);
		PIXEndEvent();

		THROW_IF_FAIL(m_CommandAllocator->Reset());
		THROW_IF_FAIL(m_CommandList->Reset(m_CommandAllocator.Get(), nullptr));
	}

	// Step 4: Read counter value
	{
		PIXBeginEvent(PIX_COLOR_INDEX(18), L"Allocate pool");

		// It is only save to read the counter value after the GPU has finished its work
		const UINT brickCount = counterReadback.ReadElement(0);
		object->AllocateOptimalBrickPool(brickCount, SDFObject::RESOURCES_WRITE);

		// Update build data required for the next stage
		buildParamsBuffer.BrickPool_BrickCapacityPerAxis = object->GetBrickPoolDimensions(SDFObject::RESOURCES_WRITE);
		PIXEndEvent();
	}

	// Step 5: Build command list to execute SDF baker compute shader
	{
		PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(22), "Brick Evaluator");

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
		PIXEndEvent(m_CommandList.Get());
	}

	// End all events before closing the command list
	PIXEndEvent(m_CommandList.Get());

	// Step 6: Execute command list and wait until completion
	{
		THROW_IF_FAIL(m_CommandList->Close());
		ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
		m_PreviousBakeFence = computeQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	}
}


void SDFFactory::PerformPipelinedSDFBake_CPUBlocking(UINT count, SDFObject** ppObjects, SDFEditList** ppEditLists)
{
	// argument validation
	ASSERT(count < s_MaxPipelinedObjects, "Count is larger than max pipelined objects!");

	const auto device = g_D3DGraphicsContext->GetDevice();
	const auto computeQueue = g_D3DGraphicsContext->GetComputeCommandQueue();

	THROW_IF_FAIL(m_CommandAllocator->Reset());
	THROW_IF_FAIL(m_CommandList->Reset(m_CommandAllocator.Get(), nullptr));

	PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(15), "SDF Bake Pipelined");

	// Step 1: Setup constant buffer data and counter temporary resources
	std::vector<SDFBuilderConstantBuffer> buildParamsBuffers(count);
	UploadBuffer<UINT32> counterUpload;
	ReadbackBuffer<UINT32> counterReadback;
	{
		PIXBeginEvent(PIX_COLOR_INDEX(15), L"Bake setup");
		for (UINT i = 0; i < count; i++)
		{
			auto& buildParamsBuffer = buildParamsBuffers.at(i);

			// Copy edit list
			ppEditLists[i]->CopyStagingToGPU();

			// Populate build params
			buildParamsBuffer.EvalSpace_MinBoundary = { -1.0f, -1.0f, -1.0f, 0.0f };
			buildParamsBuffer.EvalSpace_MaxBoundary = { 1.0f,  1.0f,  1.0f, 0.0f };
			buildParamsBuffer.EvalSpace_BrickSize = ppObjects[i]->GetBrickSize();

			const auto bricksPerAxis = (buildParamsBuffer.EvalSpace_MaxBoundary - buildParamsBuffer.EvalSpace_MinBoundary) / buildParamsBuffer.EvalSpace_BrickSize;
			XMStoreUInt3(&buildParamsBuffer.EvalSpace_BricksPerAxis, bricksPerAxis);

			// Make sure the object is large enough to store this many bricks
			const UINT requiredCapacity = buildParamsBuffer.EvalSpace_BricksPerAxis.x * buildParamsBuffer.EvalSpace_BricksPerAxis.y * buildParamsBuffer.EvalSpace_BricksPerAxis.z;
			ASSERT(requiredCapacity <= ppObjects[i]->GetBrickBufferCapacity(), "Object does not have large enough brick capacity and buffer overflow is likely!");

			// Brick capacity has not been determined until brick builder has executed
			buildParamsBuffer.BrickPool_BrickCapacityPerAxis = XMUINT3{ 0, 0, 0 };

			// Helpful values that can be calculated once on the CPU
			XMFLOAT3 evalSpaceRange;
			XMStoreFloat3(&evalSpaceRange, buildParamsBuffer.EvalSpace_MaxBoundary - buildParamsBuffer.EvalSpace_MinBoundary);
			buildParamsBuffer.EvalSpace_VoxelsPerUnit = static_cast<float>(buildParamsBuffer.EvalSpace_BricksPerAxis.x * SDF_BRICK_SIZE_VOXELS) / evalSpaceRange.x;

			buildParamsBuffer.SDFEditCount = ppEditLists[i]->GetEditCount();
		}

		// Setup counter temporary resources
		counterUpload.Allocate(device, m_CounterResource.GetNumCounters(), 0, L"Counter upload");
		counterReadback.Allocate(device, m_CounterResource.GetNumCounters(), 0, L"Counter Readback");
		// Copy 0 into all counters
		counterUpload.SetElements(0, count, 0);
		PIXEndEvent();
	}

	// Step 2: Build command list to execute AABB builder compute shader
	{
		PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(8), "Brick Builder");

		ID3D12DescriptorHeap* ppDescriptorHeaps[] = { g_D3DGraphicsContext->GetSRVHeap()->GetHeap() };
		m_CommandList->SetDescriptorHeaps(_countof(ppDescriptorHeaps), ppDescriptorHeaps);

		// Copy the initial counter value into the counter
		m_CounterResource.SetValue(m_CommandList.Get(), counterUpload.GetResource());

		for (UINT i = 0; i < count; i++)
		{
			{
				D3D12_RESOURCE_BARRIER barriers[] =
				{
					CD3DX12_RESOURCE_BARRIER::Transition(ppObjects[i]->GetAABBBuffer(SDFObject::RESOURCES_WRITE), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
					CD3DX12_RESOURCE_BARRIER::Transition(ppObjects[i]->GetBrickBuffer(SDFObject::RESOURCES_WRITE), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				};
				m_CommandList->ResourceBarrier(ARRAYSIZE(barriers), barriers);
			}

			// Setup pipeline
			m_BrickBuilderPipeline->Bind(m_CommandList.Get());

			// Set resources
			auto& buildParamsBuffer = buildParamsBuffers.at(i);
			m_CommandList->SetComputeRoot32BitConstants(AABBBuilderComputeRootSignature::BuildParameterSlot, SizeOfInUint32(buildParamsBuffer), &buildParamsBuffer, 0);
			m_CommandList->SetComputeRootShaderResourceView(AABBBuilderComputeRootSignature::EditListSlot, ppEditLists[i]->GetEditBufferAddress());
			m_CommandList->SetComputeRootUnorderedAccessView(AABBBuilderComputeRootSignature::CounterResourceSlot, m_CounterResource.GetAddress(i));
			m_CommandList->SetComputeRootDescriptorTable(AABBBuilderComputeRootSignature::AABBBufferSlot, ppObjects[i]->GetAABBBufferUAV(SDFObject::RESOURCES_WRITE));
			m_CommandList->SetComputeRootDescriptorTable(AABBBuilderComputeRootSignature::BrickBufferSlot, ppObjects[i]->GetBrickBufferUAV(SDFObject::RESOURCES_WRITE));

			// Calculate number of thread groups

			const UINT threadGroupX = (buildParamsBuffer.EvalSpace_BricksPerAxis.x + AABB_BUILD_NUM_THREADS_PER_GROUP - 1) / AABB_BUILD_NUM_THREADS_PER_GROUP;
			const UINT threadGroupY = (buildParamsBuffer.EvalSpace_BricksPerAxis.y + AABB_BUILD_NUM_THREADS_PER_GROUP - 1) / AABB_BUILD_NUM_THREADS_PER_GROUP;
			const UINT threadGroupZ = (buildParamsBuffer.EvalSpace_BricksPerAxis.z + AABB_BUILD_NUM_THREADS_PER_GROUP - 1) / AABB_BUILD_NUM_THREADS_PER_GROUP;

			// Dispatch
			m_CommandList->Dispatch(threadGroupX, threadGroupY, threadGroupZ);
		}

		// Copy counter value to a readback resource
		m_CounterResource.ReadValue(m_CommandList.Get(), counterReadback.GetResource());
		PIXEndEvent(m_CommandList.Get());
	}

	// Step 3: Execute command list and wait until completion
	{
		THROW_IF_FAIL(m_CommandList->Close());
		ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
		const auto fenceValue = computeQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		// CPU wait until this work has been complete before continuing
		PIXBeginEvent(PIX_COLOR_INDEX(17), L"Wait for first dispatch");
		computeQueue->WaitForFenceCPUBlocking(fenceValue);
		PIXEndEvent();

		THROW_IF_FAIL(m_CommandAllocator->Reset());
		THROW_IF_FAIL(m_CommandList->Reset(m_CommandAllocator.Get(), nullptr));
	}

	// Step 4: Read counter value
	{
		PIXBeginEvent(PIX_COLOR_INDEX(18), L"Allocate pool");

		for (UINT i = 0; i < count; i++)
		{
			// It is only save to read the counter value after the GPU has finished its work
			const UINT brickCount = counterReadback.ReadElement(i);
			ppObjects[i]->AllocateOptimalBrickPool(brickCount, SDFObject::RESOURCES_WRITE);

			// Update build data required for the next stage
			buildParamsBuffers[i].BrickPool_BrickCapacityPerAxis = ppObjects[i]->GetBrickPoolDimensions(SDFObject::RESOURCES_WRITE);
		}
		PIXEndEvent();
	}

	// Step 5: Build command list to execute SDF baker compute shader
	{
		PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(22), "Brick Evaluator");

		ID3D12DescriptorHeap* ppDescriptorHeaps[] = { g_D3DGraphicsContext->GetSRVHeap()->GetHeap() };
		m_CommandList->SetDescriptorHeaps(_countof(ppDescriptorHeaps), ppDescriptorHeaps);

		for (UINT i = 0; i < count; i++)
		{
			// Scene texture must be in unordered access state
			{
				D3D12_RESOURCE_BARRIER barriers[] =
				{
					CD3DX12_RESOURCE_BARRIER::Transition(ppObjects[i]->GetBrickPool(SDFObject::RESOURCES_WRITE), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
					CD3DX12_RESOURCE_BARRIER::Transition(ppObjects[i]->GetAABBBuffer(SDFObject::RESOURCES_WRITE), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
					CD3DX12_RESOURCE_BARRIER::Transition(ppObjects[i]->GetBrickBuffer(SDFObject::RESOURCES_WRITE), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
				};
				m_CommandList->ResourceBarrier(ARRAYSIZE(barriers), barriers);
			}

			// Set pipeline state
			m_BrickEvaluatorPipeline->Bind(m_CommandList.Get());

			// Set resource views
			auto& buildParamsBuffer = buildParamsBuffers.at(i);
			m_CommandList->SetComputeRoot32BitConstants(BrickBuildComputeRootSignature::BuildParameterSlot, SizeOfInUint32(buildParamsBuffer), &buildParamsBuffer, 0);
			m_CommandList->SetComputeRootShaderResourceView(BrickBuildComputeRootSignature::EditListSlot, ppEditLists[i]->GetEditBufferAddress());
			m_CommandList->SetComputeRootShaderResourceView(BrickBuildComputeRootSignature::BrickBufferSlot, ppObjects[i]->GetBrickBufferAddress(SDFObject::RESOURCES_WRITE));
			m_CommandList->SetComputeRootDescriptorTable(BrickBuildComputeRootSignature::BrickPoolSlot, ppObjects[i]->GetBrickPoolUAV(SDFObject::RESOURCES_WRITE));

			// Execute one group for each AABB
			const UINT threadGroupX = ppObjects[i]->GetBrickCount(SDFObject::RESOURCES_WRITE);

			// Dispatch
			LOG_TRACE("Evaluating bricks with {} thread groups...", threadGroupX);
			m_CommandList->Dispatch(threadGroupX, 1, 1);

			// Brick pool resource will now be read from in the subsequent stages
			{
				auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(ppObjects[i]->GetBrickPool(SDFObject::RESOURCES_WRITE), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				m_CommandList->ResourceBarrier(1, &barrier);
			}
		}
		PIXEndEvent(m_CommandList.Get());
	}

	// End all events before closing the command list
	PIXEndEvent(m_CommandList.Get());

	// Step 6: Execute command list and wait until completion
	{
		THROW_IF_FAIL(m_CommandList->Close());
		ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
		m_PreviousBakeFence = computeQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	}
}




void SDFFactory::LogBuildParameters(const SDFBuilderConstantBuffer& buildParams)
{
	XMFLOAT3 tempFloat3;

	LOG_INFO("----- Build Params -----");

	XMStoreFloat3(&tempFloat3, buildParams.EvalSpace_MinBoundary);
	LOG_INFO("Min Boundary: {} {} {}", tempFloat3.x, tempFloat3.y, tempFloat3.z);

	XMStoreFloat3(&tempFloat3, buildParams.EvalSpace_MaxBoundary);
	LOG_INFO("Max Boundary: {} {} {}", tempFloat3.x, tempFloat3.y, tempFloat3.z);

	LOG_INFO("Bricks Per Axis: {} {} {}", buildParams.EvalSpace_BricksPerAxis.x, buildParams.EvalSpace_BricksPerAxis.y, buildParams.EvalSpace_BricksPerAxis.z);

	LOG_INFO("Brick Size: {}", buildParams.EvalSpace_BrickSize);

	LOG_INFO("Brick Pool Capacity: {} {} {}", buildParams.BrickPool_BrickCapacityPerAxis.x, buildParams.BrickPool_BrickCapacityPerAxis.y, buildParams.BrickPool_BrickCapacityPerAxis.z);

	LOG_INFO("Voxels Per Unit: {}", buildParams.EvalSpace_VoxelsPerUnit);

	LOG_INFO("Edit Count: {}", buildParams.SDFEditCount);

	LOG_INFO("------------------------");
}
