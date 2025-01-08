#include "Bit.hh"

#ifdef LS_COMPILER_MSVC
#include <intrin.h>

#define __builtin_popcount(v) __popcnt(v)
#define __builtin_popcountll(v) __popcnt64(v)
#define __builtin_ia32_lzcnt_u32(v) _lzcnt_u32(v)
#define __builtin_ia32_lzcnt_u64(v) _lzcnt_u64(v)
#define __builtin_ia32_tzcnt_u32(v) _tzcnt_u32(v)
#define __builtin_ia32_tzcnt_u64(v) _tzcnt_u64(v)

#elifdef LS_COMPILER_CLANG
#include <x86intrin.h>
#elifdef LS_COMPILER_GCC
#include <x86gprintrin.h>
#endif

namespace lr {
usize memory::set_bit_count(u32 v) {
    return __builtin_popcount(v);
}

usize memory::set_bit_count(u64 v) {
    return __builtin_popcountll(v);
}

__attribute__((target("lzcnt"))) u32 memory::lzcnt_32(u32 v) {
    return __builtin_ia32_lzcnt_u32(v);
}

__attribute__((target("lzcnt"))) u32 memory::lzcnt_64(u64 v) {
    return __builtin_ia32_lzcnt_u64(v);
}

__attribute__((target("bmi"))) u32 memory::tzcnt_32(u32 v) {
    return __builtin_ia32_tzcnt_u32(v);
}

__attribute__((target("bmi"))) u32 memory::tzcnt_64(u64 v) {
    return __builtin_ia32_tzcnt_u64(v);
}

}  // namespace lr
