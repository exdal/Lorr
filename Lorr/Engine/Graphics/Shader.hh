#pragma once

#include "Common.hh"
#include "Engine/Util/VirtualDir.hh"

namespace lr {
struct Shader {
    Shader() = default;
    Shader(ShaderStageFlag stage_flags, VkShaderModule shader_module)
        : stage(stage_flags),
          handle(shader_module)
    {
    }

    ShaderStageFlag stage = ShaderStageFlag::Count;

    VkShaderModule handle = VK_NULL_HANDLE;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_SHADER_MODULE;
    operator auto &() { return handle; }
    explicit operator bool() { return handle != VK_NULL_HANDLE; }
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
template<>
struct has_bitmask<lr::ShaderCompileFlag> : std::true_type {};

struct ShaderPreprocessorMacroInfo {
    std::string_view name = {};
    std::string_view value = {};
};

struct ShaderCompileInfo {
    ShaderCompileFlag compile_flags = ShaderCompileFlag::MatrixRowMajor;
    std::string_view entry_point = "main";
    std::string_view real_path = {};  // for shader debugging
    std::string_view code = {};
    ls::span<const VirtualDir> virtual_env = {};
    ls::span<ShaderPreprocessorMacroInfo> definitions = {};
};

namespace ShaderCompiler {
    bool init();
    Result<std::vector<u32>, VKResult> compile(const ShaderCompileInfo &info);
}  // namespace ShaderCompiler

}  // namespace lr
