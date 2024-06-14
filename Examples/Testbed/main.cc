#include "Engine/Core/Log.hh"
#include "Engine/Graphics/Common.hh"
#include "Engine/Graphics/Device.hh"
#include "Engine/Graphics/Instance.hh"

#include "Engine/OS/Window.hh"

using namespace lr;
using namespace lr::graphics;

int main(int argc, char *argv[])
{
    ZoneScoped;

    lr::Log::init(argc, argv);

    os::Window window;
    Instance instance;
    Device device;
    SwapChain swap_chain;
    std::array<ImageID, 3> images = {};
    std::array<ImageViewID, 3> image_views = {};

    auto create_swap_chain = [&](u32 width, u32 height) {
        if (swap_chain) {
            device.delete_swap_chains({ &swap_chain, 1 });
            device.delete_image_views(image_views);
            device.delete_images(images);
        }

        device.create_swap_chain(swap_chain, { window.get_surface(instance.m_handle), { width, height } });
        device.get_swapchain_images(swap_chain, images);
        for (u32 i = 0; i < images.size(); i++) {
            ImageViewInfo image_view_info = { .image_id = images[i], .usage_flags = ImageUsage::ColorAttachment | ImageUsage::TransferDst };
            auto [image_view_id, r_image_view] = device.create_image_view(image_view_info);
            image_views[i] = image_view_id;
        }
    };

    instance.init({});
    window.init({ .title = "Testbed", .width = 1280, .height = 780, .flags = os::WindowFlag::Centered });
    device.init({ .instance = &instance.m_handle, .frame_count = 3 });
    create_swap_chain(window.m_width, window.m_height);

    std::array<BufferID, 3> cpu_buffers = {};
    std::array<BufferID, 3> gpu_buffers = {};
    for (BufferID &cpu_buffer : cpu_buffers) {
        cpu_buffer = device.create_buffer({
            .usage_flags = BufferUsage::TransferSrc,
            .flags = MemoryFlag::HostSeqWrite,
            .preference = MemoryPreference::Host,
            .data_size = 256,
        });
    }

    for (BufferID &gpu_buffer : gpu_buffers) {
        gpu_buffer = device.create_buffer({
            .usage_flags = BufferUsage::TransferDst,
            .flags = MemoryFlag::Dedicated,
            .preference = MemoryPreference::Device,
            .data_size = 256,
        });
    }

    while (!window.should_close()) {
        usize frame_index = device.new_frame();
        Semaphore &frame_sema = device.frame_sema();

        auto [acquire_sema, present_sema] = swap_chain.binary_semas(frame_index);
        auto [acq_index, acq_result] = device.acquire_next_image(swap_chain, acquire_sema);
        ImageID image_id = images[acq_index];
        ImageViewID image_view_id = image_views[acq_index];

        BufferID cpu_buffer = cpu_buffers[frame_index];
        BufferID gpu_buffer = gpu_buffers[frame_index];

        CommandQueue &graphics_queue = device.get_queue(CommandType::Graphics);
        CommandQueue &transfer_queue = device.get_queue(CommandType::Transfer);
        Semaphore &transfer_sema = transfer_queue.semaphore();

        {
            CommandList &cmd_list = transfer_queue.begin_command_list();
            BufferCopyRegion buffer_copy_region = {
                .size = 4,
            };
            cmd_list.copy_buffer_to_buffer(cpu_buffer, gpu_buffer, { &buffer_copy_region, 1 });

            transfer_queue.end_command_list(cmd_list);
            transfer_queue.submit({ .self_wait = false });
        }

        {
            CommandList &cmd_list = graphics_queue.begin_command_list();
            cmd_list.image_transition({
                .src_access = PipelineAccess::TopOfPipe,
                .dst_access = PipelineAccess::ColorAttachmentReadWrite,
                .old_layout = ImageLayout::Undefined,
                .new_layout = ImageLayout::ColorAttachment,
                .image_id = image_id,
            });

            RenderingBeginInfo render_info = {
                .render_area = { { 0, 0 }, { swap_chain.m_extent.width, swap_chain.m_extent.height } },
                .color_attachments = { { {
                    .image_view_id = image_view_id,
                    .image_layout = ImageLayout::ColorAttachment,
                    .load_op = AttachmentLoadOp::Clear,
                    .store_op = AttachmentStoreOp::Store,
                    .clear_value = { ColorClearValue(0.0f, 0.0f, 0.0f, 1.0f) },
                } } },
            };
            cmd_list.begin_rendering(render_info);
            cmd_list.end_rendering();

            cmd_list.image_transition({
                .src_access = PipelineAccess::GraphicsReadWrite,
                .dst_access = PipelineAccess::BottomOfPipe,
                .old_layout = ImageLayout::ColorAttachment,
                .new_layout = ImageLayout::Present,
                .image_id = image_id,
            });
            graphics_queue.end_command_list(cmd_list);
        }

        graphics_queue.submit({
            .additional_wait_semas = { {
                { acquire_sema, 0, PipelineStage::TopOfPipe },
                { transfer_sema, transfer_sema.counter(), PipelineStage::AllCommands },
            } },
            .additional_signal_semas = { {
                { present_sema, 0, PipelineStage::BottomOfPipe },
                { frame_sema, frame_sema.counter() + 1, PipelineStage::AllCommands },
            } },
        });

        graphics_queue.present(swap_chain, present_sema, acq_index);

        device.end_frame();
        window.poll();

        FrameMark;
    }

    return 0;
}
