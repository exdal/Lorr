#include "Engine/Graphics/Vulkan/Impl.hh"

namespace lr {
auto Surface::create(Device_H device, void *handle) -> std::expected<Surface, vk::Result> {
    auto result = vk::get_vk_surface(device->instance, handle);
    if (!result.has_value()) {
        return std::unexpected(result.error());
    }

    auto impl = new Impl;
    impl->handle = result.value();

    return Surface(impl);
}

auto SwapChain::create(Device_H device, Surface_H surface, vk::Extent2D extent) -> std::expected<SwapChain, vk::Result> {
    ZoneScoped;

    VkPresentModeKHR present_mode = device->frame_count == 1 ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
    vkb::SwapchainBuilder builder(device->handle, surface->handle);
    builder.set_desired_min_image_count(device->frame_count);
    builder.set_desired_extent(extent.width, extent.height);
    builder.set_desired_format({ VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR });
    builder.set_desired_present_mode(present_mode);
    builder.set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    auto result = builder.build();
    if (!result.has_value()) {
        auto error = result.error();
        auto vk_result = result.vk_result();
        LOG_ERROR("Failed to create Swap Chain! {}", error.message());

        switch (vk_result) {
            case VK_ERROR_OUT_OF_HOST_MEMORY:
                return std::unexpected(vk::Result::OutOfHostMem);
            case VK_ERROR_OUT_OF_DEVICE_MEMORY:
                return std::unexpected(vk::Result::OutOfDeviceMem);
            case VK_ERROR_DEVICE_LOST:
                return std::unexpected(vk::Result::DeviceLost);
            case VK_ERROR_SURFACE_LOST_KHR:
                return std::unexpected(vk::Result::SurfaceLost);
            case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
                return std::unexpected(vk::Result::WindowInUse);
            case VK_ERROR_INITIALIZATION_FAILED:
                return std::unexpected(vk::Result::InitFailed);
            default:;
        }
    }

    auto impl = new Impl;
    impl->device = device;
    impl->format = vk::Format::B8G8R8A8_SRGB;
    impl->extent = { extent.width, extent.height, 1 };
    impl->surface = surface;
    impl->handle = result.value();
    for (u32 i = 0; i < device->frame_count; i++) {
        impl->acquire_semas.emplace_back(Semaphore::create(device, ls::nullopt).value());
        impl->present_semas.emplace_back(Semaphore::create(device, ls::nullopt).value());
    }

    return SwapChain(impl);
}

auto SwapChain::destroy() -> void {
}

auto SwapChain::set_name(const std::string &name) -> SwapChain & {
    set_object_name(impl->device, impl->handle.swapchain, VK_OBJECT_TYPE_SWAPCHAIN_KHR, name);
    for (u32 i = 0; i < impl->device->frame_count; i++) {
        impl->acquire_semas[i].set_name(std::format("{} Acquire Sema {}", name, i));
        impl->present_semas[i].set_name(std::format("{} Present Sema {}", name, i));
    }

    return *this;
}

auto SwapChain::format() const -> vk::Format {
    return impl->format;
}

auto SwapChain::extent() const -> vk::Extent3D {
    return impl->extent;
}

auto SwapChain::semaphores(usize i) const -> std::pair<Semaphore, Semaphore> {
    return { impl->acquire_semas.at(i), impl->present_semas.at(i) };
}

auto SwapChain::get_images() const -> std::vector<ImageID> {
    ZoneScoped;

    u32 image_count = impl->device->frame_count;
    std::vector<VkImage> image_handles(image_count);
    auto result = vkGetSwapchainImagesKHR(impl->device->handle, impl->handle, &image_count, image_handles.data());
    if (result != VK_SUCCESS) {
        return {};
    }

    std::vector<ImageID> images(image_count);

    for (u32 i = 0; i < image_count; i++) {
        auto image_id = Image::create_for_swap_chain(impl->device, impl->format, impl->extent).value();
        auto image = impl->device.image(image_id);
        image->handle = image_handles[i];
        image->default_view =
            ImageView::create(
                impl->device,
                image_id,
                vk::ImageViewType::View2D,
                { .aspect_flags = vk::ImageAspectFlag::Color, .base_mip = 0, .mip_count = 1, .base_slice = 0, .slice_count = 1 })
                .value_or(ImageViewID::Invalid);

        images[i] = image_id;

        set_object_name(impl->device, impl->device.image(image_id)->handle, VK_OBJECT_TYPE_IMAGE, std::format("SwapChain Image {}", i));
        set_object_name(
            impl->device,
            impl->device.image_view(image->default_view)->handle,
            VK_OBJECT_TYPE_IMAGE_VIEW,
            std::format("SwapChain Image View {}", i));
    }

    return images;
}

auto SwapChain::image_index() const -> u32 {
    return impl->acquired_index;
}

}  // namespace lr
