#include "pch.h"
#include "D3DShaderCompiler.h"

#include "Core.h"

D3DShaderCompiler::D3DShaderCompiler()
{
	THROW_IF_FAIL(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_Utils)));
	THROW_IF_FAIL(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_Compiler)));

	THROW_IF_FAIL(m_Utils->CreateDefaultIncludeHandler(&m_IncludeHandler));
}


HRESULT D3DShaderCompiler::CompileFromFileImpl(const wchar_t* file, const wchar_t* entryPoint, const wchar_t* target, const std::vector<std::wstring>& defines, ComPtr<IDxcBlob>* ppBlob) const
{
	// format target
	std::wstring targetStr = target;
	targetStr += m_ShaderModelExtension;

	std::vector<LPCWSTR> args;
	args.push_back(file);

#ifdef _DEBUG
	args.push_back(L"-Zi");
	args.push_back(L"-Od");
	args.push_back(L"-Qembed_debug");
#else
	args.push_back(L"-Qstrip_debug");
	args.push_back(L"-O3");
#endif

	args.push_back(L"-E");
	args.push_back(entryPoint);

	args.push_back(L"-T");
	args.push_back(targetStr.c_str());

	args.push_back(L"-Qstrip_reflect");
	args.push_back(L"-enable-16bit-types");

	for (const auto& define : defines)
	{
		args.push_back(L"-D");
		args.push_back(define.c_str());
	}


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
		args.data(),                // Array of pointers to arguments.
		static_cast<UINT32>(args.size()),      // Number of arguments.
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


void D3DShaderCompiler::SetShaderModelImpl(const wchar_t* major, const wchar_t* minor)
{
	const std::wstring majorStr(major);
	const std::wstring minorStr(minor);
	m_ShaderModelExtension = L"_" + majorStr + L"_" + minorStr;
}

