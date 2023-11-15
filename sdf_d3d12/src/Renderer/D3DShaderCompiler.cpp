#include "pch.h"
#include "D3DShaderCompiler.h"

#include <D3Dcompiler.h>

using Microsoft::WRL::ComPtr;


HRESULT D3DShaderCompiler::CompileFromFile(const wchar_t* file, const char* entryPoint, const char* target, D3D_SHADER_MACRO* defines, ID3DBlob** ppBlob)
{
#ifdef _DEBUG
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif

	// Compile shader
	ComPtr<ID3DBlob> error;
	HRESULT result = D3DCompileFromFile(file, defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint, target, compileFlags, 0, ppBlob, error.GetAddressOf());

	// Check for errors
	if (result != S_OK)
	{
		char* error_msg = new char[error->GetBufferSize()];
		sprintf_s(error_msg, error->GetBufferSize(), "%s", static_cast<const char*>(error->GetBufferPointer()));
		LOG_ERROR("Shader failed to compile! Error: {0}", error_msg);
		delete[] error_msg;
	}

	return result;
}
