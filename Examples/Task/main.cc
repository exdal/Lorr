#include "Engine/Graphics/Device.hh"
#include "Engine/Graphics/Instance.hh"

#include "Engine/Graphics/Task/BuiltinTasks.hh"
#include "Engine/Graphics/Task/TaskGraph.hh"

#include "Engine/OS/Window.hh"

using namespace lr;
using namespace lr::graphics;

namespace lr::embedded::shaders {
constexpr static _data_t example_compute[] = {
#include <imgui.slang.h>
};
constexpr static std::span example_compute_sp = { &example_compute->as_u8, count_of(example_compute) };
constexpr static std::string_view example_compute_str = { &example_compute->as_c8, count_of(example_compute) };
}  // namespace lr::embedded::shaders

struct ComputeTask {
    std::string_view name = "Compute";

    struct Uses {
        Preset::ComputeWrite storage_image = {};
    } uses = {};

    struct PushConstants {
        glm::vec2 image_size = {};
        ImageViewID image_view_id = ImageViewID::Invalid;
        f32 time = 0.0f;
    } push_constants = {};

    bool prepare(TaskPrepareInfo &info)
    {
        auto &pipeline_info = info.pipeline_info;

        VirtualFileInfo virtual_files[] = { { "lorr", embedded::shaders::lorr_sp } };
        VirtualDir virtual_dir = { virtual_files };
        ShaderCompileInfo shader_compile_info = {
            .real_path = "example_compute.slang",
            .code = embedded::shaders::example_compute_str,
            .virtual_env = { { virtual_dir } },
        };

        shader_compile_info.entry_point = "cs_main";
        auto [vs_ir, cs_result] = ShaderCompiler::compile(shader_compile_info);
        if (!cs_result) {
            return false;
        }

        pipeline_info.set_shader(info.device->create_shader(ShaderStageFlag::Compute, vs_ir));

        return true;
    }

    void execute(TaskContext &tc)
    {
        auto &cmd_list = tc.command_list();
        auto &storage_image_data = tc.task_image_data(uses.storage_image);
        Image *storage_image = tc.device()->get_image(storage_image_data.image_id);
        auto &io = ImGui::GetIO();

        push_constants.image_size = { storage_image->m_extent.width, storage_image->m_extent.height };
        push_constants.image_view_id = storage_image_data.image_view_id;
        push_constants.time = static_cast<f32>(glfwGetTime());
        tc.set_push_constants(push_constants);

        cmd_list.dispatch(storage_image->m_extent.width / 8 + 1, storage_image->m_extent.height / 8 + 1, 1);
    }
};

struct BlitTask {
    std::string_view name = "Blit";

    struct Uses {
        Preset::TransferRead blit_source = {};
        Preset::TransferWrite blit_target = {};
    } uses = {};

    void execute(TaskContext &tc)
    {
        auto &cmd_list = tc.command_list();
        auto &blit_source_data = tc.task_image_data(uses.blit_source);
        auto &blit_target_data = tc.task_image_data(uses.blit_target);
        Image *blit_source = tc.device()->get_image(blit_source_data.image_id);
        Offset3D src_offset = { static_cast<i32>(blit_source->m_extent.width),
                                static_cast<i32>(blit_source->m_extent.height),
                                static_cast<i32>(blit_source->m_extent.depth) };

        ImageBlit blit = { .src_offsets = { { 0, 0, 0 }, src_offset }, .dst_offsets = { { 0, 0, 0 }, src_offset } };
        cmd_list.blit_image(blit_source_data.image_id, ImageLayout::TransferSrc, blit_target_data.image_id, ImageLayout::TransferDst, Filtering::Nearest, { &blit, 1 });
    }
};

void poll_events(os::Window &window)
{
    auto &io = ImGui::GetIO();

    while (window.m_event_manager.peek()) {
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
                // io.AddKeyEvent(example::ImGuiBackend::lr_key_to_imgui(event_data.key), event_data.key_state != KeyState::Up);
                break;
            }
            default:
                break;
        }
    }
}

i32 main(i32 argc, c8 **argv)
{
    ZoneScoped;

    Log::init(argc, argv);

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
    device.init({ .instance = &instance.m_handle, .frame_count = 3 });
    create_swap_chain(window.m_width, window.m_height);

    ImageID storage_image_id = device.create_image({
        .usage_flags = ImageUsage::Storage | ImageUsage::TransferSrc,
        .format = Format::R8G8B8A8_UNORM,
        .type = ImageType::View2D,
        .extent = { 1280, 780, 1 },
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

    task_graph.add_task<ComputeTask>({ .uses = { .storage_image = task_storage_image } });
    task_graph.add_task<BlitTask>({ .uses = { .blit_source = task_storage_image, .blit_target = swap_chain_image } });
    task_graph.add_task<BuiltinTask::ImGuiTask>({ .uses = { .attachment = swap_chain_image } });
    task_graph.present(swap_chain_image);

    while (!window.should_close()) {
        device.end_frame();

        window.poll();
        poll_events(window);

        FrameMark;
    }

    return 1;
}
