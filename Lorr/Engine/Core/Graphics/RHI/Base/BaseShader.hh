//
// Created on Tuesday 25th October 2022 by e-erdal
//

#pragma once

#include "Core/IO/BufferStream.hh"

#include <dxcapi.h>

namespace lr::Graphics
{
    enum ShaderStage
    {
        LR_SHADER_STAGE_NONE = 0,
        LR_SHADER_STAGE_VERTEX = 1 << 0,
        LR_SHADER_STAGE_PIXEL = 1 << 1,
        LR_SHADER_STAGE_COMPUTE = 1 << 2,
        LR_SHADER_STAGE_HULL = 1 << 3,
        LR_SHADER_STAGE_DOMAIN = 1 << 4,

        LR_SHADER_STAGE_COUNT = 5,
    };

    EnumFlags(ShaderStage);

    struct Shader
    {
        ShaderStage m_Type = LR_SHADER_STAGE_NONE;
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

        eastl::wstring_view m_PathOpt;

        ShaderStage m_Type = LR_SHADER_STAGE_NONE;
        ShaderFlags m_Flags = LR_SHADER_FLAG_NONE;

        u32 m_DefineCount = 0;
        struct
        {
            eastl::wstring_view m_Name;
            eastl::wstring_view m_Value;
        } m_Defines[kMaxDefinceCount] = {};
    };

    namespace ShaderCompiler
    {
        IDxcBlob *CompileFromBuffer(ShaderCompileDesc *pDesc, BufferReadStream &buf);
    }

}  // namespace lr::Graphics