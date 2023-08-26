// Created on Friday August 18th 2023 by exdal
// Last modified on Sunday August 20th 2023 by exdal

#pragma once

namespace lr::Hash
{
typedef u32 (*CRCHashFn)(const u8 *pData, u64 dataSize);  // Move this to something more global

// Because aligned memory is faster, if the data is big
u32 CRC32DataAligned(const u8 *pData, u64 dataSize);
// This function uses `_mm_crc32_u64`, better for large data, wasteful for small data
u32 CRC32DataAligned_64(const u8 *pData, u64 dataSize);

u32 CRC32Data_SW(const u8 *pData, u64 dataSize);

u32 CRC32String(CRCHashFn pHashFn, eastl::string_view str);
}  // namespace lr::Hash