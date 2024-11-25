#include "Filesystem.hpp"

namespace ysn
{
    bool DirectoryExists(const std::wstring& path)
    {
        DWORD file_attributes = GetFileAttributes(path.c_str());

        if (file_attributes == INVALID_FILE_ATTRIBUTES)
        {
            return false; // Directory does not exist
        }

        return (file_attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

	bool CreateDirectoryIfNotExists(const std::wstring& path)
	{
		if (!DirectoryExists(path))
		{
			if (CreateDirectory(path.c_str(), nullptr))
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

    std::wstring GetVirtualFilesystemPath(std::wstring path)
	{
		std::string source_dir = "";
		std::wstring wsource_dir = std::wstring(source_dir.begin(), source_dir.end());
		//wsource_dir += L"/";
		wsource_dir += path;
		return wsource_dir;
	}
}
