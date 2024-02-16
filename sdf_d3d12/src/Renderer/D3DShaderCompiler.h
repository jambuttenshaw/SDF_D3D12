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
		const std::vector<std::wstring>& defines,
		ComPtr<IDxcBlob>* ppBlob
	)
	{
		return Get().CompileFromFileImpl(file, entryPoint, target, defines, ppBlob);
	}


	// e.g.: SetShaderModel(L"6", L"5"); for shader model 6.5
	static void SetShaderModel(const wchar_t* major, const wchar_t* minor)
	{
		Get().SetShaderModelImpl(major, minor);
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
		const std::vector<std::wstring>& defines,
		ComPtr<IDxcBlob>* ppBlob
	) const;

	void SetShaderModelImpl(const wchar_t* major, const wchar_t* minor);

private:
	ComPtr<IDxcUtils> m_Utils;
	ComPtr<IDxcCompiler3> m_Compiler;
	ComPtr<IDxcIncludeHandler> m_IncludeHandler;

	std::wstring m_ShaderModelExtension = L"_6_5"; // e.g. "_6_5"
};
