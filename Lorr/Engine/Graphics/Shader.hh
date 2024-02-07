#pragma once

#include <EASTL/vector.h>

#include "APIObject.hh"
#include "Common.hh"
#include "Pipeline.hh"

namespace lr::Graphics {
enum class ShaderCompileFlag : u32 {
    None = 0,
    GenerateDebugInfo = 1 << 1,
    SkipOptimization = 1 << 2,
    SkipValidation = 1 << 3,
};
LR_TYPEOP_ARITHMETIC(ShaderCompileFlag, ShaderCompileFlag, |);
LR_TYPEOP_ARITHMETIC_INT(ShaderCompileFlag, ShaderCompileFlag, &);

struct Shader {
    ShaderStage m_type = ShaderStage::Vertex;

    VkShaderModule m_handle = VK_NULL_HANDLE;
};
LR_ASSIGN_OBJECT_TYPE(Shader, VK_OBJECT_TYPE_SHADER_MODULE);

struct ShaderCompileDesc {
    eastl::string m_code = {};
    eastl::span<eastl::string> m_include_dirs = {};
    eastl::span<eastl::string> m_definitions = {};
    ShaderCompileFlag m_flags = ShaderCompileFlag::None;
};

struct ShaderReflectionDesc {
    eastl::string_view m_descriptors_struct_name = "Descriptors";  //!< Identifier for offset of bindless descriptors
};

struct ShaderDescriptors {
    u16 m_indexes[16] = {};
};

struct ShaderStructRange {
    u32 offset = 0;
    u32 size = 0;
};

struct ShaderReflectionData {
    ShaderStage m_compiled_stage = {};
    u32 m_push_constant_size = 0;                 //!< Total size of constant used by user, useful for bindless
    ShaderStructRange m_descriptors_struct = {};  //!< Reflected bindless start offset
    eastl::string m_entry_point;
};

namespace ShaderCompiler {
    eastl::vector<u32> compile_shader(ShaderCompileDesc *desc);
    ShaderReflectionData reflect_spirv(ShaderReflectionDesc *desc, eastl::span<u32> ir);
}  // namespace ShaderCompiler
}  // namespace lr::Graphics
