#include "pch.h"
#include "SDFFactory.h"

#include "Renderer/D3DGraphicsContext.h"
#include "Renderer/Hlsl/ComputeHlslCompat.h"


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
