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
// constexpr bool IsDebugActive();
// constexpr bool IsProfileActive();
// constexpr bool IsReleaseActive();
}

module :private;
