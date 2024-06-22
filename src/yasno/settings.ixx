export module yasno.settings;

export namespace ysn
{
struct GraphicsSettings
{
    bool is_vsync_enabled = false;
    bool is_full_screen = false;
    bool is_raster_mode = true;
};

class YasnoSettings
{
    GraphicsSettings graphics;

public:
    // bool LoadFromDisk();
    // bool SaveToDisk();
};
} // namespace ysn