#pragma once

namespace ysn
{
	struct GraphicsSettings
	{
		bool is_vsync_enabled = false;
		bool is_full_screen = false;
	};

	class YasnoSettings
	{
		GraphicsSettings graphics;
	public:
		bool LoadFromDisk();
		bool SaveToDisk();
	};
}