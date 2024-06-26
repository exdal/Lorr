#pragma once

namespace lr::Hash {
u64 XXH3Data(const u8 *pData, usize dataSize);
u64 XXH3String(std::string_view str);
}  // namespace lr::Hash
