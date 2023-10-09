// Created on Friday October 28th 2022 by exdal
// Last modified on Monday August 28th 2023 by exdal

#pragma once

#include "APIObject.hh"
#include "Common.hh"

namespace lr::Graphics
{
enum class ShaderCompileFlag : u32
{
    None = 0,
    GenerateDebugInfo = 1 << 1,
    SkipOptimization = 1 << 2,
    SkipValidation = 1 << 3,
};
LR_TYPEOP_ARITHMETIC(ShaderCompileFlag, ShaderCompileFlag, |);
LR_TYPEOP_ARITHMETIC_INT(ShaderCompileFlag, ShaderCompileFlag, &);

struct Shader : APIObject
{
    ShaderStage m_Type = ShaderStage::Vertex;
    VkShaderModule m_pHandle;
};
LR_ASSIGN_OBJECT_TYPE(Shader, VK_OBJECT_TYPE_SHADER_MODULE);

struct ShaderCompileDesc
{
    eastl::string_view m_WorkingDir;
    eastl::string_view m_Code;
    ShaderStage m_Type = ShaderStage::Vertex;
    ShaderCompileFlag m_Flags = ShaderCompileFlag::None;
};

struct ShaderCompileOutput
{
    bool IsValid() { return !m_DataSpv.empty(); }
    ShaderStage m_Stage = ShaderStage::Vertex;
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