#include "pch.h"
#include "SDFFactoryHierarchical.h"

#include "Renderer/D3DGraphicsContext.h"

#include "Renderer/Buffer/CounterResource.h"
#include "Renderer/Buffer/ReadbackBuffer.h"
#include "Renderer/Buffer/StructuredBuffer.h"

#include "Renderer/Hlsl/ComputeHlslCompat.h"

#include "SDF/SDFEditList.h"
#include "SDF/SDFObject.h"


#include <pix3.h>


namespace BrickCounterSignature
{
	enum Value
	{
		BuildParameterSlot = 0,
		BrickCounterSlot,
		EditListSlot,
		BricksSlot,
		CountTableSlot,
		PrefixSumGroupCounterSlot,
		Count
	};
}

namespace BrickScanSignature
{
	enum Value
	{
		CountTableSlot = 0,
		NumberOfCountsSlot,
		BlockSumTableSlot,
		ScannedBlockTableSlot,
		PrefixSumTable,
		Count
	};
}

namespace BrickBuilderSignature
{
	enum Value
	{
		BuildParameterSlot = 0,
		InBrickCounterSlot,
		InBricksSlot,
		PrefixSumTableSlot,
		OutBrickCounterSlot,
		OutBricksSlot,
		Count
	};
}

namespace AABBBuilderSignature
{
	enum Value
	{
		BuildParameterSlot = 0,
		BricksSlot,
		AABBsSlot,
		BrickBufferSlot,
		Count
	};
}

namespace BrickEvaluatorSignature
{
	enum Value
	{
		BuildParameterSlot = 0,
		EditListSlot,
		BrickBufferSlot,
		BrickPoolSlot,
		Count
	};
}



SDFFactoryHierarchical::SDFFactoryHierarchical()
{
	const auto device = g_D3DGraphicsContext->GetDevice();

	// Create command allocator and list
	THROW_IF_FAIL(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&m_CommandAllocator)));
	m_CommandAllocator->SetName(L"Hierarchical Factory Command Allocator");
	THROW_IF_FAIL(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, m_CommandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_CommandList)));
	m_CommandList->SetName(L"Hierarchical Factory Command List");

	THROW_IF_FAIL(m_CommandList->Close());

	// Create indirect execution objects
	{
		// Describe indirect arguments
		D3D12_INDIRECT_ARGUMENT_DESC indirectArg;
		indirectArg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;

		// Create command signature
		D3D12_COMMAND_SIGNATURE_DESC signatureDesc = {};
		signatureDesc.ByteStride = sizeof(IndirectCommand);
		signatureDesc.NumArgumentDescs = 1;
		signatureDesc.pArgumentDescs = &indirectArg;
		signatureDesc.NodeMask = 0;

		// The command signature only contains dispatches, so no root signature is required
		THROW_IF_FAIL(device->CreateCommandSignature(&signatureDesc, nullptr, IID_PPV_ARGS(&m_CommandSignature)));
	}

	// Create command buffer
	{
		// The buffer will contain 2 commands
		m_CommandBuffer.Allocate(device, 2 * sizeof(IndirectCommand), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_NONE, L"SDF Factory Command Buffer");

		// Upload the commands into the command buffer
		m_CommandUploadBuffer.Allocate(device, 2, 0, L"Command buffer upload");

		// The default commands are to only dispatch 1 group
		IndirectCommand commands[2];
		commands[0].dispatchArgs.ThreadGroupCountX = 1;
		commands[0].dispatchArgs.ThreadGroupCountY = 1;
		commands[0].dispatchArgs.ThreadGroupCountZ = 1;

		commands[1].dispatchArgs.ThreadGroupCountX = 1;
		commands[1].dispatchArgs.ThreadGroupCountY = 1;
		commands[1].dispatchArgs.ThreadGroupCountZ = 1;
		m_CommandUploadBuffer.CopyElements(0, 2, commands);

		// The upload buffer will be copied into the command buffer on bake init
	}

	InitializePipelines();
}


void SDFFactoryHierarchical::BakeSDFSync(SDFObject* object, SDFEditList&& editList)
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
	computeQueue->WaitForFenceCPUBlocking(m_PreviousWorkFence);
	PIXEndEvent();
	// Make the compute queue wait until the render queue has finished its work
	computeQueue->InsertWaitForQueue(directQueue);

	PerformSDFBake_CPUBlocking(object, editList);

	// The rendering queue should wait until these operations have completed
	directQueue->InsertWaitForQueue(computeQueue);

	// The CPU can optionally wait until the operations have completed too
	PIXBeginEvent(PIX_COLOR_INDEX(13), L"Wait for bake completion");
	computeQueue->WaitForFenceCPUBlocking(m_PreviousWorkFence);
	PIXEndEvent();

	object->SetResourceState(SDFObject::RESOURCES_WRITE, SDFObject::COMPUTED);

	PIXEndEvent();
	LOG_TRACE("-----SDF Factory Synchronous Bake Complete-----");
}


void SDFFactoryHierarchical::InitializePipelines()
{
	{
		using namespace BrickCounterSignature;

		CD3DX12_ROOT_PARAMETER1 rootParams[Count];
		rootParams[BuildParameterSlot].InitAsConstants(SizeOfInUint32(BrickBuildParametersConstantBuffer), 0);
		rootParams[BrickCounterSlot].InitAsShaderResourceView(0);
		rootParams[EditListSlot].InitAsShaderResourceView(1);
		rootParams[BricksSlot].InitAsUnorderedAccessView(0);
		rootParams[CountTableSlot].InitAsUnorderedAccessView(1);
		rootParams[PrefixSumGroupCounterSlot].InitAsUnorderedAccessView(2);

		D3DComputePipelineDesc desc = {};
		desc.NumRootParameters = ARRAYSIZE(rootParams);
		desc.RootParameters = rootParams;
		desc.Shader = L"assets/shaders/compute/sub_brick_counter.hlsl";
		desc.EntryPoint = L"main";
		desc.Defines = nullptr;

		m_BrickCounterPipeline = std::make_unique<D3DComputePipeline>(&desc);
	}

	{
		using namespace BrickScanSignature;

		CD3DX12_ROOT_PARAMETER1 rootParams[Count];
		rootParams[CountTableSlot].InitAsShaderResourceView(0);
		rootParams[NumberOfCountsSlot].InitAsShaderResourceView(1);
		rootParams[BlockSumTableSlot].InitAsUnorderedAccessView(0);
		rootParams[ScannedBlockTableSlot].InitAsUnorderedAccessView(1);
		rootParams[PrefixSumTable].InitAsUnorderedAccessView(2);

		D3DComputePipelineDesc desc = {};
		desc.NumRootParameters = ARRAYSIZE(rootParams);
		desc.RootParameters = rootParams;
		desc.Shader = L"assets/shaders/compute/prefix_sum.hlsl";
		desc.Defines = nullptr;

		desc.EntryPoint = L"scan_blocks";
		m_ScanBlocksPipeline = std::make_unique<D3DComputePipeline>(&desc);

		desc.EntryPoint = L"scan_block_sums";
		m_ScanBlockSumsPipeline = std::make_unique<D3DComputePipeline>(&desc);

		desc.EntryPoint = L"sum_scans";
		m_SumScansPipeline = std::make_unique<D3DComputePipeline>(&desc);
	}

	{
		using namespace BrickBuilderSignature;

		CD3DX12_ROOT_PARAMETER1 rootParams[Count];
		rootParams[BuildParameterSlot].InitAsConstants(SizeOfInUint32(BrickBuildParametersConstantBuffer), 0);
		rootParams[InBrickCounterSlot].InitAsShaderResourceView(0);
		rootParams[InBricksSlot].InitAsShaderResourceView(1);
		rootParams[PrefixSumTableSlot].InitAsShaderResourceView(2);
		rootParams[OutBrickCounterSlot].InitAsUnorderedAccessView(0);
		rootParams[OutBricksSlot].InitAsUnorderedAccessView(1);

		D3DComputePipelineDesc desc = {};
		desc.NumRootParameters = ARRAYSIZE(rootParams);
		desc.RootParameters = rootParams;
		desc.Shader = L"assets/shaders/compute/sub_brick_builder.hlsl";
		desc.EntryPoint = L"main";
		desc.Defines = nullptr;

		m_BrickBuilderPipeline = std::make_unique<D3DComputePipeline>(&desc);
	}

	{
		using namespace AABBBuilderSignature;

		CD3DX12_ROOT_PARAMETER1 rootParams[Count];
		rootParams[BuildParameterSlot].InitAsConstants(SizeOfInUint32(AABBBuilderConstantBuffer), 0);
		rootParams[BricksSlot].InitAsShaderResourceView(0);
		rootParams[AABBsSlot].InitAsUnorderedAccessView(0);
		rootParams[BrickBufferSlot].InitAsUnorderedAccessView(1);

		D3DComputePipelineDesc desc = {};
		desc.NumRootParameters = ARRAYSIZE(rootParams);
		desc.RootParameters = rootParams;
		desc.Shader = L"assets/shaders/compute/aabb_builder.hlsl";
		desc.EntryPoint = L"main";
		desc.Defines = nullptr;

		m_AABBBuilderPipeline = std::make_unique<D3DComputePipeline>(&desc);
	}

	{
		using namespace BrickEvaluatorSignature;

		CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);

		CD3DX12_ROOT_PARAMETER1 rootParameters[Count];
		rootParameters[BuildParameterSlot].InitAsConstants(SizeOfInUint32(BrickEvaluationConstantBuffer), 0);
		rootParameters[EditListSlot].InitAsShaderResourceView(0);
		rootParameters[BrickBufferSlot].InitAsShaderResourceView(1);
		rootParameters[BrickPoolSlot].InitAsDescriptorTable(1, &ranges[0]);

		D3DComputePipelineDesc desc;
		desc.NumRootParameters = ARRAYSIZE(rootParameters);
		desc.RootParameters = rootParameters;
		desc.Shader = L"assets/shaders/compute/brick_evaluator.hlsl";
		desc.EntryPoint = L"main";
		desc.Defines = nullptr;

		m_BrickEvaluatorPipeline = std::make_unique<D3DComputePipeline>(&desc);
	}
}


void SDFFactoryHierarchical::PerformSDFBake_CPUBlocking(SDFObject* object, const SDFEditList& editList)
{
	const auto device = g_D3DGraphicsContext->GetDevice();
	const auto computeQueue = g_D3DGraphicsContext->GetComputeCommandQueue();

	THROW_IF_FAIL(m_CommandAllocator->Reset());
	THROW_IF_FAIL(m_CommandList->Reset(m_CommandAllocator.Get(), nullptr));

	PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(40), L"SDF Bake Hierarchical");

	// Resources used for building
	BrickBuildParametersConstantBuffer buildParamsCB;
	AABBBuilderConstantBuffer aabbCB;
	BrickEvaluationConstantBuffer brickEvalCB;

	// Ping-pong buffers to contain the bricks
	UINT currentBuffers = 0;
	std::array<StructuredBuffer<Brick>, 2> brickBuffers;
	// An upload buffer for bricks is required to send the initial brick to the GPU
	UploadBuffer<Brick> brickUpload;

	// Two counters are required for ping-pong buffers
	std::array<CounterResource, 2> counters;
	UploadBuffer<UINT32> counterUploadZero;	// Used to set a counter to 0
	UploadBuffer<UINT32> counterUploadOne;	// Used to set a counter to 1
	ReadbackBuffer<UINT32> counterReadback;

	// Buffers for calculating prefix sums
	DefaultBuffer subBrickCountBuffer, blockPrefixSumsBuffer, scannedBlockPrefixSumsBuffer, prefixSumsBuffer;
	CounterResource blockCounter; // How many block scan groups to dispatch
	
	{
		// Step 1: Set up resources

		// Copy edit list into GPU memory
		editList.CopyStagingToGPU();

		// Populate initial build params
		buildParamsCB.SDFEditCount = editList.GetEditCount();
		// The brick size will be different for each dispatch
		buildParamsCB.BrickSize = 6.4f; // size of entire evaluation space
		buildParamsCB.SubBrickSize = buildParamsCB.BrickSize / 4.0f; // brick size will quarter with each dispatch


		// Allocate brick buffers
		for (auto& buffer : brickBuffers)
		{
			buffer.Allocate(device, object->GetBrickBufferCapacity(), D3D12_RESOURCE_STATE_COMMON, true, L"Brick buffer");
		}
		brickUpload.Allocate(device, 1, 0, L"Brick upload");

		constexpr Brick initialBrick = {
			{0, 0},
			{ -3.2f, -3.2f, -3.2f }
		};
		brickUpload.CopyElement(0, initialBrick);

		// Allocate counters
		for (auto& counter : counters)
		{
			counter.Allocate(device, L"SDF Factory Counters");
		}
		counterUploadZero.Allocate(device, 1, 0, L"Counters Upload Zero");
		counterUploadOne.Allocate(device, 1, 0, L"Counters Upload One");
		counterReadback.Allocate(device, 1, 0, L"Counters Readback");

		counterUploadZero.CopyElement(0, 0);
		counterUploadOne.CopyElement(0, 1);

		UINT64 width = object->GetBrickBufferCapacity() * sizeof(UINT);
		subBrickCountBuffer.Allocate(device, width, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"Sub brick count buffer");
		blockPrefixSumsBuffer.Allocate(device, width, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"Block prefix sums buffer");
		scannedBlockPrefixSumsBuffer.Allocate(device, width, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"Scanned block prefix sums buffer");
		prefixSumsBuffer.Allocate(device, width, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"prefix sums buffer");

		blockCounter.Allocate(device, L"Block Group Counter");
	}

	{
		// Step 2: Upload initial data into the buffers and transition resources into the required state

		PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(41), L"Data upload");

		{
			// Transition brick buffer for reading
			const D3D12_RESOURCE_BARRIER barriers[] = {
				CD3DX12_RESOURCE_BARRIER::Transition(brickBuffers.at(currentBuffers).GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST),
				CD3DX12_RESOURCE_BARRIER::Transition(m_CommandBuffer.GetResource(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_COPY_DEST)
			};
			m_CommandList->ResourceBarrier(ARRAYSIZE(barriers), barriers);
		}

		// Copy brick data into the brick buffer
		m_CommandList->CopyBufferRegion(brickBuffers.at(currentBuffers).GetResource(), 0, brickUpload.GetResource(), 0, sizeof(Brick));

		// Copy default command buffer into the processed command buffer
		m_CommandList->CopyBufferRegion(m_CommandBuffer.GetResource(), 0, m_CommandUploadBuffer.GetResource(), 0, 2 * sizeof(IndirectCommand));

		{
			// Transition brick buffers into unordered access
			const D3D12_RESOURCE_BARRIER barriers[] = {
				CD3DX12_RESOURCE_BARRIER::Transition(brickBuffers.at(currentBuffers).GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
				CD3DX12_RESOURCE_BARRIER::Transition(brickBuffers.at(1 - currentBuffers).GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
				CD3DX12_RESOURCE_BARRIER::Transition(m_CommandBuffer.GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)
			};
			m_CommandList->ResourceBarrier(ARRAYSIZE(barriers), barriers);
		}

		// Set initial counter values
		counters.at(currentBuffers).SetValue(m_CommandList.Get(), counterUploadOne.GetResource());
		counters.at(1 - currentBuffers).SetValue(m_CommandList.Get(), counterUploadZero.GetResource());

		blockCounter.SetValue(m_CommandList.Get(), counterUploadZero.GetResource());

		{
			// The counter will initially be read from so it should be in read only state
			const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(counters.at(currentBuffers).GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			m_CommandList->ResourceBarrier(1, &barrier);
		}

		{
			// Put prefix sum buffers into correct state
			const D3D12_RESOURCE_BARRIER barriers[] = {
				CD3DX12_RESOURCE_BARRIER::Transition(subBrickCountBuffer.GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
				CD3DX12_RESOURCE_BARRIER::Transition(blockPrefixSumsBuffer.GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
				CD3DX12_RESOURCE_BARRIER::Transition(scannedBlockPrefixSumsBuffer.GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
				CD3DX12_RESOURCE_BARRIER::Transition(prefixSumsBuffer.GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			};
			m_CommandList->ResourceBarrier(ARRAYSIZE(barriers), barriers);
		}

		PIXEndEvent(m_CommandList.Get());
	}


	{
		// Step 3: Populate command list to hierarchically build bricks

		PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(42), L"Hierarchical brick building");

		ID3D12DescriptorHeap* ppDescriptorHeaps[] = { g_D3DGraphicsContext->GetSRVHeap()->GetHeap() };
		m_CommandList->SetDescriptorHeaps(_countof(ppDescriptorHeaps), ppDescriptorHeaps);

		// Multiple iterations will be made until the brick size is small enough
		while(buildParamsCB.SubBrickSize >= object->GetMinBrickSize())
		{
			PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(45), L"Brick Building Iteration");

			PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(46), L"Brick Counting");
			// Step 3.1: Dispatch brick counter
			m_BrickCounterPipeline->Bind(m_CommandList.Get());

			// Set root parameters
			m_CommandList->SetComputeRoot32BitConstants(BrickCounterSignature::BuildParameterSlot, SizeOfInUint32(buildParamsCB), &buildParamsCB, 0);
			m_CommandList->SetComputeRootShaderResourceView(BrickCounterSignature::BrickCounterSlot, counters.at(currentBuffers).GetAddress());
			m_CommandList->SetComputeRootShaderResourceView(BrickCounterSignature::EditListSlot, editList.GetEditBufferAddress());
			m_CommandList->SetComputeRootUnorderedAccessView(BrickCounterSignature::BricksSlot, brickBuffers.at(currentBuffers).GetAddress());
			m_CommandList->SetComputeRootUnorderedAccessView(BrickCounterSignature::CountTableSlot, subBrickCountBuffer.GetAddress());
			m_CommandList->SetComputeRootUnorderedAccessView(BrickCounterSignature::PrefixSumGroupCounterSlot, blockCounter.GetAddress());

			// Indirectly dispatch compute shader
			// The number of groups to dispatch is contained in the processed command buffer
			// The contents of this buffer will be updated after each iteration to dispatch the correct number of groups
			m_CommandList->ExecuteIndirect(m_CommandSignature.Get(), 1, m_CommandBuffer.GetResource(), 0, nullptr, 0);


			{
				// Insert UAV barriers to make sure the first dispatch has finished writing to the brick buffer
				const D3D12_RESOURCE_BARRIER uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(brickBuffers.at(currentBuffers).GetResource());
				m_CommandList->ResourceBarrier(1, &uavBarrier);
			}

			// Insert transition barriers for next stage of the pipeline
			{
				D3D12_RESOURCE_BARRIER barriers[] = {
					// Transition brick buffers
					CD3DX12_RESOURCE_BARRIER::Transition(brickBuffers.at(currentBuffers).GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
					CD3DX12_RESOURCE_BARRIER::Transition(brickBuffers.at(1 - currentBuffers).GetResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
					CD3DX12_RESOURCE_BARRIER::Transition(subBrickCountBuffer.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
				};
				m_CommandList->ResourceBarrier(ARRAYSIZE(barriers), barriers);
			}

			// Copy block counter into second indirect argument
			{
				{
					D3D12_RESOURCE_BARRIER barriers[] = {
						CD3DX12_RESOURCE_BARRIER::Transition(m_CommandBuffer.GetResource(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_COPY_DEST),
						CD3DX12_RESOURCE_BARRIER::Transition(blockCounter.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE)
					};
					m_CommandList->ResourceBarrier(ARRAYSIZE(barriers), barriers);
				}

				// Copy the counter that was just written to into the groups X of the dispatch args
				m_CommandList->CopyBufferRegion(m_CommandBuffer.GetResource(), sizeof(IndirectCommand), blockCounter.GetResource(), 0, sizeof(UINT32));

				{
					D3D12_RESOURCE_BARRIER barriers[] = {
						CD3DX12_RESOURCE_BARRIER::Transition(m_CommandBuffer.GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT),
						CD3DX12_RESOURCE_BARRIER::Transition(blockCounter.GetResource(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
					};
					m_CommandList->ResourceBarrier(ARRAYSIZE(barriers), barriers);
				}
			}

			PIXEndEvent(m_CommandList.Get());
			PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(47), L"Calculate prefix sum");

			// Step 3.2: Calculate prefix sums. This is done with 3 dispatches
			{
				m_ScanBlocksPipeline->Bind(m_CommandList.Get());
				m_CommandList->SetComputeRootShaderResourceView(BrickScanSignature::CountTableSlot, subBrickCountBuffer.GetAddress());
				m_CommandList->SetComputeRootShaderResourceView(BrickScanSignature::NumberOfCountsSlot, counters.at(currentBuffers).GetAddress());
				m_CommandList->SetComputeRootUnorderedAccessView(BrickScanSignature::BlockSumTableSlot, blockPrefixSumsBuffer.GetAddress());
				m_CommandList->SetComputeRootUnorderedAccessView(BrickScanSignature::PrefixSumTable, prefixSumsBuffer.GetAddress());

				m_CommandList->ExecuteIndirect(m_CommandSignature.Get(), 1, m_CommandBuffer.GetResource(), sizeof(IndirectCommand), nullptr, 0);

				{
					const D3D12_RESOURCE_BARRIER uavBarriers[] = {
						CD3DX12_RESOURCE_BARRIER::UAV(blockPrefixSumsBuffer.GetResource()),
						CD3DX12_RESOURCE_BARRIER::UAV(prefixSumsBuffer.GetResource())
					};
					m_CommandList->ResourceBarrier(ARRAYSIZE(uavBarriers), uavBarriers);
				}

				m_ScanBlockSumsPipeline->Bind(m_CommandList.Get());
				m_CommandList->SetComputeRootShaderResourceView(BrickScanSignature::NumberOfCountsSlot, counters.at(currentBuffers).GetAddress());
				m_CommandList->SetComputeRootUnorderedAccessView(BrickScanSignature::BlockSumTableSlot, blockPrefixSumsBuffer.GetAddress());
				m_CommandList->SetComputeRootUnorderedAccessView(BrickScanSignature::ScannedBlockTableSlot, scannedBlockPrefixSumsBuffer.GetAddress());

				m_CommandList->ExecuteIndirect(m_CommandSignature.Get(), 1, m_CommandBuffer.GetResource(), sizeof(IndirectCommand), nullptr, 0);

				{
					const D3D12_RESOURCE_BARRIER uavBarriers[] = {
						CD3DX12_RESOURCE_BARRIER::UAV(scannedBlockPrefixSumsBuffer.GetResource()),
					};
					m_CommandList->ResourceBarrier(ARRAYSIZE(uavBarriers), uavBarriers);
				}

				m_SumScansPipeline->Bind(m_CommandList.Get());
				m_CommandList->SetComputeRootShaderResourceView(BrickScanSignature::NumberOfCountsSlot, counters.at(currentBuffers).GetAddress());
				m_CommandList->SetComputeRootUnorderedAccessView(BrickScanSignature::ScannedBlockTableSlot, scannedBlockPrefixSumsBuffer.GetAddress());
				m_CommandList->SetComputeRootUnorderedAccessView(BrickScanSignature::PrefixSumTable, prefixSumsBuffer.GetAddress());

				m_CommandList->ExecuteIndirect(m_CommandSignature.Get(), 1, m_CommandBuffer.GetResource(), sizeof(IndirectCommand), nullptr, 0);

				{
					const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(prefixSumsBuffer.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
					m_CommandList->ResourceBarrier(1, &barrier);
				}

				{
					const D3D12_RESOURCE_BARRIER uavBarriers[] = {
						CD3DX12_RESOURCE_BARRIER::UAV(prefixSumsBuffer.GetResource()),
					};
					m_CommandList->ResourceBarrier(ARRAYSIZE(uavBarriers), uavBarriers);
				}
			}

			PIXEndEvent(m_CommandList.Get());
			PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(48), L"Build sub-bricks");

			// Step 3.3: Dispatch brick builder
			// This step will make use of the ping-pong buffers to output the next collection of bricks to process
			m_BrickBuilderPipeline->Bind(m_CommandList.Get());

			// Set root parameters
			m_CommandList->SetComputeRoot32BitConstants(BrickBuilderSignature::BuildParameterSlot, SizeOfInUint32(buildParamsCB), &buildParamsCB, 0);
			m_CommandList->SetComputeRootShaderResourceView(BrickBuilderSignature::InBrickCounterSlot, counters.at(currentBuffers).GetAddress());
			m_CommandList->SetComputeRootShaderResourceView(BrickBuilderSignature::InBricksSlot, brickBuffers.at(currentBuffers).GetAddress());
			m_CommandList->SetComputeRootShaderResourceView(BrickBuilderSignature::PrefixSumTableSlot, prefixSumsBuffer.GetAddress());
			m_CommandList->SetComputeRootUnorderedAccessView(BrickBuilderSignature::OutBrickCounterSlot, counters.at(1 - currentBuffers).GetAddress());
			m_CommandList->SetComputeRootUnorderedAccessView(BrickBuilderSignature::OutBricksSlot, brickBuffers.at(1 - currentBuffers).GetAddress());

			// Indirectly dispatch compute shader
			// The number of groups to dispatch is contained in the processed command buffer
			// The contents of this buffer will be updated after each iteration to dispatch the correct number of groups
			m_CommandList->ExecuteIndirect(m_CommandSignature.Get(), 1, m_CommandBuffer.GetResource(), 0, nullptr, 0);

			PIXEndEvent(m_CommandList.Get());
			PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(48), L"Prepare for next iteration");

			{
				// Insert UAV barriers to make sure the first dispatch has finished writing to the brick buffer
				const D3D12_RESOURCE_BARRIER uavBarriers[] = {
					CD3DX12_RESOURCE_BARRIER::UAV(brickBuffers.at(1 - currentBuffers).GetResource()),
					CD3DX12_RESOURCE_BARRIER::UAV(counters.at(1 - currentBuffers).GetResource())
				};
				m_CommandList->ResourceBarrier(ARRAYSIZE(uavBarriers), uavBarriers);
			}

			// Insert transition barriers for next stage of the pipeline
			{
				D3D12_RESOURCE_BARRIER barriers[] = {
					CD3DX12_RESOURCE_BARRIER::Transition(m_CommandBuffer.GetResource(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_COPY_DEST),
					CD3DX12_RESOURCE_BARRIER::Transition(counters.at(currentBuffers).GetResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
					CD3DX12_RESOURCE_BARRIER::Transition(counters.at(1 - currentBuffers).GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE)
				};
				m_CommandList->ResourceBarrier(ARRAYSIZE(barriers), barriers);
			}

			// Copy the counter that was just written to into the groups X of the dispatch args
			m_CommandList->CopyBufferRegion(m_CommandBuffer.GetResource(), 0, counters.at(1 - currentBuffers).GetResource(), 0, sizeof(UINT32));
			// Reset the other counter to 0
			counters.at(currentBuffers).SetValue(m_CommandList.Get(), counterUploadZero.GetResource());
			// Also reset block counter to 0 for next iteration
			blockCounter.SetValue(m_CommandList.Get(), counterUploadZero.GetResource());

			{
				D3D12_RESOURCE_BARRIER barriers[] = {
					CD3DX12_RESOURCE_BARRIER::Transition(m_CommandBuffer.GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT),
					CD3DX12_RESOURCE_BARRIER::Transition(counters.at(1 - currentBuffers).GetResource(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
					CD3DX12_RESOURCE_BARRIER::Transition(subBrickCountBuffer.GetResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
					CD3DX12_RESOURCE_BARRIER::Transition(prefixSumsBuffer.GetResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				};
				m_CommandList->ResourceBarrier(ARRAYSIZE(barriers), barriers);
			}

			// Swap buffers
			currentBuffers = 1 - currentBuffers;

			// Update parameters for next iteration
			buildParamsCB.BrickSize = buildParamsCB.SubBrickSize;
			buildParamsCB.SubBrickSize = buildParamsCB.BrickSize / 4.0f;

			PIXEndEvent(m_CommandList.Get());
			PIXEndEvent(m_CommandList.Get());
		}

		// Once all bricks have been built,
		// copy the final number of bricks back to the CPU for the next stage
		{
			const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(counters.at(currentBuffers).GetResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			m_CommandList->ResourceBarrier(1, &barrier);
		}
		counters.at(currentBuffers).ReadValue(m_CommandList.Get(), counterReadback.GetResource());

		PIXEndEvent(m_CommandList.Get());
	}

	{
		// Step 4: execute work and wait for it to complete
		THROW_IF_FAIL(m_CommandList->Close());
		ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
		const auto fenceValue = computeQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		// CPU wait until this work has been complete before continuing
		computeQueue->WaitForFenceCPUBlocking(fenceValue);

		THROW_IF_FAIL(m_CommandAllocator->Reset());
		THROW_IF_FAIL(m_CommandList->Reset(m_CommandAllocator.Get(), nullptr));
	}

	{
		// Step 5: Read counter value

		// It is only save to read the counter value after the GPU has finished its work
		const UINT brickCount = counterReadback.ReadElement(0);
		object->AllocateOptimalResources(brickCount, buildParamsCB.BrickSize, SDFObject::RESOURCES_WRITE);

		// Update build data required for the next stage
		aabbCB.BrickSize = buildParamsCB.BrickSize;
		aabbCB.BrickCount = brickCount;


		brickEvalCB.EvalSpace_MinBoundary = { -1.0f, -1.0f, -1.0f, 0.0f };
		brickEvalCB.EvalSpace_MaxBoundary = { 1.0f,  1.0f,  1.0f, 0.0f };
		brickEvalCB.EvalSpace_BrickSize = buildParamsCB.BrickSize;

		const auto bricksPerAxis = (brickEvalCB.EvalSpace_MaxBoundary - brickEvalCB.EvalSpace_MinBoundary) / brickEvalCB.EvalSpace_BrickSize;
		XMStoreUInt3(&brickEvalCB.EvalSpace_BricksPerAxis, bricksPerAxis);

		brickEvalCB.BrickPool_BrickCapacityPerAxis = object->GetBrickPoolDimensions(SDFObject::RESOURCES_WRITE);

		// Helpful values that can be calculated once on the CPU
		XMFLOAT3 evalSpaceRange;
		XMStoreFloat3(&evalSpaceRange, brickEvalCB.EvalSpace_MaxBoundary - brickEvalCB.EvalSpace_MinBoundary);
		brickEvalCB.EvalSpace_VoxelsPerUnit = static_cast<float>(brickEvalCB.EvalSpace_BricksPerAxis.x * SDF_BRICK_SIZE_VOXELS) / evalSpaceRange.x;

		brickEvalCB.SDFEditCount = editList.GetEditCount();
	}

	{
		// Step 6: build raytracing AABBs

		PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(43), L"Build AABBs");

		// This is a different compute shader that simply builds an AABB out of a brick from the buffer

		ID3D12DescriptorHeap* ppDescriptorHeaps[] = { g_D3DGraphicsContext->GetSRVHeap()->GetHeap() };
		m_CommandList->SetDescriptorHeaps(_countof(ppDescriptorHeaps), ppDescriptorHeaps);

		{
			D3D12_RESOURCE_BARRIER barriers[] =
			{
				CD3DX12_RESOURCE_BARRIER::Transition(object->GetAABBBuffer(SDFObject::RESOURCES_WRITE), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
				CD3DX12_RESOURCE_BARRIER::Transition(object->GetBrickBuffer(SDFObject::RESOURCES_WRITE), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			};
			m_CommandList->ResourceBarrier(ARRAYSIZE(barriers), barriers);
		}

		m_AABBBuilderPipeline->Bind(m_CommandList.Get());

		m_CommandList->SetComputeRoot32BitConstants(AABBBuilderSignature::BuildParameterSlot, SizeOfInUint32(AABBBuilderConstantBuffer), &aabbCB, 0);
		m_CommandList->SetComputeRootShaderResourceView(AABBBuilderSignature::BricksSlot, brickBuffers.at(currentBuffers).GetAddress());
		m_CommandList->SetComputeRootUnorderedAccessView(AABBBuilderSignature::AABBsSlot, object->GetAABBBufferAddress(SDFObject::RESOURCES_WRITE));
		m_CommandList->SetComputeRootUnorderedAccessView(AABBBuilderSignature::BrickBufferSlot, object->GetBrickBufferAddress(SDFObject::RESOURCES_WRITE));

		// Calculate number of thread groups and dispatch
		const UINT threadGroupX = (aabbCB.BrickCount + AABB_BUILDING_THREADS - 1) / AABB_BUILDING_THREADS;
		m_CommandList->Dispatch(threadGroupX, 1, 1);

		PIXEndEvent(m_CommandList.Get());
	}

	{
		// Step 7: Evaluate bricks

		PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(44), L"Evaluate Bricks");


		// Brick pool must be in unordered access state
		{
			D3D12_RESOURCE_BARRIER barriers[] =
			{
				CD3DX12_RESOURCE_BARRIER::Transition(object->GetBrickPool(SDFObject::RESOURCES_WRITE), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
				CD3DX12_RESOURCE_BARRIER::Transition(object->GetAABBBuffer(SDFObject::RESOURCES_WRITE), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
				CD3DX12_RESOURCE_BARRIER::Transition(object->GetBrickBuffer(SDFObject::RESOURCES_WRITE), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
			};
			m_CommandList->ResourceBarrier(ARRAYSIZE(barriers), barriers);
		}

		m_BrickEvaluatorPipeline->Bind(m_CommandList.Get());

		// Set resource views
		m_CommandList->SetComputeRoot32BitConstants(BrickEvaluatorSignature::BuildParameterSlot, SizeOfInUint32(BrickEvaluationConstantBuffer), &brickEvalCB, 0);
		m_CommandList->SetComputeRootShaderResourceView(BrickEvaluatorSignature::EditListSlot, editList.GetEditBufferAddress());
		m_CommandList->SetComputeRootShaderResourceView(BrickEvaluatorSignature::BrickBufferSlot, object->GetBrickBufferAddress(SDFObject::RESOURCES_WRITE));
		m_CommandList->SetComputeRootDescriptorTable(BrickEvaluatorSignature::BrickPoolSlot, object->GetBrickPoolUAV(SDFObject::RESOURCES_WRITE));

		// Execute one group for each brick
		const UINT threadGroupX = object->GetBrickCount(SDFObject::RESOURCES_WRITE);
		m_CommandList->Dispatch(threadGroupX, 1, 1);

		// Brick pool resource will now be read from in the subsequent stages
		{
			auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(object->GetBrickPool(SDFObject::RESOURCES_WRITE), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			m_CommandList->ResourceBarrier(1, &barrier);
		}

		PIXEndEvent(m_CommandList.Get());
	}

	PIXEndEvent(m_CommandList.Get());

	{
		// Step 9: Execute command list
		THROW_IF_FAIL(m_CommandList->Close());
		ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
		m_PreviousWorkFence = computeQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		computeQueue->WaitForFenceCPUBlocking(m_PreviousWorkFence);
	}
}
