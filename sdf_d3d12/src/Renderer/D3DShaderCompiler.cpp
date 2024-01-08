#include "pch.h"
#include "D3DShaderCompiler.h"


D3DShaderCompiler::D3DShaderCompiler()
{
	THROW_IF_FAIL(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_Utils)));
	THROW_IF_FAIL(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_Compiler)));

	THROW_IF_FAIL(m_Utils->CreateDefaultIncludeHandler(&m_IncludeHandler));
}


HRESULT D3DShaderCompiler::CompileFromFileImpl(const wchar_t* file, const wchar_t* entryPoint, const wchar_t* target, D3D_SHADER_MACRO* defines, ComPtr<IDxcBlob>* ppBlob) const
{
	/*
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
	*/


	LPCWSTR pszArgs[] =
	{
		file,						// Optional shader source file name for error reporting
									// and for PIX shader source view.
#ifdef _DEBUG
		L"-Zi",
		L"-Od",
#endif
		L"-Fd", L".\\",				// Auto generate PDB name
		L"-E", entryPoint,          // Entry point.
		L"-T", target,				// Target.
		L"-Qstrip_reflect",         // Strip reflection into a separate blob. 
	};


    // Open source file.  
	ComPtr<IDxcBlobEncoding> pSource = nullptr;
	HRESULT result = m_Utils->LoadFile(file, nullptr, &pSource);

	// Check for success
	if (FAILED(result))
	{
		return result;
	}

	DxcBuffer Source;
	Source.Ptr = pSource->GetBufferPointer();
	Source.Size = pSource->GetBufferSize();
	Source.Encoding = DXC_CP_ACP;

	ComPtr<IDxcResult> pResults;
	m_Compiler->Compile(
		&Source,                // Source buffer.
		pszArgs,                // Array of pointers to arguments.
		_countof(pszArgs),      // Number of arguments.
		m_IncludeHandler.Get(), // User-provided interface to handle #include directives (optional).
		IID_PPV_ARGS(&pResults) // Compiler output status, buffer, and errors.
	);

	ComPtr<IDxcBlobUtf8> pErrors = nullptr;
	pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
	// Note that d3dcompiler would return null if no errors or warnings are present.
	// IDxcCompiler3::Compile will always return an error buffer, but its length
	// will be zero if there are no warnings or errors.
	if (pErrors != nullptr && pErrors->GetStringLength() != 0)
	{
		LOG_INFO("Warnings and Errors:\n{0}", pErrors->GetStringPointer());
	}

	//
	// Quit if the compilation failed.
	//
	pResults->GetStatus(&result);
	if (FAILED(result))
	{
		LOG_ERROR("Shader compilation failed.");
		return result;
	}

	

	ComPtr<IDxcBlobUtf16> pShaderName = nullptr;
	result = pResults->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(ppBlob->GetAddressOf()), &pShaderName);
	if (FAILED(result))
	{
		LOG_ERROR("Failed to get compiled shader object.");
		return result;
	}

	return result;
}
