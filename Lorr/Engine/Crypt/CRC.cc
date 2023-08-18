// Created on Friday August 18th 2023 by exdal
// Last modified on Friday August 18th 2023 by exdal

#include "CRC.hh"

namespace lr
{
u32 Hash::CRC32Data(const u8 *pData, u64 dataSize)
{
    u32 hash = 0;
#if defined(__x86_64__) || defined(_M_AMD64) || defined(_M_IX86)
    u64 offset = 0;
    uptr address = (uptr)pData;

    // Align to u16
    if (address % 2 != 0 && dataSize > offset)
    {
        hash = _mm_crc32_u8(hash, pData[offset]);
        offset++;
    }

    // Align to u32
    if (address % 4 != 0 && dataSize > offset)
    {
        hash = _mm_crc32_u16(hash, *(u16 *)&pData[offset]);
        offset += 2;
    }

    while (dataSize - offset >= 4 && dataSize > offset)
    {
        hash = _mm_crc32_u32(hash, *(u32 *)&pData[offset]);
        offset += 4;
    }

    if (dataSize - offset >= 2)
    {
        hash = _mm_crc32_u16(hash, *(u16 *)&pData[offset]);
        offset += 2;
    }

    if (dataSize - offset >= 1)
    {
        hash = _mm_crc32_u8(hash, *(u8 *)&pData[offset]);
        offset++;
    }

#else
#error "CRC32 hash is not implemented for this arch. TODO!"
#endif

    return hash;
}

u32 Hash::CRC32String(eastl::string_view str)
{
    return CRC32Data((u8 *)str.data(), str.length());
}
}  // namespace lr