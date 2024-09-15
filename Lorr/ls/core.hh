#pragma once

#if defined(LS_COMPILER_CLANG) || defined(LS_COMPILER_GCC)
#define LS_LIKELY(x) __builtin_expect(x, 1)
#define LS_UNLIKELY(x) __builtin_expect(x, 0)
#define LS_DEBUGBREAK() __builtin_trap()
#elif defined(LS_COMPILER_MSVC)
#define LS_LIKELY(x) x
#define LS_UNLIKELY(x) x
#define LS_DEBUGBREAK() __debugbreak()
#else
#error "Compiler is not supported"
#endif

#define LS_CRASH() *(volatile int *)(nullptr) = 0;

#if defined(LS_DEBUG) && LS_DEBUG == 1
#define LS_EXPECT(expr) ((void)(!!(expr) || (LS_DEBUGBREAK(), 0)))
#else
#define LS_EXPECT(expr) (void)0;
#endif

#define LS_CONCAT_IMPL(x, y) x##y
#define LS_CONCAT(x, y) LS_CONCAT_IMPL(x, y)
#define LS_UNIQUE_VAR() LS_CONCAT(_ls_v_, __COUNTER__)
