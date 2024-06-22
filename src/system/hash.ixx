module;

#include <nmmintrin.h>

export module system.hash;

import std;
import system.math;

export namespace ysn
{
// This requires SSE4.2 which is present on Intel Nehalem (Nov. 2008)
// and AMD Bulldozer (Oct. 2011) processors.  I could put a runtime
// check for this, but I'm just going to assume people playing with
// DirectX 12 on Windows 10 have fairly recent machines.
#ifdef _M_X64
#define ENABLE_SSE_CRC32 1
#else
#define ENABLE_SSE_CRC32 0
#endif

#if ENABLE_SSE_CRC32
#pragma intrinsic(_mm_crc32_u32)
#pragma intrinsic(_mm_crc32_u64)
#endif

inline size_t HashRange(const std::uint32_t* const Begin, const std::uint32_t* const End, size_t Hash)
{
#if ENABLE_SSE_CRC32
    const std::uint64_t* Iter64 = (const std::uint64_t*)AlignUp(Begin, 8);
    const std::uint64_t* const End64 = (const std::uint64_t* const)AlignDown(End, 8);

    // If not 64-bit aligned, start with a single u32
    if ((std::uint32_t*)Iter64 > Begin)
        Hash = _mm_crc32_u32((std::uint32_t)Hash, *Begin);

    // Iterate over consecutive u64 values
    while (Iter64 < End64)
        Hash = _mm_crc32_u64((std::uint64_t)Hash, *Iter64++);

    // If there is a 32-bit remainder, accumulate that
    if ((std::uint32_t*)Iter64 < End)
        Hash = _mm_crc32_u32((std::uint32_t)Hash, *(std::uint32_t*)Iter64);
#else
    // An inexpensive hash for CPUs lacking SSE4.2
    for (const std::uint32_t* Iter = Begin; Iter < End; ++Iter)
        Hash = 16777619U * Hash ^ *Iter;
#endif

    return Hash;
}

template <typename T>
inline size_t HashState(const T* StateDesc, size_t Count = 1, size_t Hash = 2166136261U)
{
    static_assert((sizeof(T) & 3) == 0 && alignof(T) >= 4, "State object is not word-aligned");
    return HashRange((std::uint32_t*)StateDesc, (std::uint32_t*)(StateDesc + Count), Hash);
}
} // namespace ysn
