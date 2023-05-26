// Created on Friday October 28th 2022 by exdal
// Last modified on Tuesday May 23rd 2023 by exdal

#pragma once

#include "APIAllocator.hh"

#include "IO/BufferStream.hh"

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
    All = Vertex | Pixel | Compute | TessellationControl | TessellationEvaluation,
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
    eastl::string_view m_Code;
    ShaderStage m_Type = ShaderStage::None;
    ShaderFlag m_Flags = ShaderFlag::None;
};

struct ShaderCompileOutput
{
    bool IsValid() { return !m_DataSpv.empty(); }
    ShaderStage m_Stage = ShaderStage::None;
    eastl::span<u32> m_DataSpv = {};

    void *m_pProgram = nullptr;
    void *m_pShader = nullptr;
};

namespace ShaderCompiler
{
    void Init();
    ShaderCompileOutput CompileShader(ShaderCompileDesc *pDesc);
    void FreeProgram(ShaderCompileOutput &output);
}  // namespace ShaderCompiler

}  // namespace lr::Graphics