#pragma once

#include "Engine/Asset/Asset.hh"
#include "Engine/Graphics/ImGuiRenderer.hh"
#include "Engine/Graphics/Vulkan.hh"
#include "Engine/Graphics/VulkanDevice.hh"
#include "Engine/Window/Window.hh"
#include "Engine/World/SceneRenderer.hh"

namespace lr {
struct ApplicationInfo {
    ls::span<c8 *> args = {};
    WindowInfo window_info = {};
};

struct Application {
    // Prevent `Application` from being copied, `::get()` only has to return a reference
    Application() = default;
    Application(const Application &) = delete;
    Application(Application &&) = delete;
    Application &operator=(const Application &) = delete;
    Application &operator=(Application &&) = delete;

    static Application &get();

    Device device = {};
    Window window = {};
    SwapChain swapchain = {};
    ImGuiRenderer imgui_renderer = {};
    // TODO: Should we remove this somewhere else? It's only used by editor.
    ImTextureID world_renderer_image_index = 0;
    SceneRenderer scene_renderer = {};
    AssetManager asset_man = {};
    ls::option<SceneID> active_scene_id = ls::nullopt;

    bool should_close = false;

    bool init(this Application &, const ApplicationInfo &info);
    void run(this Application &);
    void shutdown(this Application &);

    virtual bool do_super_init(ls::span<c8 *> args) = 0;
    virtual bool do_prepare() = 0;
    virtual bool do_update(f64 delta_time) = 0;
    virtual void do_shutdown() = 0;
};

}  // namespace lr
