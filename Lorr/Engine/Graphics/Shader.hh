#pragma once

#include <EASTL/vector.h>

#include "APIObject.hh"
#include "Common.hh"
#include "Pipeline.hh"

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
    ShaderStage m_target_stage = ShaderStage::All;
    ShaderCompileFlag m_flags = ShaderCompileFlag::None;
};

struct ShaderReflectionDesc
{
    bool m_reflect_vertex_layout = false;
    eastl::string_view m_descriptors_start_name = "__lr_descriptors";  //!< Identifier for offset of bindless descriptors
};

struct ShaderReflectionData
{
    ShaderStage m_compiled_stage = {};
    u32 m_push_constant_size = 0;       //!< Total size of constant used by user, useful for bindless
    u32 m_descriptor_start_offset = 0;  //!< Reflected bindless start offset
    eastl::vector<PipelineVertexAttribInfo> m_vertex_attribs = {};
};

namespace ShaderCompiler
{
    eastl::vector<u32> compile_shader(ShaderCompileDesc *desc);
    ShaderReflectionData reflect_spirv(ShaderReflectionDesc *desc, eastl::span<u32> ir);
}  // namespace ShaderCompiler
}  // namespace lr::Graphics