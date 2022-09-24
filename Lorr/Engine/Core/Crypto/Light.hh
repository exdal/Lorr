//
// Created on Saturday 16th July 2022 by e-erdal
// A place where lightweight hashes go
//

#pragma once

namespace lr::c
{
    constexpr u32 kFNVValue = 2166136261U;
    constexpr u32 kFNVPrime = 16777619U;

    constexpr u32 FNVHash(const char *pData, u32 dataLen)
    {
        u32 fnv = kFNVValue;

        for (u32 i = 0; i < dataLen; i++)
        {
            fnv ^= (u32)pData[i] * kFNVPrime;
        }

        return fnv;
    }

    template<typename T>
    constexpr u32 FNVHash(const T &val)
    {
        return FNVHash(&val, sizeof(val));
    }

    constexpr u32 FNVHashString(eastl::string_view str)
    {
        return FNVHash(str.data(), str.length());
    }

}  // namespace lr::c

#define FNVHashOf(x) (eastl::integral_constant<u32, lr::c::FNVHashString(x)>::value)