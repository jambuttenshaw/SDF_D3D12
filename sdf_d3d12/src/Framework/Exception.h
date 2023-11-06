#pragma once

#include <comdef.h>


struct DXException
{
	HRESULT ErrorCode = S_OK;

	DXException() = default;
	DXException(HRESULT hr)
		: ErrorCode(hr)
	{}

	std::wstring ToString() const
	{
		return _com_error(ErrorCode).ErrorMessage();
	}
};
