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
    ShaderStage m_type = ShaderStage::Vertex;
    VkShaderModule m_handle;
};
LR_ASSIGN_OBJECT_TYPE(Shader, VK_OBJECT_TYPE_SHADER_MODULE);

struct ShaderCompileDesc
{
    eastl::string_view m_working_dir;
    eastl::string_view m_code;
    ShaderStage m_type = ShaderStage::Vertex;
    ShaderCompileFlag m_flags = ShaderCompileFlag::None;
};

struct ShaderCompileOutput
{
    bool is_valid() { return !m_data_spv.empty(); }
    ShaderStage m_stage = ShaderStage::Vertex;
    eastl::span<u32> m_data_spv = {};

    void *m_program = nullptr;
    void *m_shader = nullptr;
};

namespace ShaderCompiler
{
    void init();
    ShaderCompileOutput compile_shader(ShaderCompileDesc *desc);
    void free_program(ShaderCompileOutput &output);
}  // namespace ShaderCompiler

}  // namespace lr::Graphics