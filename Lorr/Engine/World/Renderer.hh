#pragma once

#include "Engine/Graphics/Task/TaskGraph.hh"
#include "Engine/World/RenderContext.hh"

namespace lr {

struct WorldRenderer {
    Device device = {};
    TaskGraph pbr_graph = {};

    WorldRenderContext context = {};

    TaskImageID swap_chain_image = TaskImageID::Invalid;
    TaskImageID imgui_font_image = TaskImageID::Invalid;

    WorldRenderer(Device device_);
    auto setup_context(this WorldRenderer &) -> void;

    auto record_setup_graph(this WorldRenderer &) -> void;
    auto record_pbr_graph(this WorldRenderer &, vk::Format swap_chain_format) -> void;

    auto render(
        this WorldRenderer &, SwapChain &swap_chain, ImageID image_id, ls::span<Semaphore> wait_semas, ls::span<Semaphore> signal_semas)
        -> bool;
};

}  // namespace lr
