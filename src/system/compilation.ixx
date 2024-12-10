export module system.compilation;

namespace ysn
{
enum class CompilationMode
{
    Debug,
    Profile,
    Release
};

constexpr CompilationMode g_compilation_mode =
#if YSN_DEBUG
    CompilationMode::Debug;
#elif YSN_PROFILE
    CompilationMode::Profile;
#elif YSN_RELEASE
    CompilationMode::Release;
#else
#error Unknown compilation mode!
#endif
} // namespace ysn

export namespace ysn
{
constexpr bool IsDebugActive()
{
    return g_compilation_mode == CompilationMode::Debug;
}

constexpr bool IsProfileActive()
{
    return g_compilation_mode == CompilationMode::Profile;
}

constexpr bool IsReleaseActive()
{
    return g_compilation_mode == CompilationMode::Release;
}
}

module :private;
