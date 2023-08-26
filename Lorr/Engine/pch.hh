// Created on Wednesday May 4th 2022 by exdal
// Last modified on Saturday August 26th 2023 by exdal

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

#include <tracy/Tracy.hpp>

#include <EASTL/atomic.h>
#include <EASTL/array.h>
#include <EASTL/vector.h>
#include <EASTL/fixed_vector.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/span.h>

#include <DirectXMath.h>
#include <DirectXPackedVector.h>

#ifdef PTR_SIZE
#undef PTR_SIZE
#endif
#define PTR_SIZE EA_PLATFORM_PTR_SIZE

#if EA_PLATFORM_PTR_SIZE == 8
typedef unsigned __int64 uptr;
typedef signed __int64 iptr;
typedef u64 usize;
#else
typedef unsigned int uptr;
typedef signed int iptr;
typedef u32 usize;
#endif

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