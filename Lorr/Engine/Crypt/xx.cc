#include "xx.hh"

#include <xxhash.h>

namespace lr {
u64 Hash::XXH3Data(const u8 *pData, usize dataSize)
{
    ZoneScoped;

    return XXH3_64bits(pData, dataSize);
}

u64 Hash::XXH3String(std::string_view str)
{
    ZoneScoped;

    return XXH3Data((const u8 *)str.data(), str.length());
}
}  // namespace lr
