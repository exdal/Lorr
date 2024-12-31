#pragma once

#include "Engine/Asset/Asset.hh"
#include "Engine/Graphics/Vulkan.hh"
#include "Engine/Graphics/VulkanDevice.hh"
#include "Engine/Window/Window.hh"
#include "Engine/World/World.hh"
#include "Engine/World/WorldRenderer.hh"

namespace lr {
struct ApplicationFlags {
    bool use_wayland : 1 = true;
};

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

    ApplicationFlags flags = {};
    Device device = {};
    Window window = {};
    SwapChain swapchain = {};

    WorldRenderer world_renderer = {};
    World world = {};

    AssetManager asset_man = {};

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
