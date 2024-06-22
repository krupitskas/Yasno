module;

#include <comdef.h>

export module system.string_helpers;

import std;

export namespace ysn
{
std::wstring StringToWString(const std::string& str);
std::string WStringToString(const std::wstring& wstr);
std::string ConvertHrToString(HRESULT hr);
} // namespace ysn

module :private;

namespace ysn
{
std::wstring StringToWString(const std::string& str)
{
    std::wstring wstr;
    size_t size;
    wstr.resize(str.length());
    mbstowcs_s(&size, &wstr[0], wstr.size() + 1, str.c_str(), str.size());
    return wstr;
}

std::string WStringToString(const std::wstring& wstr)
{
    std::string str;
    size_t size;
    str.resize(wstr.length());
    wcstombs_s(&size, &str[0], str.size() + 1, wstr.c_str(), wstr.size());
    return str;
}

std::string ConvertHrToString(HRESULT hr)
{
    _com_error err(hr);

#ifdef UNICODE
    std::wstring w;
    w = err.ErrorMessage();
    std::string s = ysn::WStringToString(w);
#else
    std::string s = err.ErrorMessage();
#endif

    return s;
}
} // namespace ysn