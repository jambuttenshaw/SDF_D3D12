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

	UINT MacroCount = 0;
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

	inline void SetThreadGroupsX(UINT count) { m_ThreadGroupsX = count; }
	inline void SetThreadGroupsY(UINT count) { m_ThreadGroupsY = count; }
	inline void SetThreadGroupsZ(UINT count) { m_ThreadGroupsZ = count; }

protected:
	ComPtr<ID3D12RootSignature> m_RootSignature;
	ComPtr<ID3D12PipelineState> m_PipelineState;

	UINT m_ThreadGroupsX = 1;
	UINT m_ThreadGroupsY = 1;
	UINT m_ThreadGroupsZ = 1;
};
