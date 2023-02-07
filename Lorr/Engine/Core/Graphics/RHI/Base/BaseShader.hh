//
// Created on Tuesday 25th October 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/Common.hh"

#include "Core/IO/BufferStream.hh"

#include <dxcapi.h>

namespace lr::Graphics
{
    struct Shader
    {
        ShaderStage m_Type = LR_SHADER_STAGE_NONE;
    };

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