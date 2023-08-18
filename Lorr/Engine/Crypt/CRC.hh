// Created on Friday August 18th 2023 by exdal
// Last modified on Friday August 18th 2023 by exdal

#pragma once

namespace lr::Hash
{
u32 CRC32Data(const u8 *pData, u64 dataSize);
u32 CRC32String(eastl::string_view str);

// This function uses `_mm_crc32_u64`, better for large data, wasteful for small data
u32 CRC32Data_64(const u8 *pData, u64 dataSize);
}  // namespace lr::Hash