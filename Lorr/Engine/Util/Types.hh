#pragma once

#include <cstdint>

typedef double f64;
typedef float f32;

typedef uint64_t u64;
typedef int64_t i64;

typedef uint32_t u32;
typedef int32_t i32;

typedef uint16_t u16;
typedef int16_t i16;

typedef uint8_t u8;
typedef int8_t i8;

typedef u32 b32;

#ifndef LS_PTR_SIZE
#if defined(_WIN64) || defined(__LP64__) || defined(_LP64) || defined(_M_IA64) || defined(__ia64__) \
    || defined(__arch64__) || defined(__aarch64__) || defined(__mips64__) || defined(__64BIT__)     \
    || defined(__Ptr_Is_64)
#define LS_PTR_SIZE 8
#define LS_PTR_SIZE_64
#elif defined(__CC_ARM) && (__sizeof_ptr == 8)
#define LS_PTR_SIZE 8
#define LS_PTR_SIZE_64
#else
#define LS_PTR_SIZE 4
#define LS_PTR_SIZE_32
#endif
#endif

typedef std::size_t uptr;
typedef std::ptrdiff_t iptr;
typedef std::size_t usize;
