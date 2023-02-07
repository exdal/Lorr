//
// Created on Monday 26th September 2022 by e-erdal
//

#pragma once

#include <intrin.h>

namespace lr::Memory
{
    // Allocates memory with `count * sizeof(Type)`
    template<typename T>
    T *Allocate(u64 count)
    {
        u64 size = count * sizeof(T);

        T *pData = (T *)malloc(size);
        assert(pData && "Failed to allocate memory.");
        memset(pData, 0, size);

        return pData;
    }

    // Reallocates memory with given `count`, not size in bytes.
    // count * sizeof(T)
    template<typename T>
    void Reallocate(T *&pData, u64 newCount)
    {
        u64 size = newCount * sizeof(T);
        pData = (T *)realloc(pData, size);
        assert(pData && "Failed to allocate memory.");
        memset(pData, 0, size);
    }

    template<typename T>
    void Release(T *pData)
    {
        free(pData);
    }

    template<typename T>
    void ZeroMem(T *pData, u64 count)
    {
        memset(pData, 0, count * sizeof(T));
    }

    template<typename T, typename U>
    void CopyMem(T *pData, U &val, u64 &offset)
    {
        memcpy((u8 *)pData + offset, (u8 *)&val, sizeof(U));
        offset += sizeof(U);
    }

    /// Unit Conversions ///

    constexpr u32 KiBToBytes(const u32 x)
    {
        return x << 10;
    }

    constexpr u32 MiBToBytes(const u32 x)
    {
        return x << 20;
    }

    /// Bitwise Operations ///

    template<typename T>
    u8 GetMSB(T mask)
    {
#if defined(EA_COMPILER_CLANG) || defined(EA_COMPILER_GNUC)
        return eastl::numeric_limits<T>::digits - (u8)__builtin_clzll(mask);
#elif defined(EA_COMPILER_MSVC)
        unsigned long pos;
        if (_BitScanReverse64(&pos, mask))
            return pos;
#else
        return (u8)log2(mask & -mask);
#endif

        assert(!"Uh oh");

        return ~0;
    }

    template<typename T>
    u8 GetLSB(T mask)
    {
#if defined(EA_COMPILER_CLANG) || defined(EA_COMPILER_GNUC)
        return (u8)__builtin_ffs(mask) - 1;
#elif defined(EA_COMPILER_MSVC)
        unsigned long pos;
        if (_BitScanForward(&pos, mask))
            return pos;
#else
        return (u8)log2(mask);
#endif

        // assert(!"Uh oh");

        return 0;
    }

    template<typename _Type>
    _Type FillBits(u32 count)
    {
        assert(count != 64 && "Mask is 0");

        return ((u64)~0) << count ^ ((u64)~0);
    }

    template<typename T, typename U>
    T AlignUp(T size, U alignment)
    {
        return (size + (alignment - 1)) & ~(alignment - 1);
    }

}  // namespace lr::Memory