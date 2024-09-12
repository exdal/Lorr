#pragma once

namespace lr::memory {
usize set_bit_count(u32 v);
usize set_bit_count(u64 v);
u32 find_least_set32(u32 v);
u32 find_least_set64(u64 v);
u32 find_first_set32(u32 v);
u32 find_first_set64(u64 v);
}  // namespace lr::memory
