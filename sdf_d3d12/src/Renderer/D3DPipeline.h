#pragma once

using Microsoft::WRL::ComPtr;



struct D3DComputePipelineDesc
{
	// Root parameter descriptions
	UINT NumRootParameters = 0;
	D3D12_ROOT_PARAMETER1* RootParameters = nullptr;

	// Compute Shader
	const wchar_t* Shader = nullptr;
	const wchar_t* EntryPoint = nullptr;

	std::vector<std::wstring> Defines;
};

class D3DComputePipeline
{
public:
	D3DComputePipeline(D3DComputePipelineDesc* desc);

	void Bind(ID3D12GraphicsCommandList* commandList) const;

	inline ID3D12RootSignature* GetRootSignature() const { return m_RootSignature.Get(); }
	inline ID3D12PipelineState* GetPipelineState() const { return m_PipelineState.Get(); }

protected:
	ComPtr<ID3D12RootSignature> m_RootSignature;
	ComPtr<ID3D12PipelineState> m_PipelineState;
};
