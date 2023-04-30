//
// Created on Friday 28th October 2022 by exdal
//

#pragma once

#include "IO/BufferStream.hh"

#include "APIAllocator.hh"

namespace lr::Graphics
{
enum class ShaderStage : u32
{
    None = 0,
    Vertex = 1 << 0,
    Pixel = 1 << 1,
    Compute = 1 << 2,
    TessellationControl = 1 << 3,
    TessellationEvaluation = 1 << 4,
    Count = 5,
};

EnumFlags(ShaderStage);

enum class ShaderFlag : u32
{
    None = 0,
    GenerateDebugInfo = 1 << 1,
    SkipOptimization = 1 << 2,
    SkipValidation = 1 << 3,
};
EnumFlags(ShaderFlag);

struct Shader : APIObject<VK_OBJECT_TYPE_SHADER_MODULE>
{
    ShaderStage m_Type = ShaderStage::None;
    VkShaderModule m_pHandle;
};

struct ShaderCompileDesc
{
    static constexpr u32 kMaxDefineCount = 16;

    eastl::string_view m_PathOpt;

    ShaderStage m_Type = ShaderStage::None;
    ShaderFlag m_Flags = ShaderFlag::None;

    u32 m_DefineCount = 0;
    struct
    {
        eastl::string_view m_Name;
        eastl::string_view m_Value;
    } m_Defines[kMaxDefineCount] = {};
};

namespace ShaderCompiler
{
    void Init();
    bool CompileShader(ShaderCompileDesc *pDesc, BufferReadStream &buffer, u32 *&pDataOut, u32 &dataSizeOut);
}  // namespace ShaderCompiler

}  // namespace lr::Graphics