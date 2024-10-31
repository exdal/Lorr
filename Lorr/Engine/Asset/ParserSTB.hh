#pragma once

#include "Engine/Graphics/Vulkan.hh"

namespace lr {
struct STBImageInfo {
    vk::Format format = {};
    vk::Extent2D extent = {};
    std::vector<u8> data = {};

    static auto parse(const fs::path &path) -> ls::option<STBImageInfo>;
    static auto parse(ls::span<u8> data) -> ls::option<STBImageInfo>;
};
}  // namespace lr
