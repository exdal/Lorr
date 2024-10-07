#pragma once

#include "Common.hh"

namespace lr {
struct Shader {
    ShaderStageFlag stage = ShaderStageFlag::Count;

    VkShaderModule handle = VK_NULL_HANDLE;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_SHADER_MODULE;
    operator auto &() { return handle; }
    explicit operator bool() { return handle != VK_NULL_HANDLE; }
};

}  // namespace lr
