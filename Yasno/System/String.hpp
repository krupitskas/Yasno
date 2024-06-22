#pragma once

#include <string>

namespace ysn
{
    std::wstring StringToWString(const std::string& str);
    std::string WStringToString(const std::wstring& wstr);
}
