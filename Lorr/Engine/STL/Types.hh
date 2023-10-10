#pragma once

typedef double f64;
typedef float f32;

typedef unsigned long long u64;
typedef signed long long i64;

typedef unsigned int u32;
typedef signed int i32;

typedef unsigned short u16;
typedef signed short i16;

typedef unsigned char u8;
typedef signed char i8;

#ifndef LS_PTR_SIZE
#if defined(_WIN64) || defined(__LP64__) || defined(_LP64) || defined(_M_IA64) \
    || defined(__ia64__) || defined(__arch64__) || defined(__aarch64__)        \
    || defined(__mips64__) || defined(__64BIT__) || defined(__Ptr_Is_64)
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

#ifdef LS_PTR_SIZE_64
typedef u64 uptr;
typedef i64 iptr;
typedef u64 usize;
#else
typedef unsigned int uptr;
typedef signed int iptr;
typedef u32 usize;
#endif
