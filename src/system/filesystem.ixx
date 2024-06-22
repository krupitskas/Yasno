module;

#include <Windows.h>

export module system.filesystem;

import std;

export namespace ysn
{
bool DirectoryExists(const std::wstring_view path);
bool CreateDirectoryIfNotExists(const std::wstring_view path);
std::wstring GetVirtualFilesystemPath(const std::wstring_view path);
} // namespace ysn

namespace ysn
{
bool DirectoryExists(const std::wstring_view path)
{
    DWORD file_attributes = GetFileAttributes(path.data());

    if (file_attributes == INVALID_FILE_ATTRIBUTES)
    {
        return false; // Directory does not exist
    }

    return (file_attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool CreateDirectoryIfNotExists(const std::wstring_view path)
{
    if (!DirectoryExists(path))
    {
        if (CreateDirectory(path.data(), nullptr))
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return true;
    }
}

std::wstring GetVirtualFilesystemPath(const std::wstring_view path)
{
    std::string source_dir = "";
    std::wstring wsource_dir = std::wstring(source_dir.begin(), source_dir.end());
    // wsource_dir += L"/";
    wsource_dir += path;
    return wsource_dir;
}
} // namespace ysn
