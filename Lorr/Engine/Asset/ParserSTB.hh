#pragma once

#include "Engine/Graphics/VulkanTypes.hh"

namespace lr {
struct STBImageInfo {
    vuk::Format format = {};
    vuk::Extent3D extent = {};
    std::vector<u8> data = {};

    static auto parse(ls::span<u8> bytes) -> ls::option<STBImageInfo>;
    static auto parse_info(ls::span<u8> bytes) -> ls::option<STBImageInfo>;
};
}  // namespace lr
