#pragma once

#include <intrin.h>

namespace lr::Memory {
// Allocates memory with `count * sizeof(Type)`
template<typename T>
T *Allocate(u64 count)
{
    u64 size = count * sizeof(T);

    T *pData = (T *)malloc(size);
    TracyAlloc(pData, size);

    assert(pData && "Failed to allocate memory.");
    memset(pData, 0, size);

    return pData;
}

// Reallocates memory with given `count`, not size in bytes.
// count * sizeof(T)
template<typename T>
void Reallocate(T *&pData, u64 newCount)
{
    TracyFree(pData);

    u64 size = newCount * sizeof(T);
    pData = (T *)realloc(pData, size);

    TracyAlloc(pData, size);
    assert(pData && "Failed to allocate memory.");
    memset(pData, 0, size);
}

template<typename T>
void Release(T *pData)
{
    TracyFree(pData);

    free(pData);
}

template<typename T>
void ZeroMem(T *pData, u64 count)
{
    memset(pData, 0, count * sizeof(T));
}

template<typename _Type>
void CopyMem(_Type *pSrc, _Type *pDst, u32 count)
{
    memcpy(pSrc, pDst, sizeof(_Type) * count);
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
T find_msb(T mask)
{
    return eastl::numeric_limits<T>::digits - __builtin_clzll(mask);
}

template<typename T>
T find_lsb(T mask)
{
    return __builtin_ffsll(mask) - 1;
}

template<typename T>
T align_up(T size, T alignment)
{
    return (size + (alignment - 1)) & ~(alignment - 1);
}

template<typename T>
bool is_pow2(T v)
{
    return (v & -v) == v;
}

}  // namespace lr::Memory
