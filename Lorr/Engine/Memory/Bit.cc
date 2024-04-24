#include "Bit.hh"

namespace lr {
/// FIND LEAST SET ///
u32 memory::find_least_set32(u32 v)
{
    i32 b = v ? 32 - __builtin_clz(v) : 0;
    return b - 1;
}

u32 memory::find_least_set64(u64 v)
{
    i32 b = v ? 64 - __builtin_clzll(v) : 0;
    return b - 1;
}

/// FIND FIRST SET ///
u32 memory::find_first_set32(u32 v)
{
    return __builtin_ffs((i32)v) - 1;
}

u32 memory::find_first_set64(u64 v)
{
    return __builtin_ffsll((i64)v) - 1;
}

}  // namespace lr
