#pragma once

#include "Engine/Core/Handle.hh"
#include "Engine/Graphics/VulkanEnums.hh"

#include <slang-com-ptr.h>
#include <slang.h>

namespace lr {
struct ShaderPreprocessorMacroInfo {
    std::string_view name = {};
    std::string_view value = {};
};

struct SlangSessionInfo {
    std::vector<ShaderPreprocessorMacroInfo> definitions = {};
    fs::path root_directory = {};
};

struct SlangModuleInfo {
    fs::path path = {};
    std::string module_name = {};
    ls::option<std::string_view> source = ls::nullopt;
};

struct SlangEntryPoint {
    std::vector<u32> ir = {};
    vk::ShaderStageFlag shader_stage = {};
};

struct ShaderReflection {
    u32 pipeline_layout_index = 0;
    glm::u64vec3 thread_group_size = {};
};

struct SlangSession;
struct SlangModule : Handle<SlangModule> {
    auto destroy() -> void;

    auto get_entry_point(std::string_view name) -> ls::option<SlangEntryPoint>;
    auto get_reflection() -> ShaderReflection;
    auto session() -> SlangSession;
};

struct SlangSession : Handle<SlangSession> {
    friend SlangModule;

    auto destroy() -> void;

    auto load_module(const SlangModuleInfo &info) -> ls::option<SlangModule>;
};

struct SlangCompiler : Handle<SlangCompiler> {
    static auto create() -> ls::option<SlangCompiler>;
    auto destroy() -> void;

    auto new_session(const SlangSessionInfo &info) -> ls::option<SlangSession>;
};
}  // namespace lr
