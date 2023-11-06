#pragma once


class D3DShaderCompiler
{
public:

	static HRESULT CompileFromFile(
		const wchar_t* file,
		const char* entryPoint,
		const char* target,
		D3D_SHADER_MACRO* defines,
		ID3DBlob** ppBlob
	);

};
