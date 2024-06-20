#include "Bit.hh"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace lr {
usize memory::set_bit_count(u32 v)
{
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
    return __popcnt(v);
#else
    return __builtin_popcount(v);
#endif
}

usize memory::set_bit_count(u64 v)
{
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
    return __popcnt64(v);
#else
    return __builtin_popcountll(v);
#endif
}

u32 memory::find_least_set32(u32 v)
{
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
    unsigned long index;
    return _BitScanReverse(&index, v) ? index : ~0_u32;
#else
    i32 b = v ? 32 - __builtin_clz(v) : 0;
    return b - 1;
#endif
}

u32 memory::find_least_set64(u64 v)
{
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
    unsigned long index;
    return _BitScanReverse64(&index, v) ? index : ~0_u32;
#else
    i32 b = v ? 64 - __builtin_clzll(v) : 0;
    return b - 1;
#endif
}

u32 memory::find_first_set32(u32 v)
{
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
    unsigned long index;
    return _BitScanForward(&index, v) ? index : ~0_u32;
#else
    return __builtin_ffs((i32)v) - 1;
#endif
}

u32 memory::find_first_set64(u64 v)
{
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
    unsigned long index;
    return _BitScanForward64(&index, v) ? index : ~0_u32;
#else
    return __builtin_ffsll((i64)v) - 1;
#endif
}

}  // namespace lr
