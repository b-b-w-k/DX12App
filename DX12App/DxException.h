#pragma once
#include <string>
#include <comdef.h>

// Dx调试
class DxException
{
public:
	DxException() = default;
	DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber);

	std::wstring ToString()const;

	HRESULT ErrorCode = S_OK;
	std::wstring FunctionName;
	std::wstring Filename;
	int LineNumber = -1;
};


#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                                 \
do {                                                                     \
    HRESULT hr__ = (x);                                                  \
    if (FAILED(hr__))                                                    \
    {                                                                    \
        _com_error err__(hr__);                                          \
        wchar_t buf__[1024];                                             \
        swprintf_s(buf__,                                                \
            L"[DxError] hr=0x%08X  file=%s  line=%d\n"                   \
            L"  expr: %s\n"                                              \
            L"  msg : %s\n",                                             \
            hr__, __FILEW__, __LINE__, L#x, err__.ErrorMessage());       \
        OutputDebugStringW(buf__);                                       \
        __debugbreak();                                                  \
        throw DxException(hr__, L#x, __FILEW__, __LINE__);               \
    }                                                                    \
} while(0)
#endif