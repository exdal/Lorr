#pragma once

namespace lr::Hash {
constexpr u32 kFNV32Value = 2166136261U;
constexpr u32 kFNV32Prime = 16777619U;

constexpr u32 FNV32(const char *pData, u32 dataLen)
{
    u32 fnv = kFNV32Value;

    for (u32 i = 0; i < dataLen; i++)
        fnv = (fnv * kFNV32Prime) ^ pData[i];

    return fnv;
}

template<typename T>
constexpr u32 FNV32(const T &val)
{
    return FNV32(&val, sizeof(T));
}

constexpr u32 FNV32String(std::string_view str)
{
    return FNV32(str.data(), str.length());
}

constexpr u64 kFNV64Value = 14695981039346656037ULL;
constexpr u64 kFNV64Prime = 1099511628211ULL;

constexpr u64 FNV64(const char *pData, u32 dataLen)
{
    u64 fnv = kFNV64Value;

    for (u32 i = 0; i < dataLen; i++)
        fnv = (fnv * kFNV64Prime) ^ pData[i];

    return fnv;
}

template<typename T>
constexpr u64 FNV64(const T &val)
{
    return FNV64(&val, sizeof(T));
}

constexpr u64 FNV64String(std::string_view str)
{
    return FNV64(str.data(), str.length());
}

}  // namespace lr::Hash

#define FNV32HashOf(x) (std::integral_constant<u32, lr::Hash::FNV32String(x)>::value)
#define FNV64HashOf(x) (std::integral_constant<u64, lr::Hash::FNV64String(x)>::value)
