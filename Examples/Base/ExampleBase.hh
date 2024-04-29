#pragma once

#include "Graphics/Device.hh"
#include "Graphics/Instance.hh"
#include "Graphics/Task/TaskAccess.hh"

#include "OS/Window.hh"

using namespace lr::graphics;
namespace lr {
struct ExampleBase {
    ExampleBase(i32 argc, char *argv[]) { Log::init(argc, argv); }

    void init()
    {
        if (!m_instance.init({})) {
            return;
        }

        if (!m_window.init({
                .title = "BareBones",
                .width = 1280,
                .height = 780,
                .flags = os::WindowFlag::Centered,
            })) {
            return;
        }

        if (!m_device.init(&m_instance.m_handle)) {
            return;
        }

        if (!ShaderCompiler::init()) {
            return;
        }

        {
            auto [swap_chain, r_swap_chain] = m_device.create_swap_chain({
                .surface = m_window.get_surface(m_instance.m_handle),
                .extent = { m_window.m_width, m_window.m_height },
            });

            m_swap_chain = std::move(swap_chain);
            if (!r_swap_chain) {
                return;
            }
        }

        auto [images, r_images] = m_device.get_swapchain_images(*m_swap_chain);
        m_images = std::move(images);
        m_image_views = Unique<std::array<ImageViewID, 3>>(&m_device);
        for (u32 i = 0; i < m_images.size(); i++) {
            ImageViewInfo image_view_info = { .image_id = m_images[i] };
            auto [image_view_id, r_image_view] = m_device.create_image_view(image_view_info);
            m_image_views->at(i) = image_view_id;
        }

        CommandAllocatorInfo command_allocator_info = {
            .type = CommandType::Graphics,
        };
        m_command_allocators = Unique<std::array<CommandAllocator, 3>>(&m_device);
        m_device.create_command_allocators(*m_command_allocators, command_allocator_info);

        while (!m_window.should_close()) {
            m_window.poll();

            auto [acquire_sema, present_sema] = m_swap_chain->binary_semas();
            auto [image_id, acq_result] = m_device.acquire_next_image(*m_swap_chain, acquire_sema);

            ImageID current_image = m_images.at(image_id);
            CommandAllocator &command_allocator = m_command_allocators->at(image_id);
            m_device.reset_command_allocator(command_allocator);

            update(command_allocator, current_image, m_image_views->at(image_id));

            m_device.present(*m_swap_chain, present_sema, image_id);
            m_device.collect_garbage();
        }
    }

    virtual void update(CommandAllocator &command_allocator, ImageID image_id, ImageViewID image_view_id) = 0;

    ImageBarrier make_barrier(
        ImageID image_id,
        ImageLayout src_layout,
        const TaskAccess::Access &src_access,
        ImageLayout dst_layout,
        const TaskAccess::Access &dst_access)
    {
        return ImageBarrier{
            .src_stage_mask = src_access.stage,
            .src_access_mask = src_access.access,
            .dst_stage_mask = dst_access.stage,
            .dst_access_mask = dst_access.access,
            .old_layout = src_layout,
            .new_layout = dst_layout,
            .image_id = image_id,
        };
    }

    void submit(
        CommandType command_type,
        std::vector<SemaphoreSubmitInfo> &wait_semas,
        std::vector<CommandListSubmitInfo> &command_lists,
        std::vector<SemaphoreSubmitInfo> &signal_semas,
        bool present)
    {
        ZoneScoped;

        if (present) {
            auto [acquire_sema, present_sema] = m_swap_chain->binary_semas();
            Semaphore &frame_sema = m_swap_chain->frame_sema();

            wait_semas.emplace_back(acquire_sema, 0, PipelineStage::TopOfPipe);
            signal_semas.emplace_back(present_sema, 0, PipelineStage::AllCommands);
            signal_semas.emplace_back(frame_sema, frame_sema.advance(), PipelineStage::AllCommands);
        }

        Semaphore &garbage_sema = m_device.get_queue(CommandType::Graphics).semaphore();
        signal_semas.emplace_back(garbage_sema, garbage_sema.advance(), PipelineStage::AllCommands);

        QueueSubmitInfo submit_info = {
            .wait_sema_count = static_cast<u32>(wait_semas.size()),
            .wait_sema_infos = wait_semas.data(),
            .command_list_count = static_cast<u32>(command_lists.size()),
            .command_list_infos = command_lists.data(),
            .signal_sema_count = static_cast<u32>(signal_semas.size()),
            .signal_sema_infos = signal_semas.data(),
        };
        m_device.submit(command_type, submit_info);
    }

    Instance m_instance = {};
    os::Window m_window = {};
    Device m_device = {};
    Unique<SwapChain> m_swap_chain;

    ls::static_vector<ImageID, 3> m_images;
    Unique<std::array<ImageViewID, 3>> m_image_views;
    Unique<std::array<CommandAllocator, 3>> m_command_allocators;
};
}  // namespace lr
