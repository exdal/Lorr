#pragma once

namespace lr::memory {
usize set_bit_count(u32 v);
usize set_bit_count(u64 v);

// Count the Number of Leading Zero Bits
// https://www.felixcloutier.com/x86/lzcnt
u32 lzcnt_32(u32 v);
u32 lzcnt_64(u64 v);

// Count the Number of Trailing Zero Bits
// https://www.felixcloutier.com/x86/tzcnt
u32 tzcnt_32(u32 v);
u32 tzcnt_64(u64 v);
}  // namespace lr::memory
