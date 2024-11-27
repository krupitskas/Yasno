export module YasnoSettings;

namespace ysn
{
	export struct GraphicsSettings
	{
		bool is_vsync_enabled = false;
		bool is_full_screen = false;
		bool is_raster_mode = true;
	};

	export class YasnoSettings
	{
		GraphicsSettings graphics;
	public:
		bool LoadFromDisk();
		bool SaveToDisk();
	};
}