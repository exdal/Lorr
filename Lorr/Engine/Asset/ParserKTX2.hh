#pragma once

#include "Engine/Graphics/VulkanTypes.hh"

namespace lr {
struct KTX2ImageInfo {
    vuk::Format format = vuk::Format::eUndefined;
    vuk::Extent3D base_extent = {};
    u32 mip_level_count = 0;

    std::vector<u64> per_level_offsets = {};
    std::vector<u8> data = {};

    static auto parse(ls::span<u8> bytes) -> ls::option<KTX2ImageInfo>;
    static auto parse_info(ls::span<u8> bytes) -> ls::option<KTX2ImageInfo>;
    static auto encode(ls::span<u8> raw_pixels, vuk::Format format, vuk::Extent3D extent, u32 level_count, bool normal) -> std::vector<u8>;
};
} // namespace lr
