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
#define ThrowIfFailed(x)                             \
{                                                    \
    HRESULT hr__ = (x);                              \
	if(FAILED(hr__))                                 \
	{                                                \
        throw DxException(hr__, L#x, L"", __LINE__); \
    }                                                \
}
#endif