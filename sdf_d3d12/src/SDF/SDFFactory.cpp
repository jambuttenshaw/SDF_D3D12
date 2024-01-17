#include "pch.h"
#include "SDFFactory.h"

#include "Renderer/D3DGraphicsContext.h"

#include "Renderer/Hlsl/ComputeHlslCompat.h"
#include "Renderer/Buffer/D3DCounterResource.h"

#include "SDFObject.h"


// Pipeline signatures
namespace BakeComputeRootSignature
{
	enum Value
	{
		BakeDataSlot = 0,
		PrimitiveDataSlot,
		OutputVolumeSlot,
		Count
	};
}


namespace AABBBuilderComputeRootSignature
{
	enum Value
	{
		BuildParameters = 0,
		VolumeSlot,
		AABBCounterSlot,
		AABBBufferSlot,
		PrimitiveDataSlot,
		Count
	};
}


struct SDFPrimitiveBufferType
{
	// Inverse world matrix is required to position SDF primitives
	XMMATRIX InvWorldMat;
	// Only uniform scale can be applied to SDFs
	float Scale;

	// SDF primitive data
	UINT Shape;
	UINT Operation;
	float BlendingFactor;

	static_assert(sizeof(SDFShapeProperties) == sizeof(XMFLOAT4));
	XMFLOAT4 ShapeProperties;

	XMFLOAT4 Color;


	// Constructors

	SDFPrimitiveBufferType() = default;

	// Construct from a SDFPrimitive
	SDFPrimitiveBufferType(const SDFPrimitive& primitive)
	{
		InvWorldMat = XMMatrixTranspose(XMMatrixInverse(nullptr, primitive.PrimitiveTransform.GetWorldMatrix()));
		Scale = primitive.PrimitiveTransform.GetScale();

		Shape = static_cast<UINT>(primitive.Shape);
		Operation = static_cast<UINT>(primitive.Operation);
		BlendingFactor = primitive.BlendingFactor;

		memcpy(&ShapeProperties, &primitive.ShapeProperties, sizeof(XMFLOAT4));

		Color = primitive.Color;
	}
};



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
}

SDFFactory::~SDFFactory()
{
	CloseHandle(m_FenceEvent);
}


void SDFFactory::BakeSDFSynchronous(SDFObject* object)
{
	const auto device = g_D3DGraphicsContext->GetDevice();

	// Resources used throughout the process
	ComPtr<ID3D12Resource> primitiveBuffer;
	D3DDescriptorAllocation primitiveBufferSRV;

	D3DCounterResource counterResource;
	D3DDescriptorAllocation counterResourceUAV;

	// Step 1: upload primitive array to gpu so that it can be accessed while rendering
	const size_t primitiveCount = object->GetPrimitiveCount();
	{

		const UINT64 bufferSize = primitiveCount * sizeof(SDFPrimitiveBufferType);

		// Create primitive buffer
		const CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
		const auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
		THROW_IF_FAIL(device->CreateCommittedResource(
			&uploadHeap,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&primitiveBuffer)));
		primitiveBuffer->SetName(L"SDF Factory Primitive Buffer");

		// Create an SRV for this resource
		// Allocate a descriptor
		primitiveBufferSRV = g_D3DGraphicsContext->GetSRVHeap()->Allocate(1);
		ASSERT(primitiveBufferSRV.IsValid(), "Failed to allocate SRV");

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = static_cast<UINT>(primitiveCount);
		srvDesc.Buffer.StructureByteStride = sizeof(SDFPrimitiveBufferType);
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		// Create srv
		device->CreateShaderResourceView(primitiveBuffer.Get(), &srvDesc, primitiveBufferSRV.GetCPUHandle());

		// Copy primitive data into buffer
		void* mappedData;
		THROW_IF_FAIL(primitiveBuffer->Map(0, nullptr, &mappedData));
		for (size_t index = 0; index < primitiveCount; index++)
		{
			auto primitive = static_cast<SDFPrimitiveBufferType*>(mappedData) + index;
			*primitive = object->GetPrimitive(index);
		}
		primitiveBuffer->Unmap(0, nullptr);
	}

	// Step 2: Update bake data in the constant buffer
	BakeDataConstantBuffer bakeDataBuffer;
	{
		bakeDataBuffer.PrimitiveCount = static_cast<UINT>(primitiveCount);
	}

	// Step 3: Build command list to execute SDF baker compute shader
	{
		// Scene texture must be in unordered access state
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(object->GetVolumeResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		m_CommandList->ResourceBarrier(1, &barrier);

		ID3D12DescriptorHeap* ppDescriptorHeaps[] = { g_D3DGraphicsContext->GetSRVHeap()->GetHeap() };
		m_CommandList->SetDescriptorHeaps(_countof(ppDescriptorHeaps), ppDescriptorHeaps);

		// Set pipeline state
		m_BakePipeline->Bind(m_CommandList.Get());

		// Set resource views
		m_CommandList->SetComputeRoot32BitConstants(BakeComputeRootSignature::BakeDataSlot, SizeOfInUint32(BakeDataConstantBuffer), &bakeDataBuffer, 0);
		m_CommandList->SetComputeRootDescriptorTable(BakeComputeRootSignature::PrimitiveDataSlot, primitiveBufferSRV.GetGPUHandle());
		m_CommandList->SetComputeRootDescriptorTable(BakeComputeRootSignature::OutputVolumeSlot, object->GetVolumeUAV());

		// Use fast ceiling of integer division
		const UINT threadGroupX = (object->GetVolumeResolution() + s_NumShaderThreads - 1) / s_NumShaderThreads;

		// Dispatch
		m_CommandList->Dispatch(threadGroupX, threadGroupX, threadGroupX);

		// Volume resource will now be read from in the subsequent stages
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(object->GetVolumeResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		m_CommandList->ResourceBarrier(1, &barrier);
	}

	// Step 4: Setup data required to build the AABBs
	AABBBuilderParametersConstantBuffer buildParamsBuffer;
	{
		// Allocate counter resource and create a UAV
		counterResource.Allocate(device, L"AABB Counter");
		counterResourceUAV = g_D3DGraphicsContext->GetSRVHeap()->Allocate(1);

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = 1;
		uavDesc.Buffer.StructureByteStride = 0;
		uavDesc.Buffer.CounterOffsetInBytes = 0;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

		device->CreateUnorderedAccessView(counterResource.GetResource(), nullptr, &uavDesc, counterResourceUAV.GetCPUHandle());

		// Populate build params
		UINT divisions = object->GetDivisions();
		buildParamsBuffer.Divisions = divisions;
		buildParamsBuffer.AABBDimensions = 2.0f / static_cast<float>(divisions);
		buildParamsBuffer.UVWIncrement = 1.0f / static_cast<float>(divisions);
	}

	// Step 5: Build command list to execute AABB builder compute shader
	{
		// Copy the initial counter value into the counter
		counterResource.SetCounterValue(m_CommandList.Get(), 0);

		// Setup pipeline
		m_AABBBuildPipeline->Bind(m_CommandList.Get());

		// Set resources
		m_CommandList->SetComputeRoot32BitConstants(AABBBuilderComputeRootSignature::BuildParameters, SizeOfInUint32(buildParamsBuffer), &buildParamsBuffer, 0);
		m_CommandList->SetComputeRootDescriptorTable(AABBBuilderComputeRootSignature::VolumeSlot, object->GetVolumeSRV());
		m_CommandList->SetComputeRootDescriptorTable(AABBBuilderComputeRootSignature::AABBCounterSlot, counterResourceUAV.GetGPUHandle());
		m_CommandList->SetComputeRootDescriptorTable(AABBBuilderComputeRootSignature::AABBBufferSlot, object->GetAABBBufferUAV());
		m_CommandList->SetComputeRootDescriptorTable(AABBBuilderComputeRootSignature::PrimitiveDataSlot, object->GetPrimitiveDataBufferUAV());

		// Calculate number of thread groups
		const UINT threadGroupX = (object->GetDivisions() + s_NumShaderThreads - 1) / s_NumShaderThreads;

		// Dispatch
		m_CommandList->Dispatch(threadGroupX, threadGroupX, threadGroupX);

		// Copy counter value to a readback resource
		counterResource.CopyCounterValue(m_CommandList.Get());
	}

	// Step 6: Execute command list and wait until completion
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

	// Step 7: Read counter value
	{
		// It is only save to read the counter value after the GPU has finished its work
		UINT aabbCount = counterResource.ReadCounterValue();
		object->SetAABBCount(aabbCount);
	}

	// Step 8: Clean up
	{
		THROW_IF_FAIL(m_CommandAllocator->Reset());
		THROW_IF_FAIL(m_CommandList->Reset(m_CommandAllocator.Get(), nullptr));

		// primitiveBuffer will get released
		// Free the srv
		primitiveBufferSRV.Free();
		counterResourceUAV.Free();
	}
}


void SDFFactory::InitializePipelines()
{
	// Create pipeline that is used to bake SDFs into 3D textures
	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[BakeComputeRootSignature::Count];
		rootParameters[BakeComputeRootSignature::BakeDataSlot].InitAsConstants(SizeOfInUint32(BakeDataConstantBuffer), 0);
		rootParameters[BakeComputeRootSignature::PrimitiveDataSlot].InitAsDescriptorTable(1, &ranges[0]);
		rootParameters[BakeComputeRootSignature::OutputVolumeSlot].InitAsDescriptorTable(1, &ranges[1]);

		D3DComputePipelineDesc desc;
		desc.NumRootParameters = ARRAYSIZE(rootParameters);
		desc.RootParameters = rootParameters;
		desc.Shader = L"assets/shaders/compute/sdf_baker.hlsl";
		desc.EntryPoint = L"main";
		desc.Defines = nullptr;

		m_BakePipeline = std::make_unique<D3DComputePipeline>(&desc);
	}


	// Create pipeline to build array of AABB geometry to fit the volume texture
	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[4];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
		ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);
		ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2);

		CD3DX12_ROOT_PARAMETER1 rootParameters[AABBBuilderComputeRootSignature::Count];
		rootParameters[AABBBuilderComputeRootSignature::BuildParameters].InitAsConstants(SizeOfInUint32(AABBBuilderParametersConstantBuffer), 0);
		rootParameters[AABBBuilderComputeRootSignature::VolumeSlot].InitAsDescriptorTable(1, &ranges[0]);
		rootParameters[AABBBuilderComputeRootSignature::AABBCounterSlot].InitAsDescriptorTable(1, &ranges[1]);
		rootParameters[AABBBuilderComputeRootSignature::AABBBufferSlot].InitAsDescriptorTable(1, &ranges[2]);
		rootParameters[AABBBuilderComputeRootSignature::PrimitiveDataSlot].InitAsDescriptorTable(1, &ranges[3]);

		D3DComputePipelineDesc desc;
		desc.NumRootParameters = ARRAYSIZE(rootParameters);
		desc.RootParameters = rootParameters;
		desc.Shader = L"assets/shaders/compute/aabb_builder.hlsl";
		desc.EntryPoint = L"main";
		desc.Defines = nullptr;

		m_AABBBuildPipeline = std::make_unique<D3DComputePipeline>(&desc);
	}
}
