
#include "dxUtils.h"

DxException::DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber) :
    ErrorCode(hr),
    FunctionName(functionName),
    Filename(filename),
    LineNumber(lineNumber)
{
}
std::wstring AnsiToWString(const std::string& str)
{
    WCHAR buffer[512];
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
    return std::wstring(buffer);
}
std::wstring DxException::ToString() const
{
    // »ñÈ¡´íÎóÂëµÄ×Ö·û´®ÃèÊö
    _com_error err(ErrorCode);
    std::wstring msg = err.ErrorMessage();

    std::wstringstream ss;
    ss << LineNumber;

    return FunctionName + L" failed in " + Filename + L"; line " + ss.str() + L"; error: " + msg;
}