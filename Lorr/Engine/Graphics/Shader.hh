#pragma once

#include "Common.hh"
#include "Util/VirtualDir.hh"

namespace lr::graphics {
struct Shader {
    Shader() = default;
    Shader(VkShaderModule shader_module, ShaderStageFlag stage_flags)
        : m_handle(shader_module),
          m_stage(stage_flags)
    {
    }

    ShaderStageFlag m_stage = ShaderStageFlag::Count;

    VkShaderModule m_handle = VK_NULL_HANDLE;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_SHADER_MODULE;
    operator auto &() { return m_handle; }
    explicit operator bool() { return m_handle != VK_NULL_HANDLE; }
};

enum class ShaderCompileFlag : u32 {
    None = 0,
    GenerateDebugInfo = 1 << 1,
    SkipOptimization = 1 << 2,
    SkipValidation = 1 << 3,
    MatrixRowMajor = 1 << 4,
    MatrixColumnMajor = 1 << 5,
    InvertY = 1 << 6,
    DXPositionW = 1 << 7,
    UseGLLayout = 1 << 8,
    UseScalarLayout = 1 << 9,
};
LR_TYPEOP_ARITHMETIC(ShaderCompileFlag, ShaderCompileFlag, |);
LR_TYPEOP_ARITHMETIC_INT(ShaderCompileFlag, ShaderCompileFlag, &);

struct ShaderPreprocessorMacroInfo {
    std::string_view name = {};
    std::string_view value = {};
};

struct ShaderCompileInfo {
    ShaderCompileFlag compile_flags = ShaderCompileFlag::None;
    std::string_view entry_point = "main";
    std::string_view real_path = {};  // for shader debugging
    std::string_view code = {};
    std::span<VirtualDir> virtual_env = {};
    std::span<std::string> include_dirs = {};
    std::span<ShaderPreprocessorMacroInfo> definitions = {};
};

namespace ShaderCompiler {
    bool init();
    Result<std::vector<u32>, VKResult> compile(const ShaderCompileInfo &info);
}  // namespace ShaderCompiler

}  // namespace lr::graphics
