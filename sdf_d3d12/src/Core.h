#pragma once

#include <stdexcept>

// Taken from Microsoft D3D12 Samples
inline std::string HrToString(HRESULT hr)
{
    char s_str[64] = {};
    sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
    return std::string(s_str);
}

class HrException : public std::runtime_error
{
public:
    HrException(HRESULT hr) : std::runtime_error(HrToString(hr)), m_hr(hr) {}
    HRESULT Error() const { return m_hr; }
private:
    const HRESULT m_hr;
};


// My code resumes

#ifdef _DEBUG

#define THROW_IF_FAIL(x) { HRESULT hr = (x); if (FAILED(hr)) { throw HrException(hr); }}
#define ASSERT(x) if (!(x)) { DebugBreak(); }

#else

#define THROW_IF_FAIL(x) (void)(x);

#endif
