#pragma once

#include <string>

namespace ysn
{
	bool DirectoryExists(const std::wstring& path);
	bool CreateDirectoryIfNotExists(const std::wstring& path);

	std::wstring GetVirtualFilesystemPath(std::wstring path);
}
