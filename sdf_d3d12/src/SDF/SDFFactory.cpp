#include "pch.h"
#include "SDFFactory.h"

#include "Renderer/D3DGraphicsContext.h"
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
		AABBBufferSlot,
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


void SDFFactory::BakeSDFSynchronous(const SDFObject* object)
{

	// Step 1: upload primitive array to gpu so that it can be accessed while rendering
	ComPtr<ID3D12Resource> primitiveBuffer;
	D3DDescriptorAllocation primitiveBufferSRV;

	const size_t primitiveCount = object->GetPrimitiveCount();
	{
		const auto device = g_D3DGraphicsContext->GetDevice();

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

	// Step 3: Build command list to execute compute shader
	{
		// Scene texture must be in unordered access state
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(object->GetResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		m_CommandList->ResourceBarrier(1, &barrier);

		ID3D12DescriptorHeap* ppDescriptorHeaps[] = { g_D3DGraphicsContext->GetSRVHeap()->GetHeap() };
		m_CommandList->SetDescriptorHeaps(_countof(ppDescriptorHeaps), ppDescriptorHeaps);

		m_CommandList->SetPipelineState(m_BakePipeline->GetPipelineState());

		// Set pipeline state
		m_BakePipeline->Bind(m_CommandList.Get());

		// Set resource views
		m_CommandList->SetComputeRoot32BitConstants(BakeComputeRootSignature::BakeDataSlot, SizeOfInUint32(BakeDataConstantBuffer), &bakeDataBuffer, 0);
		m_CommandList->SetComputeRootDescriptorTable(BakeComputeRootSignature::PrimitiveDataSlot, primitiveBufferSRV.GetGPUHandle());
		m_CommandList->SetComputeRootDescriptorTable(BakeComputeRootSignature::OutputVolumeSlot, object->GetUAV());

		// Use fast ceiling of integer division
		const UINT threadGroupX = (object->GetResolution() + s_NumShaderThreads - 1) / s_NumShaderThreads;

		// Dispatch
		m_CommandList->Dispatch(threadGroupX, threadGroupX, threadGroupX);

		// Perform graphics commands

		// Scene texture will now be used to render to the back buffer
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(object->GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		m_CommandList->ResourceBarrier(1, &barrier);
	}

	// Step 4: Execute command list and wait until completion
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

	// Step 5: Clean up
	{
		THROW_IF_FAIL(m_CommandAllocator->Reset());
		THROW_IF_FAIL(m_CommandList->Reset(m_CommandAllocator.Get(), nullptr));

		// primitiveBuffer will get released
		// Free the srv
		primitiveBufferSRV.Free();
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
		CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[AABBBuilderComputeRootSignature::Count];
		rootParameters[AABBBuilderComputeRootSignature::BuildParameters].InitAsConstants(SizeOfInUint32(AABBBuilderParametersConstantBuffer), 0);
		rootParameters[AABBBuilderComputeRootSignature::VolumeSlot].InitAsDescriptorTable(1, &ranges[0]);
		rootParameters[AABBBuilderComputeRootSignature::AABBBufferSlot].InitAsDescriptorTable(1, &ranges[1]);

		D3DComputePipelineDesc desc;
		desc.NumRootParameters = ARRAYSIZE(rootParameters);
		desc.RootParameters = rootParameters;
		desc.Shader = L"assets/shaders/compute/aabb_builder.hlsl";
		desc.EntryPoint = L"main";
		desc.Defines = nullptr;

		m_AABBBuildPipeline = std::make_unique<D3DComputePipeline>(&desc);
	}
}
