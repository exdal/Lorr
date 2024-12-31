#include "Engine/Graphics/Vulkan.hh"

#include "Engine/Graphics/VulkanDevice.hh"

namespace lr {
auto SwapChain::create(Device &device, VkSurfaceKHR surface, vuk::Extent2D extent, [[maybe_unused]] vuk::source_location LOC)
    -> std::expected<SwapChain, vuk::VkException> {
    ZoneScoped;

    VkPresentModeKHR present_mode = device.frame_count() == 1 ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
    vkb::SwapchainBuilder builder(device.handle, surface);
    builder.set_desired_min_image_count(device.frame_count());
    builder.set_desired_extent(extent.width, extent.height);
    builder.set_desired_format({ VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR });
    builder.set_desired_present_mode(present_mode);
    builder.set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    auto result = builder.build();
    if (!result.has_value()) {
        auto error = result.error();
        auto vk_result = result.vk_result();
        LOG_ERROR("Failed to create Swap Chain! {}", error.message());

        return std::unexpected(vk_result);
    }

    auto format = vuk::Format::eB8G8R8A8Srgb;
    auto extent_3d = vuk::Extent3D{ extent.width, extent.height, 1 };

    auto frame_count = device.frame_count();
    auto swapchain_handle = vuk::Swapchain(*device.allocator, frame_count);
    swapchain_handle.surface = surface;
    swapchain_handle.swapchain = result->swapchain;

    auto images = *result->get_images();
    auto image_views = *result->get_image_views();

    for (u32 i = 0; i < images.size(); i++) {
        vuk::ImageAttachment attachment = {
            .image = vuk::Image{ images[i], nullptr },
            .image_view = vuk::ImageView{ { 0 }, image_views[i] },
            .extent = extent_3d,
            .format = format,
            .sample_count = vuk::Samples::e1,
            .view_type = vuk::ImageViewType::e2D,
            .components = {},
            .base_level = 0,
            .level_count = 1,
            .base_layer = 0,
            .layer_count = 1,
        };

        swapchain_handle.images.push_back(attachment);
    }

    auto swapchain = SwapChain{};
    swapchain.format_ = format;
    swapchain.extent_ = extent_3d;
    swapchain.surface_ = surface;
    swapchain.handle_.emplace(std::move(swapchain_handle));

    return swapchain;
}

auto SwapChain::destroy() -> void {
    ZoneScoped;

    handle_.reset();
}

auto SwapChain::format() const -> vuk::Format {
    return format_;
}

auto SwapChain::extent() const -> vuk::Extent3D {
    return extent_;
}
}  // namespace lr
