module;

#include <intrin.h>

export module system.math;

import std;
import system.asserts;

export namespace ysn
{
	constexpr bool IsPow2(std::uint64_t value)
	{
		return (value == 0) ? false : ((value & (value - 1)) == 0);
	}

	template <typename T>
	constexpr T AlignPow2(T value, std::uint64_t alignment)
	{
		AssertMsg(IsPow2(alignment), "Alignment not power of two");
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
}
