#include "Core/Log.hh"
#include "Graphics/Common.hh"
#include "Graphics/Device.hh"
#include "Graphics/Instance.hh"

#include "Memory/Stack.hh"

#include "OS/Window.hh"

#include "ExampleBase.hh"
#include "ImGuiBackend.hh"

using namespace lr;
using namespace lr::graphics;

struct PushConstant {
    ImageViewID image_view_id;
    f32 time;
};

struct App {
    ShaderID shader_cs;
    PipelineID compute_pipeline;
    ImageID storage_image;
    ImageViewID storage_image_view;
} app;

bool load_shaders(Device &device)
{
    ZoneScoped;

    ShaderPreprocessorMacroInfo macros[] = {
        { "LR_EDITOR", "1" },
#if LR_DEBUG
        { "LR_DEBUG", "1" },
#endif
    };

    ShaderCompileInfo info = {
        .virtual_env = { &example::default_vdir(), 1 },
        .definitions = macros,
    };

    {
        std::string_view path = COMPUTE_SHADER_PATH;
        auto [code, result] = example::load_file(path);
        if (!result) {
            LR_LOG_ERROR("Failed to open file '{}'!", path);
            return false;
        }
        info.code = code;
        info.real_path = path;
        info.entry_point = "cs_main";
        info.compile_flags = ShaderCompileFlag::SkipOptimization | ShaderCompileFlag::GenerateDebugInfo;
        auto [cs_data, cs_result] = ShaderCompiler::compile(info);

        app.shader_cs = device.create_shader(ShaderStageFlag::Compute, cs_data);
    }

    return true;
}

i32 main(i32 argc, c8 **argv)
{
    lr::Log::init(argc, argv);

    ShaderCompiler::init();

    os::Window window;
    Instance instance;
    Device device;
    SwapChain swap_chain;
    example::ImGuiBackend imgui = {};
    std::array<ImageID, 3> images = {};
    std::array<ImageViewID, 3> image_views = {};
    std::array<CommandAllocator, 3> graphics_callocators = {};
    std::array<CommandAllocator, 3> compute_callocators = {};

    auto create_swap_chain = [&](u32 width, u32 height) {
        if (swap_chain) {
            device.delete_swap_chains({ &swap_chain, 1 });
            device.delete_image_views(image_views);
            device.delete_images(images);
        }

        device.create_swap_chain(swap_chain, { window.get_surface(instance.m_handle), { width, height } });
        device.get_swapchain_images(swap_chain, images);
        for (u32 i = 0; i < images.size(); i++) {
            ImageViewInfo image_view_info = { .image_id = images[i] };
            auto [image_view_id, r_image_view] = device.create_image_view(image_view_info);
            image_views[i] = image_view_id;
        }
    };

    instance.init({});
    window.init({ .title = "Compute", .width = 1280, .height = 780, .flags = os::WindowFlag::Centered });
    device.init(&instance.m_handle);
    create_swap_chain(window.m_width, window.m_height);
    device.create_command_allocators(graphics_callocators, { CommandType::Graphics });
    device.create_command_allocators(compute_callocators, { CommandType::Compute });

    Semaphore &frame_sema = swap_chain.frame_sema();
    Semaphore &garbage_collector_sema = device.m_garbage_timeline_sema;
    CommandQueue &graphics_queue = device.get_queue(CommandType::Graphics);
    CommandQueue &compute_queue = device.get_queue(CommandType::Compute);

    imgui.init(device, swap_chain);
    load_shaders(device);

    app.compute_pipeline = device.create_compute_pipeline({
        .shader_id = app.shader_cs,
        .layout = device.get_layout<PushConstant>(),
    });

    app.storage_image = device.create_image({
        .usage_flags = ImageUsage::Storage | ImageUsage::TransferSrc,
        .format = Format::R32G32B32A32_SFLOAT,
        .type = ImageType::View2D,
        .extent = { 128, 128, 1 },
        .debug_name = "Compute image",
    });

    app.storage_image_view = device.create_image_view({
        .image_id = app.storage_image,
        .usage_flags = ImageUsage::Storage | ImageUsage::TransferSrc,
        .type = ImageViewType::View2D,
        .debug_name = "Compute image view",
    });

    {
        CommandList command_list;
        device.create_command_lists({ &command_list, 1 }, graphics_callocators[0]);
        device.begin_command_list(command_list);
        command_list.image_transition({
            .src_access = PipelineAccess::None,
            .dst_access = PipelineAccess::TransferRead,
            .old_layout = ImageLayout::Undefined,
            .new_layout = ImageLayout::TransferSrc,
            .image_id = app.storage_image,
        });

        device.end_command_list(command_list);
        device.defer({ { command_list } });

        CommandListSubmitInfo command_list_info = { command_list };
        QueueSubmitInfo submit_info = {
            .command_list_count = 1,
            .command_list_infos = &command_list_info,
        };
        graphics_queue.submit(submit_info);

        device.wait_for_work();
    }

    while (!window.should_close()) {
        memory::ScopedStack stack;

        auto [acquire_sema, present_sema] = swap_chain.binary_semas();
        auto [acq_index, acq_result] = device.acquire_next_image(swap_chain, acquire_sema);

        ImageID image_id = images[acq_index];
        ImageViewID image_view_id = image_views[acq_index];
        CommandAllocator &graphics_callocator = graphics_callocators[acq_index];
        CommandAllocator &compute_callocator = compute_callocators[acq_index];
        device.reset_command_allocator(graphics_callocator);
        device.reset_command_allocator(compute_callocator);

        // COMPUTE OP //
        {
            CommandList command_list;
            device.create_command_lists({ &command_list, 1 }, compute_callocator);
            device.begin_command_list(command_list);

            command_list.image_transition({
                .src_access = PipelineAccess::TransferRead,
                .dst_access = PipelineAccess::ComputeWrite,
                .old_layout = ImageLayout::TransferSrc,
                .new_layout = ImageLayout::General,
                .image_id = app.storage_image,
            });

            PushConstant pc = {
                .image_view_id = app.storage_image_view,
                .time = f32(glfwGetTime()),
            };
            PipelineLayout &pipeline_layout = *device.get_layout<PushConstant>();
            command_list.set_pipeline(app.compute_pipeline);
            command_list.set_push_constants(pipeline_layout, &pc, sizeof(PushConstant), 0);
            command_list.set_descriptor_sets(pipeline_layout, graphics::PipelineBindPoint::Compute, 0, { &device.m_descriptor_set, 1 });
            command_list.dispatch(129, 129, 1);

            command_list.image_transition({
                .src_access = PipelineAccess::ComputeWrite,
                .dst_access = PipelineAccess::TransferRead,
                .old_layout = ImageLayout::General,
                .new_layout = ImageLayout::TransferSrc,
                .image_id = app.storage_image,
            });

            device.end_command_list(command_list);
            device.defer({ { command_list } });

            CommandListSubmitInfo command_list_info = { command_list };
            SemaphoreSubmitInfo wait_semas[] = {
                { acquire_sema, 0, PipelineStage::TopOfPipe },
                { graphics_queue.semaphore(), graphics_queue.semaphore().counter(), PipelineStage::AllCommands },
                { compute_queue.semaphore(), compute_queue.semaphore().counter(), PipelineStage::AllCommands },
            };
            SemaphoreSubmitInfo signal_semas[] = {
                { compute_queue.semaphore(), compute_queue.semaphore().advance(), PipelineStage::AllCommands },
            };

            QueueSubmitInfo submit_info = {
                .wait_sema_count = static_cast<u32>(count_of(wait_semas)),
                .wait_sema_infos = wait_semas,
                .command_list_count = static_cast<u32>(1),
                .command_list_infos = &command_list_info,
                .signal_sema_count = static_cast<u32>(count_of(signal_semas)),
                .signal_sema_infos = signal_semas,
            };
            compute_queue.submit(submit_info);
        }

        // GRAPHICS OP //
        {
            CommandList command_list;
            device.create_command_lists({ &command_list, 1 }, graphics_callocator);
            device.begin_command_list(command_list);

            command_list.image_transition({
                .src_access = PipelineAccess::TopOfPipe,
                .dst_access = PipelineAccess::TransferWrite,
                .old_layout = ImageLayout::Undefined,
                .new_layout = ImageLayout::TransferDst,
                .image_id = image_id,
            });

            ImageBlit blit = { 
                .src_offsets = {
                    { 0, 0, 0 },
                    { 128, 128, 1 },
                }, 
                .dst_offsets = {
                    { 0, 0, 0 },
                    { 128, 128, 1 },
                },
            };
            command_list.blit_image(app.storage_image, ImageLayout::TransferSrc, image_id, ImageLayout::TransferDst, Filtering::Nearest, { &blit, 1 });

            command_list.image_transition({
                .src_access = PipelineAccess::TransferWrite,
                .dst_access = PipelineAccess::BottomOfPipe,
                .old_layout = ImageLayout::TransferDst,
                .new_layout = ImageLayout::Present,
                .image_id = image_id,
            });

            device.end_command_list(command_list);
            device.defer({ { command_list } });

            CommandListSubmitInfo command_list_info = { command_list };
            SemaphoreSubmitInfo wait_semas[] = {
                { compute_queue.semaphore(), compute_queue.semaphore().counter(), PipelineStage::AllCommands },
            };

            SemaphoreSubmitInfo signal_semas[] = {
                { present_sema, 0, PipelineStage::BottomOfPipe },
                { frame_sema, frame_sema.advance(), PipelineStage::AllCommands },
                { graphics_queue.semaphore(), graphics_queue.semaphore().advance(), PipelineStage::AllCommands },
                { garbage_collector_sema, garbage_collector_sema.advance(), PipelineStage::AllCommands },
            };

            QueueSubmitInfo submit_info = {
                .wait_sema_count = static_cast<u32>(count_of(wait_semas)),
                .wait_sema_infos = wait_semas,
                .command_list_count = static_cast<u32>(1),
                .command_list_infos = &command_list_info,
                .signal_sema_count = static_cast<u32>(count_of(signal_semas)),
                .signal_sema_infos = signal_semas,
            };
            graphics_queue.submit(submit_info);
        }

        graphics_queue.present(swap_chain, present_sema, acq_index);
        device.collect_garbage();

        window.poll();
    }
}
