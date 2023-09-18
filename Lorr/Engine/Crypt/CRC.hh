// Created on Friday August 18th 2023 by exdal
// Last modified on Tuesday September 12th 2023 by exdal

#pragma once

#include "CRCLUT.hh"

namespace lr::Hash
{
// Move this to something more global
typedef u32 (*CRCHashFn)(const u8 *pData, u64 dataSize);

// Because aligned memory is faster, if the data is big
u32 CRC32DataAligned(const u8 *pData, u64 dataSize);
// This function uses `_mm_crc32_u64`, better for large data
u32 CRC32DataAligned_64(const u8 *pData, u64 dataSize);
u32 CRC32Data_SW(const u8 *pData, u64 dataSize);
u32 CRC32String(CRCHashFn pHashFn, eastl::string_view str);
// As a default, will always select `CRC32DataAligned`
u32 CRC32String(eastl::string_view str);

constexpr u32 CRC32DataSV(eastl::string_view str)
{
    u32 hash = ~0;
    u64 offset = 0;

    while (offset != str.length())
        hash = (hash >> 8) ^ kCRC32CLUT[(hash ^ str[offset++]) & 0xFF];

    return hash ^ ~0;
}
}  // namespace lr::Hash

#define CRC32HashOf(x) (eastl::integral_constant<u32, lr::Hash::CRC32DataSV(x)>::value)