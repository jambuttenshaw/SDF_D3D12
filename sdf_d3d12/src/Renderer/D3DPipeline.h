#pragma once

using Microsoft::WRL::ComPtr;



struct D3DComputePipelineDesc
{
	// Root parameter descriptions
	UINT NumRootParameters = 0;
	D3D12_ROOT_PARAMETER1* RootParameters = nullptr;

	// Compute Shader
	const wchar_t* Shader = nullptr;
	const char* EntryPoint = nullptr;

	D3D_SHADER_MACRO* Defines = nullptr;
};



class D3DGraphicsPipeline
{
public:
	D3DGraphicsPipeline();

	void Bind(ID3D12GraphicsCommandList* commandList) const;

protected:
	ComPtr<ID3D12RootSignature> m_RootSignature;
	ComPtr<ID3D12PipelineState> m_PipelineState;
};



class D3DComputePipeline
{
public:
	D3DComputePipeline(D3DComputePipelineDesc* desc);

	void Bind(ID3D12GraphicsCommandList* commandList);

	inline ID3D12RootSignature* GetRootSignature() const { return m_RootSignature.Get(); }
	inline ID3D12PipelineState* GetPipelineState() const { return m_PipelineState.Get(); }

protected:
	ComPtr<ID3D12RootSignature> m_RootSignature;
	ComPtr<ID3D12PipelineState> m_PipelineState;
};
