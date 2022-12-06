//
// Created on Tuesday 25th October 2022 by e-erdal
//

#pragma once

#include "Core/IO/BufferStream.hh"

#include <dxcapi.h>

namespace lr::Graphics
{
    enum class ShaderStage
    {
        None = 0,
        Vertex = 1 << 0,
        Pixel = 1 << 1,
        Compute = 1 << 2,
        Hull = 1 << 3,
        Domain = 1 << 4,
        Geometry = 1 << 5,
    };

    EnumFlags(ShaderStage);

    struct BaseShader
    {
        ShaderStage Type;
    };

    enum ShaderFlags : u32
    {
        LR_SHADER_FLAG_NONE = 0,
        LR_SHADER_FLAG_USE_SPIRV = 1 << 0,
        LR_SHADER_FLAG_USE_DEBUG = 1 << 1,
        LR_SHADER_FLAG_SKIP_OPTIMIZATION = 1 << 2,
        LR_SHADER_FLAG_MATRIX_ROW_MAJOR = 1 << 3,
        LR_SHADER_FLAG_WARNINGS_ARE_ERRORS = 1 << 4,
        LR_SHADER_FLAG_SKIP_VALIDATION = 1 << 5,
        LR_SHADER_FLAG_ALL_RESOURCES_BOUND = 1 << 6,
    };
    EnumFlags(ShaderFlags);

    struct ShaderCompileDesc
    {
        static constexpr u32 kMaxDefinceCount = 16;

        eastl::wstring_view PathOpt;

        ShaderStage Type;
        ShaderFlags Flags;

        u32 DefineCount = 0;
        struct
        {
            eastl::wstring_view Name;
            eastl::wstring_view Value;
        } Defines[kMaxDefinceCount] = {};
    };

    namespace ShaderCompiler
    {
        IDxcBlob *CompileFromBuffer(ShaderCompileDesc *pDesc, BufferReadStream &buf);
    }

}  // namespace lr::Graphics