#include "pch.h"
#include "D3DPipeline.h"

#include "D3DGraphicsContext.h"
#include "D3DShaderCompiler.h"


D3DGraphicsPipeline::D3DGraphicsPipeline()
{
	const auto device = g_D3DGraphicsContext->GetDevice();

	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	// Create graphics root signature
	{
		// Create a root signature consisting of two root descriptors for CBVs (per-object and per-pass)
		CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
		CD3DX12_ROOT_PARAMETER1 rootParameters[1];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);	// raytracing output texture
		rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

		// Allow input layout and deny unnecessary access to certain pipeline stages
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS;

		// Create a point sampler
		D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.MipLODBias = 0;
		samplerDesc.MaxAnisotropy = 0;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc.ShaderRegister = 0;
		samplerDesc.RegisterSpace = 0;
		samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &samplerDesc, rootSignatureFlags);

		ComPtr<ID3DBlob> signature;
		THROW_IF_FAIL(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, nullptr));
		THROW_IF_FAIL(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_RootSignature)));
	}

	// Create the graphics pipeline state, which includes compiling and loading shaders
	{
		ComPtr<IDxcBlob> vertexShader;
		ComPtr<IDxcBlob> pixelShader;

		// Compile shaders
		THROW_IF_FAIL(D3DShaderCompiler::CompileFromFile(L"assets/shaders/graphics/finalpass.hlsl", L"VSMain", L"vs_6_3", nullptr, &vertexShader));
		THROW_IF_FAIL(D3DShaderCompiler::CompileFromFile(L"assets/shaders/graphics/finalpass.hlsl", L"PSMain", L"ps_6_3", nullptr, &pixelShader));

		// Describe and create the graphics pipeline state object (PSO)
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { nullptr, 0 };
		psoDesc.pRootSignature = m_RootSignature.Get();
		psoDesc.VS.pShaderBytecode = vertexShader->GetBufferPointer();
		psoDesc.VS.BytecodeLength = vertexShader->GetBufferSize();
		psoDesc.PS.pShaderBytecode = pixelShader->GetBufferPointer();
		psoDesc.PS.BytecodeLength = pixelShader->GetBufferSize();
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = false;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = g_D3DGraphicsContext->GetBackBufferFormat();
		psoDesc.DSVFormat = g_D3DGraphicsContext->GetDepthStencilFormat();
		psoDesc.SampleDesc.Count = 1;
		THROW_IF_FAIL(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_PipelineState)));
	}
}

void D3DGraphicsPipeline::Bind(ID3D12GraphicsCommandList* commandList) const
{
	commandList->SetGraphicsRootSignature(m_RootSignature.Get());
	commandList->SetPipelineState(m_PipelineState.Get());
}


D3DComputePipeline::D3DComputePipeline(D3DComputePipelineDesc* desc)
{
	const auto device = g_D3DGraphicsContext->GetDevice();

	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	// Create the compute root signature
	{
		// Create a default sampler
		D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.MipLODBias = 0;
		samplerDesc.MaxAnisotropy = 0;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc.ShaderRegister = 0;
		samplerDesc.RegisterSpace = 0;
		samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init_1_1(desc->NumRootParameters, desc->RootParameters, 1, &samplerDesc, D3D12_ROOT_SIGNATURE_FLAG_NONE);

		ComPtr<ID3DBlob> signature;
		THROW_IF_FAIL(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, nullptr));
		THROW_IF_FAIL(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_RootSignature)));
	}

	// Create the compute pipeline state
	{
		ComPtr<IDxcBlob> computeShader;
		THROW_IF_FAIL(D3DShaderCompiler::CompileFromFile(desc->Shader, desc->EntryPoint, L"cs_6_3", desc->Defines, &computeShader));

		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = m_RootSignature.Get();
		psoDesc.CS.pShaderBytecode = computeShader->GetBufferPointer();
		psoDesc.CS.BytecodeLength = computeShader->GetBufferSize();
		THROW_IF_FAIL(device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_PipelineState)));
	}
}


void D3DComputePipeline::Bind(ID3D12GraphicsCommandList* commandList)
{
	commandList->SetComputeRootSignature(m_RootSignature.Get());
	commandList->SetPipelineState(m_PipelineState.Get());
}
