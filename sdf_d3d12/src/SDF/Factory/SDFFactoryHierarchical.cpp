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

#include "SDFConstructionResources.h"


namespace BrickCounterSignature
{
	enum Value
	{
		BuildParameterSlot = 0,
		BrickCounterSlot,
		EditListSlot,
		BricksSlot,
		CountTableSlot,
		Count
	};
}

namespace ScanThreadGroupCalculatorSignature
{
	enum Value
	{
		BrickCounterSlot = 0,
		IndirectCommandArgumentSlot,
		Count
	};
}

namespace BrickScanSignature
{
	enum Value
	{
		CountTableSlot = 0,
		NumberOfCountsSlot,
		BlockPrefixSumTableSlot,
		BlockPrefixSumOutputTableSlot,
		PrefixSumTableSlot,
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

namespace EditTesterSignature
{
	enum Value
	{
		BuildParameterSlot = 0,
		EditListSlot,
		InIndexBufferSlot,
		BrickSlot,
		OutIndexBufferSlot,
		OutIndexCounterSlot,
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
		Count
	};
}

namespace BrickEvaluatorSignature
{
	enum Value
	{
		GroupOffsetSlot = 0,
		BuildParameterSlot,
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
		D3D12_COMMAND_SIGNATURE_DESC signatureDesc;
		signatureDesc.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);
		signatureDesc.NumArgumentDescs = 1;
		signatureDesc.pArgumentDescs = &indirectArg;
		signatureDesc.NodeMask = 0;

		// The command signature only contains dispatches, so no root signature is required
		THROW_IF_FAIL(device->CreateCommandSignature(&signatureDesc, nullptr, IID_PPV_ARGS(&m_CommandSignature)));
	}

	// Create command buffer
	{
		// Upload the commands into the command buffer
		m_CommandUploadBuffer.Allocate(device, s_NumCommands, 0, L"Command buffer upload");

		// The default commands is to only dispatch 64 groups
		// Only the first one will ever be used - the others will be re-assigned before they are ever dispatched
		D3D12_DISPATCH_ARGUMENTS commands[s_NumCommands];
		for (int i = 0; i < s_NumCommands; i++)
		{
			commands[i].ThreadGroupCountX = 64;
			commands[i].ThreadGroupCountY = 1;
			commands[i].ThreadGroupCountZ = 1;
		}
		m_CommandUploadBuffer.CopyElements(0, s_NumCommands, commands);

		// The upload buffer will be copied into the command buffer on bake init
	}


	// Allocate and populate upload buffers
	{
		m_CounterUploadZero.Allocate(device, 1, 0, L"Counters Upload Zero");
		// One counter needs to be able to be reset to 64
		m_CounterUpload64.Allocate(device, 1, 0, L"Counters Upload 64");

		m_CounterUploadZero.CopyElement(0, 0);
		m_CounterUpload64.CopyElement(0, 64);
	}

	InitializePipelines();
}


void SDFFactoryHierarchical::BakeSDFSync(SDFObject* object, const SDFEditList& editList)
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

		D3DComputePipelineDesc desc;
		desc.NumRootParameters = ARRAYSIZE(rootParams);
		desc.RootParameters = rootParams;
		desc.Shader = L"assets/shaders/compute/sub_brick_counter.hlsl";
		desc.EntryPoint = L"main";
		desc.Defines = nullptr;

		m_BrickCounterPipeline = std::make_unique<D3DComputePipeline>(&desc);
	}

	{
		using namespace ScanThreadGroupCalculatorSignature;

		CD3DX12_ROOT_PARAMETER1 rootParams[Count];
		rootParams[BrickCounterSlot].InitAsShaderResourceView(0);
		rootParams[IndirectCommandArgumentSlot].InitAsUnorderedAccessView(0);

		D3DComputePipelineDesc desc;
		desc.NumRootParameters = ARRAYSIZE(rootParams);
		desc.RootParameters = rootParams;
		desc.Shader = L"assets/shaders/compute/scan_thread_group_calculator.hlsl";
		desc.EntryPoint = L"main";
		desc.Defines = nullptr;

		m_ScanGroupCountCalculatorPipeline = std::make_unique<D3DComputePipeline>(&desc);
	}

	{
		using namespace BrickScanSignature;

		CD3DX12_ROOT_PARAMETER1 rootParams[Count];
		rootParams[CountTableSlot].InitAsShaderResourceView(0);
		rootParams[NumberOfCountsSlot].InitAsShaderResourceView(1);
		rootParams[BlockPrefixSumTableSlot].InitAsUnorderedAccessView(0);
		rootParams[BlockPrefixSumOutputTableSlot].InitAsUnorderedAccessView(1);
		rootParams[PrefixSumTableSlot].InitAsUnorderedAccessView(2);

		D3DComputePipelineDesc desc;
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

		D3DComputePipelineDesc desc;
		desc.NumRootParameters = ARRAYSIZE(rootParams);
		desc.RootParameters = rootParams;
		desc.Shader = L"assets/shaders/compute/sub_brick_builder.hlsl";
		desc.EntryPoint = L"main";
		desc.Defines = nullptr;

		m_BrickBuilderPipeline = std::make_unique<D3DComputePipeline>(&desc);
	}

	{
		using namespace EditTesterSignature;

		CD3DX12_ROOT_PARAMETER1 rootParams[Count];
		rootParams[BuildParameterSlot].InitAsConstants(SizeOfInUint32(BrickBuildParametersConstantBuffer), 0);
		rootParams[EditListSlot].InitAsShaderResourceView(0);
		rootParams[InIndexBufferSlot].InitAsShaderResourceView(1);
		rootParams[BrickSlot].InitAsUnorderedAccessView(0);
		rootParams[OutIndexBufferSlot].InitAsUnorderedAccessView(1);
		rootParams[OutIndexCounterSlot].InitAsUnorderedAccessView(2);

		D3DComputePipelineDesc desc;
		desc.NumRootParameters = ARRAYSIZE(rootParams);
		desc.RootParameters = rootParams;
		desc.Shader = L"assets/shaders/compute/edit_tester.hlsl";
		desc.EntryPoint = L"main";
		desc.Defines = nullptr;

		m_EditTesterPipeline = std::make_unique<D3DComputePipeline>(&desc);
	}

	{
		using namespace AABBBuilderSignature;

		CD3DX12_ROOT_PARAMETER1 rootParams[Count];
		rootParams[BuildParameterSlot].InitAsConstants(SizeOfInUint32(BrickEvaluationConstantBuffer), 0);
		rootParams[BricksSlot].InitAsShaderResourceView(0);
		rootParams[AABBsSlot].InitAsUnorderedAccessView(0);

		D3DComputePipelineDesc desc;
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
		rootParameters[GroupOffsetSlot].InitAsConstants(1, 0);
		rootParameters[BuildParameterSlot].InitAsConstants(SizeOfInUint32(BrickEvaluationConstantBuffer), 1);
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
	const UINT maxIterations = m_MaxBrickBuildIterations;

	const auto computeQueue = g_D3DGraphicsContext->GetComputeCommandQueue();

	THROW_IF_FAIL(m_CommandAllocator->Reset());
	THROW_IF_FAIL(m_CommandList->Reset(m_CommandAllocator.Get(), nullptr));
	{
		ID3D12DescriptorHeap* ppDescriptorHeaps[] = { g_D3DGraphicsContext->GetSRVHeap()->GetHeap() };
		m_CommandList->SetDescriptorHeaps(_countof(ppDescriptorHeaps), ppDescriptorHeaps);
	}

	PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(40), L"SDF Bake Hierarchical");

	PIXBeginEvent(PIX_COLOR_INDEX(51), L"Set up resources");
	{
		// Set up resources

		// Determine eval space size
		// It should be a multiple of the smallest brick size
		// Therefore the final iteration will build bricks of the desired size
		float evalSpaceSize = object->GetMinBrickSize();
		while (evalSpaceSize < editList.GetEvaluationRange())
		{
			evalSpaceSize *= 4.0f;
		}
		
		m_Resources.AllocateResources(object->GetBrickBufferCapacity(), editList, evalSpaceSize);
	}
	PIXEndEvent();

	BuildCommandList_Setup(object, m_Resources);
	BuildCommandList_HierarchicalBrickBuilding(object, m_Resources, maxIterations);

	{
		// Execute work and wait for it to complete
		THROW_IF_FAIL(m_CommandList->Close());
		ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
		const auto fenceValue = computeQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		// CPU wait until this work has been complete before continuing
		computeQueue->WaitForFenceCPUBlocking(fenceValue);

		THROW_IF_FAIL(m_CommandAllocator->Reset());
		THROW_IF_FAIL(m_CommandList->Reset(m_CommandAllocator.Get(), nullptr));

		ID3D12DescriptorHeap* ppDescriptorHeaps[] = { g_D3DGraphicsContext->GetSRVHeap()->GetHeap() };
		m_CommandList->SetDescriptorHeaps(_countof(ppDescriptorHeaps), ppDescriptorHeaps);
	}

	{
		// Read counter value

		// It is only save to read the counter value after the GPU has finished its work
		const UINT brickCount = m_Resources.GetCounterReadbackBuffer().ReadElement(0);
		const float brickSize = m_Resources.GetBrickBuildParams().BrickSize;
		object->AllocateOptimalResources(brickCount, brickSize, SDFObject::RESOURCES_WRITE);

		// Update build data required for the next stage
		m_Resources.GetBrickEvalParams().EvalSpace_BrickSize = brickSize;
		m_Resources.GetBrickEvalParams().BrickPool_BrickCapacityPerAxis = object->GetBrickPoolDimensions(SDFObject::RESOURCES_WRITE);
		m_Resources.GetBrickEvalParams().EvalSpace_VoxelsPerUnit = SDF_BRICK_SIZE_VOXELS / brickSize;
		m_Resources.GetBrickEvalParams().BrickCount = brickCount;
		m_Resources.GetBrickEvalParams().SDFEditCount = editList.GetEditCount();
	}

	BuildCommandList_BrickEvaluation(object, m_Resources);

	{
		// Execute command list
		PIXEndEvent(m_CommandList.Get());

		THROW_IF_FAIL(m_CommandList->Close());
		ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
		m_PreviousWorkFence = computeQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		computeQueue->WaitForFenceCPUBlocking(m_PreviousWorkFence);
	}
}

void SDFFactoryHierarchical::BuildCommandList_Setup(SDFObject* object, SDFConstructionResources& resources) const
{
	PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(41), L"Data upload");

	{
		// Transition brick buffer for reading
		const D3D12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(resources.GetReadIndexBuffer().GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST),
			CD3DX12_RESOURCE_BARRIER::Transition(resources.GetReadBrickBuffer().GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST),
			CD3DX12_RESOURCE_BARRIER::Transition(resources.GetCommandBuffer().GetResource(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_COPY_DEST),
		};
		m_CommandList->ResourceBarrier(ARRAYSIZE(barriers), barriers);
	}

	// Copy index upload data into index buffer
	const UINT64 numBytes = resources.GetIndexUploadBuffer().GetElementCount() * resources.GetIndexUploadBuffer().GetElementStride();
	m_CommandList->CopyBufferRegion(resources.GetReadIndexBuffer().GetResource(), 0, resources.GetIndexUploadBuffer().GetResource(), 0, numBytes);
	// Copy brick data into the brick buffer
	m_CommandList->CopyBufferRegion(resources.GetReadBrickBuffer().GetResource(), 0, resources.GetBrickUploadBuffer().GetResource(), 0, 64 * sizeof(Brick));
	// Copy default command buffer into the command buffer
	m_CommandList->CopyBufferRegion(resources.GetCommandBuffer().GetResource(), 0, m_CommandUploadBuffer.GetResource(), 0, s_NumCommands * sizeof(D3D12_DISPATCH_ARGUMENTS));

	{
		// Transition brick buffers into unordered access
		const D3D12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(resources.GetReadIndexBuffer().GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(resources.GetReadBrickBuffer().GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
			CD3DX12_RESOURCE_BARRIER::Transition(resources.GetWriteBrickBuffer().GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(resources.GetCommandBuffer().GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)
		};
		m_CommandList->ResourceBarrier(ARRAYSIZE(barriers), barriers);
	}

	// Set initial counter values
	resources.GetIndexCounter().SetValue(m_CommandList.Get(), m_CounterUploadZero.GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	resources.GetReadBrickCounter().SetValue(m_CommandList.Get(), m_CounterUpload64.GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	resources.GetWriteBrickCounter().SetValue(m_CommandList.Get(), m_CounterUploadZero.GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	{
		// Put prefix sum buffers into correct state
		const D3D12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(resources.GetSubBrickCountBuffer().GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
			CD3DX12_RESOURCE_BARRIER::Transition(resources.GetBlockPrefixSumsBuffer().GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
			CD3DX12_RESOURCE_BARRIER::Transition(resources.GetPrefixSumsBuffer().GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		};
		m_CommandList->ResourceBarrier(ARRAYSIZE(barriers), barriers);
	}

	PIXEndEvent(m_CommandList.Get());
}

void SDFFactoryHierarchical::BuildCommandList_HierarchicalBrickBuilding(SDFObject* object, SDFConstructionResources& resources, UINT maxIterations) const
{
	PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(42), L"Hierarchical brick building");

	// Multiple iterations will be made until the brick size is small enough
	UINT iterations = 0;
	while (resources.GetBrickBuildParams().SubBrickSize >= object->GetMinBrickSize() && iterations++ < maxIterations)
	{
		PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(45), L"Brick Building Iteration");

		PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(46), L"Brick Counting");
		// Step 3.1: Dispatch brick counter
		m_BrickCounterPipeline->Bind(m_CommandList.Get());

		// Set root parameters
		m_CommandList->SetComputeRoot32BitConstants(BrickCounterSignature::BuildParameterSlot, SizeOfInUint32(BrickBuildParametersConstantBuffer), &resources.GetBrickBuildParams(), 0);
		m_CommandList->SetComputeRootShaderResourceView(BrickCounterSignature::BrickCounterSlot, resources.GetReadBrickCounter().GetAddress());
		m_CommandList->SetComputeRootShaderResourceView(BrickCounterSignature::EditListSlot, resources.GetEditBuffer().GetAddress());
		m_CommandList->SetComputeRootUnorderedAccessView(BrickCounterSignature::BricksSlot, resources.GetReadBrickBuffer().GetAddress());
		m_CommandList->SetComputeRootUnorderedAccessView(BrickCounterSignature::CountTableSlot, resources.GetSubBrickCountBuffer().GetAddress());

		// Indirectly dispatch compute shader
		// The number of groups to dispatch is contained in the processed command buffer
		// The contents of this buffer will be updated after each iteration to dispatch the correct number of groups
		m_CommandList->ExecuteIndirect(m_CommandSignature.Get(), 1, resources.GetCommandBuffer().GetResource(), 0, nullptr, 0);


		{
			// Insert UAV barriers to make sure the first dispatch has finished writing to the brick buffer
			const D3D12_RESOURCE_BARRIER uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(resources.GetReadBrickBuffer().GetResource());
			m_CommandList->ResourceBarrier(1, &uavBarrier);
		}

		// Insert transition barriers for next stage of the pipeline
		{
			D3D12_RESOURCE_BARRIER barriers[] = {
				// Transition brick buffers
				CD3DX12_RESOURCE_BARRIER::Transition(resources.GetReadBrickBuffer().GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
				CD3DX12_RESOURCE_BARRIER::Transition(resources.GetWriteBrickBuffer().GetResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
				CD3DX12_RESOURCE_BARRIER::Transition(resources.GetSubBrickCountBuffer().GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
			};
			m_CommandList->ResourceBarrier(ARRAYSIZE(barriers), barriers);
		}

		// Calculate thread group counts to dispatch for the prefix sum stages
		{
			{
				D3D12_RESOURCE_BARRIER barriers[] = {
					CD3DX12_RESOURCE_BARRIER::Transition(resources.GetCommandBuffer().GetResource(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
				};
				m_CommandList->ResourceBarrier(ARRAYSIZE(barriers), barriers);
			}

			m_ScanGroupCountCalculatorPipeline->Bind(m_CommandList.Get());
			m_CommandList->SetComputeRootShaderResourceView(ScanThreadGroupCalculatorSignature::BrickCounterSlot, resources.GetReadBrickCounter().GetAddress());
			m_CommandList->SetComputeRootUnorderedAccessView(ScanThreadGroupCalculatorSignature::IndirectCommandArgumentSlot, resources.GetCommandBuffer().GetAddress() + sizeof(D3D12_DISPATCH_ARGUMENTS));
			m_CommandList->Dispatch(1, 1, 1);

			{
				D3D12_RESOURCE_BARRIER barriers[] = {
					CD3DX12_RESOURCE_BARRIER::UAV(resources.GetCommandBuffer().GetResource()),
					CD3DX12_RESOURCE_BARRIER::Transition(resources.GetCommandBuffer().GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)
				};
				m_CommandList->ResourceBarrier(ARRAYSIZE(barriers), barriers);
			}
		}

		PIXEndEvent(m_CommandList.Get());
		PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(47), L"Calculate prefix sum");

		// Step 3.2: Calculate prefix sums. This is done with 3 dispatches
		{
			m_ScanBlocksPipeline->Bind(m_CommandList.Get());
			m_CommandList->SetComputeRootShaderResourceView(BrickScanSignature::CountTableSlot, resources.GetSubBrickCountBuffer().GetAddress());
			m_CommandList->SetComputeRootShaderResourceView(BrickScanSignature::NumberOfCountsSlot, resources.GetReadBrickCounter().GetAddress());
			m_CommandList->SetComputeRootUnorderedAccessView(BrickScanSignature::BlockPrefixSumTableSlot, resources.GetBlockPrefixSumsBuffer().GetAddress());
			m_CommandList->SetComputeRootUnorderedAccessView(BrickScanSignature::PrefixSumTableSlot, resources.GetPrefixSumsBuffer().GetAddress());

			m_CommandList->ExecuteIndirect(m_CommandSignature.Get(), 1, resources.GetCommandBuffer().GetResource(), 1 * sizeof(D3D12_DISPATCH_ARGUMENTS), nullptr, 0);

			{
				const D3D12_RESOURCE_BARRIER uavBarriers[] = {
					CD3DX12_RESOURCE_BARRIER::UAV(resources.GetBlockPrefixSumsBuffer().GetResource()),
					CD3DX12_RESOURCE_BARRIER::UAV(resources.GetPrefixSumsBuffer().GetResource())
				};
				m_CommandList->ResourceBarrier(ARRAYSIZE(uavBarriers), uavBarriers);
			}

			m_ScanBlockSumsPipeline->Bind(m_CommandList.Get());
			m_CommandList->SetComputeRootShaderResourceView(BrickScanSignature::NumberOfCountsSlot, resources.GetReadBrickCounter().GetAddress());
			m_CommandList->SetComputeRootUnorderedAccessView(BrickScanSignature::BlockPrefixSumTableSlot, resources.GetBlockPrefixSumsBuffer().GetAddress());
			m_CommandList->SetComputeRootUnorderedAccessView(BrickScanSignature::BlockPrefixSumOutputTableSlot, resources.GetBlockPrefixSumsOutputBuffer().GetAddress());

			// Execute this stage up to twice for support for up to 262,000 input bricks (output can be up to 64x this)
			m_CommandList->ExecuteIndirect(m_CommandSignature.Get(), 1, resources.GetCommandBuffer().GetResource(), 2 * sizeof(D3D12_DISPATCH_ARGUMENTS), nullptr, 0);

			{
				const D3D12_RESOURCE_BARRIER uavBarriers[] = {
					CD3DX12_RESOURCE_BARRIER::UAV(resources.GetBlockPrefixSumsBuffer().GetResource()),
				};
				m_CommandList->ResourceBarrier(ARRAYSIZE(uavBarriers), uavBarriers);
			}

			m_SumScansPipeline->Bind(m_CommandList.Get());
			m_CommandList->SetComputeRootShaderResourceView(BrickScanSignature::NumberOfCountsSlot, resources.GetReadBrickCounter().GetAddress());
			m_CommandList->SetComputeRootUnorderedAccessView(BrickScanSignature::BlockPrefixSumOutputTableSlot, resources.GetBlockPrefixSumsOutputBuffer().GetAddress());
			m_CommandList->SetComputeRootUnorderedAccessView(BrickScanSignature::PrefixSumTableSlot, resources.GetPrefixSumsBuffer().GetAddress());

			m_CommandList->ExecuteIndirect(m_CommandSignature.Get(), 1, resources.GetCommandBuffer().GetResource(), 3 * sizeof(D3D12_DISPATCH_ARGUMENTS), nullptr, 0);

			{
				const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resources.GetPrefixSumsBuffer().GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				m_CommandList->ResourceBarrier(1, &barrier);
			}
		}

		PIXEndEvent(m_CommandList.Get());
		PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(48), L"Build sub-bricks");

		// Step 3.3: Dispatch brick builder
		// This step will make use of the ping-pong buffers to output the next collection of bricks to process
		m_BrickBuilderPipeline->Bind(m_CommandList.Get());

		// Set root parameters
		m_CommandList->SetComputeRoot32BitConstants(BrickBuilderSignature::BuildParameterSlot, SizeOfInUint32(BrickBuildParametersConstantBuffer), &resources.GetBrickBuildParams(), 0);
		m_CommandList->SetComputeRootShaderResourceView(BrickBuilderSignature::InBrickCounterSlot, resources.GetReadBrickCounter().GetAddress());
		m_CommandList->SetComputeRootShaderResourceView(BrickBuilderSignature::InBricksSlot, resources.GetReadBrickBuffer().GetAddress());
		m_CommandList->SetComputeRootShaderResourceView(BrickBuilderSignature::PrefixSumTableSlot, resources.GetPrefixSumsBuffer().GetAddress());
		m_CommandList->SetComputeRootUnorderedAccessView(BrickBuilderSignature::OutBrickCounterSlot, resources.GetWriteBrickCounter().GetAddress());
		m_CommandList->SetComputeRootUnorderedAccessView(BrickBuilderSignature::OutBricksSlot, resources.GetWriteBrickBuffer().GetAddress());

		// Indirectly dispatch compute shader
		// The number of groups to dispatch is contained in the processed command buffer
		// The contents of this buffer will be updated after each iteration to dispatch the correct number of groups
		m_CommandList->ExecuteIndirect(m_CommandSignature.Get(), 1, resources.GetCommandBuffer().GetResource(), 0, nullptr, 0);

		{
			// Insert UAV barriers to make sure the first dispatch has finished writing to the brick buffer
			const D3D12_RESOURCE_BARRIER uavBarriers[] = {
				CD3DX12_RESOURCE_BARRIER::UAV(resources.GetWriteBrickBuffer().GetResource()),
				CD3DX12_RESOURCE_BARRIER::UAV(resources.GetWriteBrickCounter().GetResource())
			};
			m_CommandList->ResourceBarrier(ARRAYSIZE(uavBarriers), uavBarriers);
		}

		// Insert transition barriers for next stage of the pipeline
		{
			D3D12_RESOURCE_BARRIER barriers[] = {
				CD3DX12_RESOURCE_BARRIER::Transition(resources.GetCommandBuffer().GetResource(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_COPY_DEST),
				CD3DX12_RESOURCE_BARRIER::Transition(resources.GetWriteBrickCounter().GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE)
			};
			m_CommandList->ResourceBarrier(ARRAYSIZE(barriers), barriers);
		}

		// Copy the counter that was just written to into the groups X of the dispatch args
		m_CommandList->CopyBufferRegion(resources.GetCommandBuffer().GetResource(), 0, resources.GetWriteBrickCounter().GetResource(), 0, sizeof(UINT32));
		// Reset the other counter to 0
		resources.GetReadBrickCounter().SetValue(m_CommandList.Get(), m_CounterUploadZero.GetResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		PIXEndEvent(m_CommandList.Get());
		PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(49), L"Cull edits");

		{
			D3D12_RESOURCE_BARRIER barriers[] = {
				CD3DX12_RESOURCE_BARRIER::Transition(resources.GetCommandBuffer().GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT),
				CD3DX12_RESOURCE_BARRIER::Transition(resources.GetSubBrickCountBuffer().GetResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
				CD3DX12_RESOURCE_BARRIER::Transition(resources.GetPrefixSumsBuffer().GetResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
			};
			m_CommandList->ResourceBarrier(ARRAYSIZE(barriers), barriers);
		}

		m_EditTesterPipeline->Bind(m_CommandList.Get());

		m_CommandList->SetComputeRoot32BitConstants(EditTesterSignature::BuildParameterSlot, SizeOfInUint32(BrickBuildParametersConstantBuffer), &resources.GetBrickBuildParams(), 0);
		m_CommandList->SetComputeRootShaderResourceView(EditTesterSignature::EditListSlot, resources.GetEditBuffer().GetAddress());
		m_CommandList->SetComputeRootShaderResourceView(EditTesterSignature::InIndexBufferSlot, resources.GetReadIndexBuffer().GetAddress());
		m_CommandList->SetComputeRootUnorderedAccessView(EditTesterSignature::BrickSlot, resources.GetWriteBrickBuffer().GetAddress());
		m_CommandList->SetComputeRootUnorderedAccessView(EditTesterSignature::OutIndexBufferSlot, resources.GetWriteIndexBuffer().GetAddress());
		m_CommandList->SetComputeRootUnorderedAccessView(EditTesterSignature::OutIndexCounterSlot, resources.GetIndexCounter().GetAddress());

		// Dispatch edit tester
		// Execute for the number of groups that were just written
		m_CommandList->ExecuteIndirect(m_CommandSignature.Get(), 1, resources.GetCommandBuffer().GetResource(), 0, nullptr, 0);

		// Barriers to wait for work to complete
		{
			D3D12_RESOURCE_BARRIER barriers[] = {
				CD3DX12_RESOURCE_BARRIER::UAV(resources.GetWriteBrickBuffer().GetResource()),
				CD3DX12_RESOURCE_BARRIER::UAV(resources.GetWriteIndexBuffer().GetResource()),
				CD3DX12_RESOURCE_BARRIER::UAV(resources.GetIndexCounter().GetResource()),
			};
			m_CommandList->ResourceBarrier(ARRAYSIZE(barriers), barriers);
		}

		// Reset index counter to 0
		resources.GetIndexCounter().SetValue(m_CommandList.Get(), m_CounterUploadZero.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		{
			D3D12_RESOURCE_BARRIER barriers[] = {
				CD3DX12_RESOURCE_BARRIER::Transition(resources.GetReadIndexBuffer().GetResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
				CD3DX12_RESOURCE_BARRIER::Transition(resources.GetWriteIndexBuffer().GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
				CD3DX12_RESOURCE_BARRIER::Transition(resources.GetWriteBrickCounter().GetResource(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
			};
			m_CommandList->ResourceBarrier(ARRAYSIZE(barriers), barriers);
		}

		// Swap buffers and update brick size
		resources.SwapBuffersAndRefineBrickSize();

		PIXEndEvent(m_CommandList.Get());
		PIXEndEvent(m_CommandList.Get());
	}

	// Once all bricks have been built,
	// copy the final number of bricks back to the CPU for the next stage
	resources.GetReadBrickCounter().ReadValue(m_CommandList.Get(), resources.GetCounterReadbackBuffer().GetResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	PIXEndEvent(m_CommandList.Get());
}

void SDFFactoryHierarchical::BuildCommandList_BrickEvaluation(SDFObject* object, SDFConstructionResources& resources) const
{
	{
		PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(43), L"Build AABBs");

		{
			D3D12_RESOURCE_BARRIER barriers[] =
			{
				CD3DX12_RESOURCE_BARRIER::Transition(object->GetAABBBuffer(SDFObject::RESOURCES_WRITE), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
			};
			m_CommandList->ResourceBarrier(ARRAYSIZE(barriers), barriers);
		}

		m_AABBBuilderPipeline->Bind(m_CommandList.Get());

		m_CommandList->SetComputeRoot32BitConstants(AABBBuilderSignature::BuildParameterSlot, SizeOfInUint32(BrickEvaluationConstantBuffer), &resources.GetBrickEvalParams(), 0);
		m_CommandList->SetComputeRootShaderResourceView(AABBBuilderSignature::BricksSlot, resources.GetReadBrickBuffer().GetAddress());
		m_CommandList->SetComputeRootUnorderedAccessView(AABBBuilderSignature::AABBsSlot, object->GetAABBBufferAddress(SDFObject::RESOURCES_WRITE));

		// Calculate number of thread groups and dispatch
		const UINT threadGroupX = (resources.GetBrickEvalParams().BrickCount + AABB_BUILDING_THREADS - 1) / AABB_BUILDING_THREADS;
		m_CommandList->Dispatch(threadGroupX, 1, 1);

		PIXEndEvent(m_CommandList.Get());
	}

	{
		PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(52), L"Copy Brick Data");

		// Copy brick data from temp buffer into objects brick buffer
		{
			D3D12_RESOURCE_BARRIER barriers[] =
			{
				CD3DX12_RESOURCE_BARRIER::Transition(object->GetBrickBuffer(SDFObject::RESOURCES_WRITE), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST),
			};
			m_CommandList->ResourceBarrier(ARRAYSIZE(barriers), barriers);
		}

		const UINT numBytes = resources.GetBrickEvalParams().BrickCount * sizeof(Brick);
		m_CommandList->CopyBufferRegion(object->GetBrickBuffer(SDFObject::RESOURCES_WRITE), 0, resources.GetReadBrickBuffer().GetResource(), 0, numBytes);

		PIXEndEvent(m_CommandList.Get());
	}

	{
		// Evaluate bricks

		PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(44), L"Evaluate Bricks");


		// Brick pool must be in unordered access state
		{
			D3D12_RESOURCE_BARRIER barriers[] =
			{
				CD3DX12_RESOURCE_BARRIER::Transition(object->GetBrickPool(SDFObject::RESOURCES_WRITE), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
				CD3DX12_RESOURCE_BARRIER::Transition(object->GetAABBBuffer(SDFObject::RESOURCES_WRITE), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
				CD3DX12_RESOURCE_BARRIER::Transition(object->GetBrickBuffer(SDFObject::RESOURCES_WRITE), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
			};
			m_CommandList->ResourceBarrier(ARRAYSIZE(barriers), barriers);
		}

		m_BrickEvaluatorPipeline->Bind(m_CommandList.Get());

		// Set resource views
		m_CommandList->SetComputeRoot32BitConstants(BrickEvaluatorSignature::BuildParameterSlot, SizeOfInUint32(BrickEvaluationConstantBuffer), &resources.GetBrickEvalParams(), 0);
		m_CommandList->SetComputeRootShaderResourceView(BrickEvaluatorSignature::EditListSlot, resources.GetEditBuffer().GetAddress());
		m_CommandList->SetComputeRootShaderResourceView(BrickEvaluatorSignature::BrickBufferSlot, object->GetBrickBufferAddress(SDFObject::RESOURCES_WRITE));
		m_CommandList->SetComputeRootDescriptorTable(BrickEvaluatorSignature::BrickPoolSlot, object->GetBrickPoolUAV(SDFObject::RESOURCES_WRITE));

		// Calculate number of thread groups and dispatch
		// Execute one group for each brick
		INT brickCount = static_cast<INT>(resources.GetBrickEvalParams().BrickCount);
		if (brickCount > 65536)
		{
			UINT groupOffset = 0;
			while (brickCount > 0)
			{
				m_CommandList->SetComputeRoot32BitConstant(BrickEvaluatorSignature::GroupOffsetSlot, groupOffset, 0);

				const UINT threadGroupX = min(brickCount, 65536);
				m_CommandList->Dispatch(threadGroupX, 1, 1);
				brickCount -= static_cast<INT>(threadGroupX);
				groupOffset += threadGroupX;
			}
		}
		else
		{
			m_CommandList->SetComputeRoot32BitConstant(BrickEvaluatorSignature::GroupOffsetSlot, 0, 0);

			const UINT threadGroupX = brickCount;
			m_CommandList->Dispatch(threadGroupX, 1, 1);
		}
		

		// Brick pool resource will now be read from in the subsequent stages
		{
			const auto barrier = 
				CD3DX12_RESOURCE_BARRIER::Transition(object->GetBrickPool(SDFObject::RESOURCES_WRITE), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			m_CommandList->ResourceBarrier(1, &barrier);
		}

		PIXEndEvent(m_CommandList.Get());
	}
}
