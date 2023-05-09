#pragma once

#ifdef PTR_SIZE
#undef PTR_SIZE
#endif
#define PTR_SIZE sizeof(void *)

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

#include <stdio.h>
#include <stdint.h>

#include <Windows.h>
#undef CreateSemaphore

#include <Tracy.hpp>

#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/algorithm.h>
#include <EASTL/vector.h>
#include <EASTL/fixed_vector.h>
#include <EASTL/unordered_map.h>
#include <EASTL/map.h>
#include <EASTL/array.h>
#include <EASTL/atomic.h>
#include <EASTL/iterator.h>
#include <EASTL/queue.h>
#include <EASTL/span.h>

#include <DirectXMath.h>
#include <DirectXPackedVector.h>

template<typename _Type>
using cinitl = const std::initializer_list<_Type>;
template<typename _Type>
using initl = std::initializer_list<_Type>;

using namespace DirectX;
using namespace PackedVector;

#include "Utils/EnumFlags.hh"
#include "Core/Logger/Format.hh"
#include "Core/Logger/Logger.hh"

#define SAFE_DELETE(var) \
    if (var)             \
    {                    \
        delete var;      \
        var = NULL;      \
    }

#define SAFE_FREE(var) \
    if (var)           \
    {                  \
        free(var);     \
        var = NULL;    \
    }

#define SAFE_RELEASE(var) \
    if (var != nullptr)   \
    {                     \
        var->Release();   \
        var = nullptr;    \
    }
