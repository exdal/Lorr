#pragma once

#include "Engine/Graphics/VulkanTypes.hh"

namespace lr {
struct STBImageInfo {
    vuk::Format format = {};
    vuk::Extent3D extent = {};
    std::vector<u8> data = {};

    static auto parse(ls::span<u8> data) -> ls::option<STBImageInfo>;
};
}  // namespace lr
