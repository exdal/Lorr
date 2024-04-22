#include "Graphics/Instance.hh"
#include "Graphics/Device.hh"
#include "OS/Window.hh"

using namespace lr;
using namespace lr::graphics;

int main(int argc, char *argv[])
{
    Logger::Init();
    Instance instance;
    VKResult result = instance.init({});

    os::Window window;
    window.init({
        .title = "BareBones",
        .width = 1280,
        .height = 780,
        .flags = os::WindowFlag::Centered,
    });

    auto [device, result_device] = instance.create_device();
    device.init();
    ShaderCompiler::init();

    auto [swap_chain, result_swap_chain] = device.create_swap_chain({
        .surface = window.get_surface(instance.m_handle),
        .extent = { window.m_width, window.m_height },
    });

    auto [images, result_images] = device.get_swapchain_images(*swap_chain);
    Unique<std::array<CommandAllocator, 3>> command_allocators(&device);
    device.create_command_allocators(
        *command_allocators, { .type = CommandType::Graphics, .flags = CommandAllocatorFlag::Transient });

    while (!window.should_close()) {
        window.poll();

        auto [frame_counter_sema, frame_counter_val] = swap_chain->frame_sema();
        auto [acquire_sema, present_sema] = swap_chain->get_binary_semas();
        auto [image_id, acq_result] = device.acquire_next_image(*swap_chain, acquire_sema);

        ImageID current_image = images.at(image_id);
        CommandAllocator &command_allocator = command_allocators->at(image_id);
        device.reset_command_allocator(command_allocator);

        Unique<CommandList> command_list(&device);
        device.create_command_lists({ &*command_list, 1 }, command_allocator);
        device.begin_command_list(*command_list);

        CommandBatcher batcher(&device, *command_list);
        ImageBarrier barrier = {
            .src_stage_mask = PipelineStage::TopOfPipe,
            .src_access_mask = MemoryAccess::None,
            .dst_stage_mask = PipelineStage::BottomOfPipe,
            .dst_access_mask = MemoryAccess::None,
            .old_layout = ImageLayout::Undefined,
            .new_layout = ImageLayout::Present,
            .image_id = current_image,
            .subresource_range = {},
        };
        batcher.insert_image_barrier(barrier);
        batcher.flush_barriers();
        device.end_command_list(*command_list);

        QueueSubmitInfoDyn submit_info = {};
        submit_info.wait_sema_infos.emplace_back(acquire_sema, 0, PipelineStage::TopOfPipe);
        submit_info.command_lists.emplace_back(*command_list);
        submit_info.signal_sema_infos.emplace_back(present_sema, 0, PipelineStage::AllCommands);
        submit_info.signal_sema_infos.emplace_back(
            frame_counter_sema, frame_counter_val, PipelineStage::AllCommands);

        device.submit(CommandType::Graphics, submit_info);
        device.present(*swap_chain, present_sema, image_id);
        device.collect_garbage();
    }

    return 0;
}
