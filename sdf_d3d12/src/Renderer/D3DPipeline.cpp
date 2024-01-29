#include "pch.h"
#include "D3DPipeline.h"

#include "D3DGraphicsContext.h"
#include "D3DShaderCompiler.h"


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
		THROW_IF_FAIL(D3DShaderCompiler::CompileFromFile(desc->Shader, desc->EntryPoint, L"cs", desc->Defines, &computeShader));

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
