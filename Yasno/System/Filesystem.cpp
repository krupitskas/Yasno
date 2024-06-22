#include "Filesystem.hpp"

namespace ysn
{
	std::wstring GetVirtualFilesystemPath(std::wstring path)
	{
		std::string source_dir = "";
		std::wstring wsource_dir = std::wstring(source_dir.begin(), source_dir.end());
		//wsource_dir += L"/";
		wsource_dir += path;
		return wsource_dir;
	}
}
