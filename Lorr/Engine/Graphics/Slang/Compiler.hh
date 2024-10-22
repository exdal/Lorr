#pragma once

#include "Engine/Graphics/Vulkan.hh"

#include <slang-com-ptr.h>
#include <slang.h>

namespace lr {
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
consteval void enable_bitmask(ShaderCompileFlag);

struct ShaderPreprocessorMacroInfo {
    std::string_view name = {};
    std::string_view value = {};
};

// If `ShaderProgramCompileInfo::source` has value, shader is loaded from memory.
// But `path` is STILL used to print debug information.
struct ShaderSessionInfo {
    std::vector<ShaderPreprocessorMacroInfo> definitions = {};
    ShaderCompileFlag compile_flags = ShaderCompileFlag::MatrixRowMajor;
    fs::path root_directory = {};
};

struct ShaderCompileInfo {
    fs::path path = {};
    ls::option<std::string_view> source = ls::nullopt;
};

struct ShaderReflection {
    PipelineLayoutID pipeline_layout_id = PipelineLayoutID::None;
    glm::u64vec3 thread_group_size = {};
};

struct SlangSession;
struct SlangModule : Handle<SlangModule> {
    auto destroy() -> void;

    auto get_entry_point(std::string_view name) -> std::vector<u32>;
    auto get_reflection() -> ShaderReflection;
    auto session() -> SlangSession *;
};

struct SlangSession : Handle<SlangSession> {
    friend SlangModule;

    auto destroy() -> void;

    auto load_module(const ShaderCompileInfo &info) -> ls::option<SlangModule>;
};

struct SlangCompiler : Handle<SlangCompiler> {
    static auto create() -> ls::option<SlangCompiler>;
    auto destroy() -> void;

    auto new_session(const ShaderSessionInfo &info) -> ls::option<SlangSession>;
};
}  // namespace lr
