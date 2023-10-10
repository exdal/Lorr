// Created on Wednesday May 4th 2022 by exdal
// Last modified on Saturday August 26th 2023 by exdal

#pragma once

#include "STL/Types.hh"

#include <tracy/Tracy.hpp>

#include <EASTL/string_view.h>
#include <EASTL/span.h>
#include <EASTL/array.h>

#include <DirectXMath.h>
using namespace DirectX;

#include "Utils/TypeOperators.hh"
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