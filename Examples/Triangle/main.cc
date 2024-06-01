#include "Engine/Core/Log.hh"
#include "Engine/Graphics/Common.hh"
#include "Engine/Graphics/Device.hh"
#include "Engine/Graphics/Instance.hh"

#include "Engine/Memory/Stack.hh"

#include "Engine/OS/Window.hh"

#include "ImGuiBackend.hh"

using namespace lr;
using namespace lr::graphics;

struct App {
    ShaderID triangle_vs;
    ShaderID triangle_fs;
    PipelineID triangle_pipeline;
} app;

int main(int argc, char *argv[])
{
    ZoneScoped;

    lr::Log::init(argc, argv);

    os::Window window;
    Instance instance;
    Device device;
    SwapChain swap_chain;
    example::ImGuiBackend imgui = {};
    std::array<ImageID, 3> images = {};
    std::array<ImageViewID, 3> image_views = {};
    Unique<std::array<CommandAllocator, 3>> command_allocators(&device);

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
    window.init({ .title = "Triangle", .width = 1280, .height = 780, .flags = os::WindowFlag::Centered });
    device.init(&instance.m_handle);
    create_swap_chain(window.m_width, window.m_height);

    device.create_command_allocators(*command_allocators, { .type = CommandType::Graphics, .debug_name = "Command Allocator" });

    imgui.init(&device, swap_chain);
    {
        std::array shader_ids = { app.triangle_vs, app.triangle_fs };
        Viewport viewport = {};
        Rect2D scissors = {};
        PipelineColorBlendAttachment blend_attachment = {};

        GraphicsPipelineInfo triangle_pipeline_info = {
            .color_attachment_formats = { &swap_chain.m_format, 1 },
            .viewports = { &viewport, 1 },
            .scissors = { &scissors, 1 },
            .shader_ids = shader_ids,
            .blend_attachments = { &blend_attachment, 1 },
            .dynamic_state = DynamicState::Viewport | DynamicState::Scissor,
        };
        app.triangle_pipeline = device.create_graphics_pipeline(triangle_pipeline_info);
    }

    bool swap_chain_invalid = false;
    while (!window.should_close()) {
        memory::ScopedStack stack;
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

        if (swap_chain_invalid) {
            create_swap_chain(window.m_width, window.m_height);
            swap_chain_invalid = false;
        }

        auto [acquire_sema, present_sema] = swap_chain.binary_semas();
        auto [acq_index, acq_result] = device.acquire_next_image(swap_chain, acquire_sema);
        if (acq_result == VKResult::OutOfDate) {
            swap_chain_invalid = true;
            continue;
        }

        Semaphore &frame_sema = swap_chain.frame_sema();
        CommandQueue &queue = device.get_queue(CommandType::Graphics);
        ImageID image_id = images[acq_index];
        ImageViewID image_view_id = image_views[acq_index];
        CommandAllocator &command_allocator = command_allocators->at(acq_index);
        glm::uvec2 render_area = { swap_chain.m_extent.width, swap_chain.m_extent.height };

        device.reset_command_allocator(command_allocator);

        static glm::vec3 clear_color = {};
        imgui.new_frame(swap_chain.m_extent, glfwGetTime());
        {
            auto &io = ImGui::GetIO();
            ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings
                                            | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
            ImGui::SetNextWindowPos({ 5, 5 });
            ImGui::SetNextWindowBgAlpha(0.35f);
            ImGui::Begin("Overlay", nullptr, window_flags);
            ImGui::Text("Frametime: %f", io.Framerate);
            ImGui::End();
            ImGui::ShowDemoWindow();
        }
        imgui.end_frame();

        {
            ls::static_vector<CommandListSubmitInfo, 8> command_list_infos;
            {
                Unique<CommandList> command_list(&device);
                device.create_command_lists({ &*command_list, 1 }, command_allocator);
                command_list.set_name(stack.format("Triangle list = {}", acq_index));
                device.begin_command_list(*command_list);

                command_list->image_transition({
                    .src_access = PipelineAccess::TopOfPipe,
                    .dst_access = PipelineAccess::ColorAttachmentReadWrite,
                    .old_layout = ImageLayout::Undefined,
                    .new_layout = ImageLayout::ColorAttachment,
                    .image_id = image_id,
                });

                RenderingAttachmentInfo attachment = {
                    .image_view_id = image_view_id,
                    .image_layout = ImageLayout::ColorAttachment,
                    .load_op = AttachmentLoadOp::Clear,
                    .store_op = AttachmentStoreOp::Store,
                    .clear_value = { ColorClearValue(clear_color.x, clear_color.y, clear_color.z, 1.0f) },
                };
                RenderingBeginInfo render_info = {
                    .render_area = { 0, 0, render_area.x, render_area.y },
                    .color_attachments = { &attachment, 1 },
                };
                command_list->begin_rendering(render_info);
                command_list->set_pipeline(app.triangle_pipeline);
                command_list->set_viewport(0, { .x = 0, .y = 0, .width = f32(render_area.x), .height = f32(render_area.y), .depth_min = 0.01, .depth_max = 1.0 });
                command_list->set_scissors(0, { 0, 0, render_area.x, render_area.y });
                command_list->draw(3);
                command_list->end_rendering();

                device.end_command_list(*command_list);
                command_list_infos.push_back({ *command_list });
            }

            if (imgui.can_render()) {
                Unique<CommandList> imgui_command_list(&device);
                device.create_command_lists({ &*imgui_command_list, 1 }, command_allocator);
                device.begin_command_list(*imgui_command_list);
                imgui.render(&device, *imgui_command_list, image_view_id);
                device.end_command_list(*imgui_command_list);
                command_list_infos.push_back({ *imgui_command_list });
            }

            {
                Unique<CommandList> command_list(&device);
                device.create_command_lists({ &*command_list, 1 }, command_allocator);
                command_list.set_name(stack.format("End list = {}", acq_index));
                device.begin_command_list(*command_list);
                command_list->image_transition({
                    .src_access = PipelineAccess::GraphicsReadWrite,
                    .dst_access = PipelineAccess::BottomOfPipe,
                    .old_layout = ImageLayout::ColorAttachment,
                    .new_layout = ImageLayout::Present,
                    .image_id = image_id,
                });
                device.end_command_list(*command_list);
                command_list_infos.push_back({ *command_list });
            }

            auto &garbage_collector = device.m_garbage_timeline_sema;
            SemaphoreSubmitInfo wait_semas[] = {
                { acquire_sema, 0, PipelineStage::TopOfPipe },
                { queue.semaphore(), queue.semaphore().counter(), PipelineStage::AllCommands },
            };
            SemaphoreSubmitInfo signal_semas[] = {
                { present_sema, 0, PipelineStage::BottomOfPipe },
                { frame_sema, frame_sema.advance(), PipelineStage::AllCommands },
                { queue.semaphore(), queue.semaphore().advance(), PipelineStage::AllCommands },
                { garbage_collector, garbage_collector.advance(), PipelineStage::AllCommands },
            };

            QueueSubmitInfo submit_info = {
                .wait_sema_count = static_cast<u32>(count_of(wait_semas)),
                .wait_sema_infos = wait_semas,
                .command_list_count = static_cast<u32>(command_list_infos.size()),
                .command_list_infos = command_list_infos.data(),
                .signal_sema_count = static_cast<u32>(count_of(signal_semas)),
                .signal_sema_infos = signal_semas,
            };
            queue.submit(submit_info);
        }

        auto present_result = queue.present(swap_chain, present_sema, acq_index);
        if (present_result == VKResult::OutOfDate) {
            swap_chain_invalid = true;
        }

        device.collect_garbage();
        window.poll();
    }

    return 0;
}
