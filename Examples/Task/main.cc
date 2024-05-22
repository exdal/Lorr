#include "Engine/Graphics/Device.hh"
#include "Engine/Graphics/Instance.hh"
#include "Engine/Graphics/Task/TaskGraph.hh"

#include "Engine/OS/Window.hh"

#include "ExampleBase.hh"
#include "ImGuiBackend.hh"

using namespace lr;
using namespace lr::graphics;
example::ImGuiBackend imgui = {};

struct ImguiTask {
    std::string_view name = "ImGui Task";

    struct Uses {
        Preset::ColorAttachmentWrite attachment = {};
    } uses = {};

    void execute(TaskContext &tc)
    {
        if (!imgui.can_render()) {
            return;
        }

        auto &cmd_list = tc.command_list();
        imgui.render(tc.device(), cmd_list, tc.task_image_data(uses.attachment).image_view_id);
    }
};

struct ComputeTask {
    std::string_view name = "Compute Task";

    struct Uses {
        Preset::ComputeWrite storage_image = {};
    } uses = {};

    struct PushConstants {
        ImageViewID image_view_id = ImageViewID::Invalid;
        f32 time = 0;
    } push_constants = {};

    PipelineID pipeline_id = PipelineID::Invalid;

    void execute(TaskContext &tc)
    {
        auto &cmd_list = tc.command_list();
        auto &storage_image_data = tc.task_image_data(uses.storage_image);
        auto &io = ImGui::GetIO();

        push_constants.image_view_id = storage_image_data.image_view_id;
        push_constants.time = static_cast<f32>(glfwGetTime());
        tc.set_push_constants(push_constants);

        cmd_list.set_pipeline(pipeline_id);
        cmd_list.dispatch(1024 / 8 + 1, 512 / 8 + 1, 1);
    }
};

struct BlitTask {
    std::string_view name = "Blit Task";

    struct Uses {
        Preset::TransferRead blit_source = {};
        Preset::TransferWrite blit_target = {};
    } uses = {};

    void execute(TaskContext &tc)
    {
        auto &cmd_list = tc.command_list();
        auto &blit_source_data = tc.task_image_data(uses.blit_source);
        auto &blit_target_data = tc.task_image_data(uses.blit_target);

        ImageBlit blit = { .src_offsets = { { 0, 0, 0 }, { 1024, 512, 1 } }, .dst_offsets = { { 0, 0, 0 }, { 1024, 512, 1 } } };
        cmd_list.blit_image(blit_source_data.image_id, ImageLayout::TransferSrc, blit_target_data.image_id, ImageLayout::TransferDst, Filtering::Nearest, { &blit, 1 });
    }
};

ShaderID load_shaders(Device &device)
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

    std::string_view path = TASK_COMPUTE_SHADER_PATH;
    auto [code, result] = example::load_file(path);
    if (!result) {
        LR_LOG_ERROR("Failed to open file '{}'!", path);
        return ShaderID::Invalid;
    }
    info.code = code;
    info.real_path = path;
    info.entry_point = "cs_main";
    info.compile_flags = ShaderCompileFlag::SkipOptimization | ShaderCompileFlag::GenerateDebugInfo;
    auto [cs_data, cs_result] = ShaderCompiler::compile(info);

    return device.create_shader(ShaderStageFlag::Compute, cs_data);
}

i32 main(i32 argc, c8 **argv)
{
    Log::init(argc, argv);

    ShaderCompiler::init();
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
            std::string name = fmt::format("SwapChain ImageView {}", i);
            ImageViewInfo image_view_info = {
                .image_id = images[i],
                .usage_flags = ImageUsage::TransferDst,
                .debug_name = name,
            };
            auto [image_view_id, r_image_view] = device.create_image_view(image_view_info);
            image_views[i] = image_view_id;
        }
    };

    instance.init({});
    window.init({ .title = "Task Graph", .width = 1280, .height = 780, .flags = os::WindowFlag::Centered });
    device.init(&instance.m_handle);
    create_swap_chain(window.m_width, window.m_height);
    imgui.init(&device, swap_chain);

    PipelineID compute_pipeline_id = device.create_compute_pipeline({
        .shader_id = load_shaders(device),
        .layout = device.get_layout<ComputeTask::PushConstants>(),
    });

    ImageID storage_image_id = device.create_image({
        .usage_flags = ImageUsage::Storage | ImageUsage::TransferSrc,
        .format = Format::R8G8B8A8_UNORM,
        .type = ImageType::View2D,
        .extent = { 1024, 512, 1 },
        .debug_name = "Compute image",
    });

    ImageViewID storage_image_view_id = device.create_image_view({
        .image_id = storage_image_id,
        .usage_flags = ImageUsage::Storage | ImageUsage::TransferSrc,
        .type = ImageViewType::View2D,
        .debug_name = "Compute image view",
    });

    TaskGraph task_graph;
    task_graph.init({ .device = &device });
    TaskImageID swap_chain_image = task_graph.add_image({ .image_id = images[0], .image_view_id = image_views[0] });
    TaskImageID task_storage_image = task_graph.add_image({ .image_id = storage_image_id, .image_view_id = storage_image_view_id });

    task_graph.add_task<ComputeTask>({ .uses = { .storage_image = task_storage_image }, .pipeline_id = compute_pipeline_id });
    task_graph.add_task<BlitTask>({ .uses = { .blit_source = task_storage_image, .blit_target = swap_chain_image } });
    task_graph.add_task<ImguiTask>({ .uses = { .attachment = swap_chain_image } });
    task_graph.present(swap_chain_image);

    while (!window.should_close()) {
        auto [acquire_sema, present_sema] = swap_chain.binary_semas();
        auto [acquired_index, acq_result] = device.acquire_next_image(swap_chain, acquire_sema);

        while (window.m_event_manager.peek()) {
            auto &io = ImGui::GetIO();

            os::WindowEventData event_data = {};
            switch (window.m_event_manager.dispatch(event_data)) {
                case os::LR_WINDOW_EVENT_MOUSE_POSITION: {
                    io.AddMousePosEvent(event_data.mouse_x, event_data.mouse_y);
                    break;
                }
                case os::LR_WINDOW_EVENT_MOUSE_STATE: {
                    io.AddMouseButtonEvent(event_data.mouse, event_data.mouse_state == KeyState::Down);
                    break;
                }
                case os::LR_WINDOW_EVENT_MOUSE_SCROLL: {
                    io.AddMouseWheelEvent(f32(event_data.mouse_offset_x), f32(event_data.mouse_offset_y));
                    break;
                }
                case os::LR_WINDOW_EVENT_CHAR: {
                    io.AddInputCharacterUTF16(event_data.char_val);
                    break;
                }
                case os::LR_WINDOW_EVENT_KEYBOARD_STATE: {
                    io.AddKeyEvent(example::ImGuiBackend::lr_key_to_imgui(event_data.key), event_data.key_state != KeyState::Up);
                    break;
                }
                default:
                    break;
            }
        }

        ImageID image_id = images[acquired_index];
        ImageViewID image_view_id = image_views[acquired_index];
        Semaphore &frame_sema = swap_chain.frame_sema();
        Semaphore &garbage_collector_sema = device.m_garbage_timeline_sema;
        CommandQueue &present_queue = device.get_queue(CommandType::Graphics);

        imgui.new_frame(static_cast<f32>(swap_chain.m_extent.width), static_cast<f32>(swap_chain.m_extent.height), glfwGetTime());
        {
            task_graph.draw_profiler_ui();
        }
        imgui.end_frame();

        task_graph.set_image(swap_chain_image, { .image_id = image_id, .image_view_id = image_view_id });

        SemaphoreSubmitInfo wait_semas[] = { { acquire_sema, 0, PipelineStage::TopOfPipe } };
        SemaphoreSubmitInfo signal_semas[] = {
            { present_sema, 0, PipelineStage::BottomOfPipe },
            { frame_sema, frame_sema.advance(), PipelineStage::AllCommands },
            { garbage_collector_sema, garbage_collector_sema.advance(), PipelineStage::AllCommands },
        };
        task_graph.execute({ .image_index = acquired_index, .wait_semas = wait_semas, .signal_semas = signal_semas });

        present_queue.present(swap_chain, present_sema, acquired_index);
        device.collect_garbage();

        window.poll();
    }

    return 1;
}
