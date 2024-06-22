module;

#include <intrin.h>

#include <System/Assert.hpp>

export module system.math;

import std;

export namespace ysn
{
constexpr bool IsPow2(std::uint64_t value)
{
    return (value == 0) ? false : ((value & (value - 1)) == 0);
}

template <typename T>
constexpr T AlignPow2(T value, std::uint64_t alignment)
{
    YSN_ASSERT(IsPow2(alignment));
    return ((value + static_cast<T>(alignment) - 1) & ~(static_cast<T>(alignment) - 1));
}

template <typename T>
__forceinline T AlignUpWithMask(T value, size_t mask)
{
    return (T)(((size_t)value + mask) & ~mask);
}

template <typename T>
__forceinline T AlignDownWithMask(T value, size_t mask)
{
    return (T)((size_t)value & ~mask);
}

template <typename T>
__forceinline T AlignUp(T value, size_t alignment)
{
    return AlignUpWithMask(value, alignment - 1);
}

template <typename T>
__forceinline T AlignDown(T value, size_t alignment)
{
    return AlignDownWithMask(value, alignment - 1);
}

template <typename T>
__forceinline bool IsAligned(T value, size_t alignment)
{
    return 0 == ((size_t)value & (alignment - 1));
}
} // namespace ysn
