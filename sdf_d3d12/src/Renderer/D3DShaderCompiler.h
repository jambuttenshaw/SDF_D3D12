#pragma once

#include <dxcapi.h>

using Microsoft::WRL::ComPtr;


class D3DShaderCompiler
{
public:

	static HRESULT CompileFromFile(
		const wchar_t* file,
		const wchar_t* entryPoint,
		const wchar_t* target,
		D3D_SHADER_MACRO* defines,
		ComPtr<IDxcBlob>* ppBlob
	)
	{
		return Get().CompileFromFileImpl(file, entryPoint, target, defines, ppBlob);
	}

private:
	inline static D3DShaderCompiler& Get()
	{
		static D3DShaderCompiler Instance;
		return Instance;
	}

	D3DShaderCompiler();

	HRESULT CompileFromFileImpl(
		const wchar_t* file,
		const wchar_t* entryPoint,
		const wchar_t* target,
		D3D_SHADER_MACRO* defines,
		ComPtr<IDxcBlob>* ppBlob
	) const;

private:
	ComPtr<IDxcUtils> m_Utils;
	ComPtr<IDxcCompiler3> m_Compiler;
	ComPtr<IDxcIncludeHandler> m_IncludeHandler;
};
