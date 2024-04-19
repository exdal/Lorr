#include "CRC.hh"

namespace lr {
u32 Hash::CRC32DataAligned(const u8 *pData, u64 dataSize)
{
    ZoneScoped;

    u32 hash = ~0;
#if defined(__x86_64__) || defined(_M_AMD64) || defined(_M_IX86)
    u64 offset = 0;
    uptr address = (uptr)pData;

    // Align to u16
    if (address % 2 != 0 && dataSize > offset) {
        hash = _mm_crc32_u8(hash, pData[offset]);
        offset++;
    }

    // Align to u32
    if (address % 4 != 0 && dataSize > offset) {
        hash = _mm_crc32_u16(hash, *(u16 *)&pData[offset]);
        offset += 2;
    }

    while (dataSize - offset >= 4 && dataSize > offset) {
        hash = _mm_crc32_u32(hash, *(u32 *)&pData[offset]);
        offset += 4;
    }

    if (dataSize - offset >= 2) {
        hash = _mm_crc32_u16(hash, *(u16 *)&pData[offset]);
        offset += 2;
    }

    if (dataSize - offset >= 1) {
        hash = _mm_crc32_u8(hash, *(u8 *)&pData[offset]);
        offset++;
    }

#else
#error "CRC32 hash is not implemented for this arch. TODO!"
#endif

    return hash ^ ~0;
}

u32 Hash::CRC32DataAligned_64(const u8 *pData, u64 dataSize)
{
    ZoneScoped;

    u32 hash = ~0;
#if defined(__x86_64__) || defined(_M_AMD64) || defined(_M_IX86)
    u64 offset = 0;
    uptr address = (uptr)pData;

    // Align to u16
    if (address % 2 != 0 && dataSize > offset) {
        hash = _mm_crc32_u8(hash, pData[offset]);
        offset++;
    }

    // Align to u32
    if (address % 4 != 0 && dataSize > offset) {
        hash = _mm_crc32_u16(hash, *(u16 *)&pData[offset]);
        offset += 2;
    }

    // Align to u64
    if (address % 8 != 0 && dataSize > offset) {
        hash = _mm_crc32_u32(hash, *(u32 *)&pData[offset]);
        offset += 4;
    }

    while (dataSize - offset >= 8 && dataSize > offset) {
        hash = _mm_crc32_u64(hash, *(u64 *)&pData[offset]);
        offset += 8;
    }

    if (dataSize - offset >= 4) {
        hash = _mm_crc32_u32(hash, *(u32 *)&pData[offset]);
        offset += 4;
    }

    if (dataSize - offset >= 2) {
        hash = _mm_crc32_u16(hash, *(u16 *)&pData[offset]);
        offset += 2;
    }

    if (dataSize - offset >= 1) {
        hash = _mm_crc32_u8(hash, *(u8 *)&pData[offset]);
        offset++;
    }

#else
#error "CRC32 hash is not implemented for this arch. TODO!"
#endif

    return hash ^ ~0;
}

u32 Hash::CRC32Data_SW(const u8 *pData, u64 dataSize)
{
    ZoneScoped;

    u32 hash = ~0;
    u64 offset = 0;

    while (offset != dataSize)
        hash = (hash >> 8) ^ kCRC32CLUT[(hash ^ pData[offset++]) & 0xFF];

    return hash ^ ~0;
}

u32 Hash::CRC32String(CRCHashFn pHashFn, std::string_view str)
{
    return pHashFn((u8 *)str.data(), str.length());
}

u32 Hash::CRC32String(std::string_view str)
{
    return CRC32DataAligned((const u8 *)str.data(), str.length());
}

}  // namespace lr
