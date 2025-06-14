#pragma once

#include "Engine/Graphics/VulkanDevice.hh"

#include "Engine/Scene/GPUScene.hh"

#include <vuk/Value.hpp>

namespace lr {
struct CullPass {
    struct Outputs {
        vuk::Value<vuk::Buffer> visible_meshlet_instances_indices_buffer;
        vuk::Value<vuk::Buffer> meshlet_indices_buffer;
        vuk::Value<vuk::Buffer> transforms_buffer;
        vuk::Value<vuk::Buffer> meshles_buffer;
        vuk::Value<vuk::Buffer> camera_buffer;
        vuk::Value<vuk::Buffer> draw_triangles_cmd_buffer;
        vuk::Value<vuk::Buffer> reordered_indices_buffer;
    };

    u32 meshlet_instance_count = 0;
    GPU::CullFlags cull_flags = {};

    auto setup(this CullPass &, Device &device, SlangSession &slang_session) -> void;
    auto run(this CullPass &, Device &device) -> Outputs;
};
} // namespace lr
